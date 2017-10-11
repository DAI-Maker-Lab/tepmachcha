//  Tepmachcha version number
#define VERSION "1.1.2"

//  Customize these items for each installation
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
#define SENSOR_ID     "sensor id"

#ifndef SECRETS
#define DMISAPIBEARER "YOUR_BEARER_ID"
#define EWSTOKEN_ID   "EWS_TOKEN"
#define EWSDEVICE_ID  "EWS_DEVICE"
#define FTPPW         "FTP PASSWORD"
#define FTPUSER       "FTP USER"
#define FTPSERVER     "FTP SERVER"
#define FOTAPASSWORD  "FOTA_PASSWORD"  // Password to trigger FOTA update
#define FLASHPASSWORD "FLASH_PASSWORD" // Password to flash app from SD file
#define PINGPASSWORD  "PING_PASSWORD"  // Password to send ping sms
#define BEEPASSWORD   "XBEE_PASSWORD"  // Password to turn on XBee by SMS
#define APN           "FONAapn"
#define KEY1          0x01020304       // Encryption key 128 bits
#define KEY2          0x05060708
#define KEY3          0x090a0b0c
#define KEY4          0x0d0e0f00
#endif


#define SENSOR_HEIGHT  500  //  Height of top of octagonal gasket from streambed, in cm
#define UTCOFFSET        7  //  Local standard time variance from UTC
#define XBEEWINDOWSTART 14  //  Hour to turn on XBee for programming window
#define XBEEWINDOWEND   17  //  Hour to turn off XBee
#define INTERVAL         5  //  Number of minutes between readings
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

#include <DS1337.h>           //  For the Stalker's real-time clock (RTC)
#include <Sleep_n0m1.h>       //  Sleep library
#include <SoftwareSerial.h>   //  For serial communication with FONA
#include <Wire.h>             //  I2C library for communication with RTC
#include <Fat16.h>            //  FAT16 library for SD card
#include <EEPROM.h>           //  EEPROM lib
#include "Adafruit_FONA.h"

// save RAM by reducing hw serial rcv buffer (default 64)
// Note the XBee has a buffer of 100 bytes, but as we don't
// receive much from XBee we can reduce the buffer.
#define SERIAL_BUFFER_SIZE 16

#define RTCINT 0    //  RTC interrupt number

#define RTCPIN   2  //  Onboard Stalker RTC pin
#define FONA_RST 3  //  FONA RST pin
#define SD_POWER 4  //  optional power to SD card
#define BEEPIN   5  //  XBee power pin
#define FONA_RX  6  //  UART pin into FONA
#define PING     7  //  Sonar ping pin
#define RANGE    8  //  Sonar range pin -- pull low to turn off sonar
#define FONA_TX  9  //  UART pin from FONA

#define FONA_RTS A1 //  FONA RTS pin
#define FONA_KEY A2 //  FONA Key pin
#define FONA_PS  A3 //  FONA power status pin
#define SOLAR    A6 //  Solar level
#define BATT     A7 //  Battery level

static const char OK_STRING[] PROGMEM = "OK";
#define OK ((__FlashStringHelper*)OK_STRING)

// call into bootloader jumptable at top of flash
#define write_flash_page (*((void(*)(const uint32_t address))(0x7ffa/2)))
#define flash_firmware (*((void(*)(const char *))(0x7ffc/2)))
#define EEPROM_FILENAME_ADDR (E2END - 1)

boolean sentData = false;
boolean smsPower = false;       //  Manual XBee power flag
boolean noSMS = false;          //  Flag to turn off SMS checking -- for future use
boolean timeReset = false;      //  Flag indicating whether midnight time reset has already occurred
byte beeShutoffHour = 0;        //  Hour to turn off manual power to XBee
byte beeShutoffMinute = 0;      //  Minute to turn off manual power to XBee
char method = 0;                //  Method of clock set, for debugging

//static boolean testmenu = 0;

DateTime now;

SoftwareSerial fonaSerial = SoftwareSerial (FONA_TX, FONA_RX);
Adafruit_FONA fona = Adafruit_FONA (FONA_RST);

DS1337 RTC;         //  Create the DS1337 real-time clock (RTC) object
Sleep sleep;        //  Create the sleep object


static void rtcIRQ (void)
{
		RTC.clearINTStatus();   //  Wake from sleep and clear the RTC interrupt
}


void setup (void)
{
		Wire.begin();         // Begin I2C interface
		RTC.begin();          // Begin RTC
		Serial.begin (57600); // Begin debug serial
    fonaSerial.begin (4800); //  Open a serial interface to FONA

    // Compile-in the date and time; helps identify software uploads
		// Note: C compiler concatenates adjacent strings
		Serial.println (F("Tepmachcha v" VERSION " " __DATE__ " " __TIME__));

		analogReference(DEFAULT); // stalkerv3: DEFAULT=3.3V, INTERNAL=1.1V, EXTERNAL=3.3V
    analogRead(BATT);         // must read once after changing reference

		Serial.print (F("Battery: "));
		Serial.print (batteryRead());
		Serial.println (F("mV"));

    // Set output pins (default is input)
		pinMode (BEEPIN, OUTPUT);
		pinMode (RANGE, OUTPUT);
		pinMode (FONA_KEY, OUTPUT);
		pinMode (FONA_RX, OUTPUT);
		pinMode (SD_POWER, OUTPUT);

    // Set RTC interrupt handler
		attachInterrupt (RTCINT, rtcIRQ, FALLING);
		interrupts();

    digitalWrite (RANGE, LOW);           //  sonar off
		digitalWrite (SD_POWER, LOW);        //  SD card off
		digitalWrite (FONA_KEY, HIGH);       //  Initial state for key pin
    digitalWrite (BEEPIN, LOW);          //  XBee on

		/*  If the voltage at startup is less than 3.5V, we assume the battery died in the field
		 *  and the unit is attempting to restart after the panel charged the battery enough to
		 *  do so. However, running the unit with a low charge is likely to just discharge the
		 *  battery again, and we will never get enough charge to resume operation. So while the
		 *  measured voltage is less than 3.5V, we will put the unit to sleep and wake once per 
		 *  hour to check the charge status.
		 */
		while (batteryRead() < 3500)
		{
				Serial.println (F("Low power sleep"));
				Serial.flush();
				digitalWrite (BEEPIN, HIGH);      //  Make sure XBee is powered off
				digitalWrite (RANGE, LOW);        //  Make sure sonar is off
				RTC.enableInterrupts (EveryHour); //  We'll wake up once an hour
				RTC.clearINTStatus();             //  Clear any outstanding interrupts
				sleep.pwrDownMode();                    //  Set sleep mode to Power Down
				sleep.sleepInterrupt (RTCINT, FALLING); //  Sleep; wake on falling voltage on RTC pin
		}

		// We will use the FONA to get the current time to set the Stalker's RTC
		fonaOn();

    // set ext. audio, to prevent crash on incoming calls
    // https://learn.adafruit.com/adafruit-feather-32u4-fona?view=all#faq-1
    fona.sendCheckReply(F("AT+CHFA=1"), OK);

		clockSet();
		
		// Delete any accumulated SMS messages to avoid interference from old commands
#ifndef DEBUG
    smsDeleteAll();
#endif
		
    fonaOff();

		now = RTC.now();    //  Get the current time from the RTC

		RTC.enableInterrupts (EveryMinute);  //  RTC will interrupt every minute
		RTC.clearINTStatus();                //  Clear any outstanding interrupts
		
		// We'll keep the XBee on for an hour after startup to assist installation
		if (now.hour() == 23) {
				beeShutoffHour = 0;
		} else {
				beeShutoffHour = (now.hour() + 1);
		}
		beeShutoffMinute = now.minute();

		Serial.print (F("XBee powered until "));
		Serial.print (beeShutoffHour);
		Serial.print (F(":"));
		Serial.println (beeShutoffMinute);
		Serial.flush();
		smsPower = true;
}


void loop (void)
{
#ifndef DEBUG
    /*
    if (Serial.read() == '?') testmenu = true;
    if (testmenu) {
      test();
      return;
    }
    */
    //test();
    //return;
#endif

		now = RTC.now();      //  Get the current time from the RTC

		Serial.print (now.hour());
		Serial.print (F(":"));
		Serial.println (now.minute());


		/*  One failure mode of the sonar -- if, for example, it is not getting enough power -- 
	   *	is to return the minimum distance the sonar can detect; in the case of the 10m sonars
	   *	this is 50cm. This is also what would happen if something were to block the unit -- a
	   *	plastic bag that blew onto the enclosure, for example. We very much want to avoid false
	   *	positive alerts, so we will ignore anything less than 55cm from the sensor. 
     *
     */

		if (now.minute() % INTERVAL == 0 && sentData == false)   //  If it is time to send a scheduled reading...
		{
		    int streamHeight = takeReading();
				upload (streamHeight);
		}
		else
		{
				sentData = false;
		}

		//  We will turn on the XBee radio for programming only within a specific
		//  window to save power
		if (now.hour() >= XBEEWINDOWSTART && now.hour() <= XBEEWINDOWEND)
		{
				digitalWrite (BEEPIN, LOW);
		}
		else
		{
				//  If the XBee power was turned on by SMS, we'll check to see if 
				//  it's time to turn it back off
				if(smsPower == true && now.hour() < beeShutoffHour)
				{
				        digitalWrite (BEEPIN, LOW);
				}
				else
				{
				        if(smsPower == true && now.hour() == beeShutoffHour && now.minute() < beeShutoffMinute)
				        {
				                digitalWrite (BEEPIN, LOW);
				        }
				        else
				        {
				                if (smsPower == true && now.hour() == 23 && beeShutoffHour == 0)
				                {
				                        digitalWrite (BEEPIN, LOW);
				                }
				                else
				                {
				                        if (smsPower == true)
				                        {
				                                  Serial.println (F("Turning XBee off.."));
				                                  Serial.flush();
				                                  wait (500);
				                        }
		
				                        digitalWrite (BEEPIN, HIGH);
				                        smsPower = false;
				                }
				        }
				}
		}

		Serial.println(F("sleeping"));
		Serial.flush();                         //  Flush any output before sleep

		sleep.pwrDownMode();                    //  Set sleep mode to "Power Down"
		//sleep.adcMode();
		//sleep.idleMode();
		RTC.clearINTStatus();                   //  Clear any outstanding RTC interrupts
		sleep.sleepInterrupt (RTCINT, FALLING); //  Sleep; wake on falling voltage on RTC pin
}


void upload(int streamHeight)
{
		fonaOn();

    //sentData = dmisPost(takeReading(), solarCharging(), v);
    sentData = ews1294Post(streamHeight);

		if (noSMS == false)
		{
				smsCheck();
		}

		/*   The RTC drifts more than the datasheet says, so we'll reset the time every day at
		 *   midnight. 
		 *
     */
		if (now.hour() == 0 && timeReset == false)
		{
				clockSet();
				timeReset = true;
		}

		if (now.hour() != 0)
		{
				timeReset = false;
		}
		
		fonaOff();
    //sentData = true;
}



#define YEAR   in->year
#define MONTH  in->month
#define DAY    in->day
#define HOUR   in->hour
#define MINUTE in->minute
#define SECOND in->second

void clockSet2 (void)
{
    int16_t values[7];
    struct {
      int16_t year;
      int16_t month;
      int16_t day;
      int16_t hour;
      int16_t minute;
      int16_t second;
    } *in;

		char theDate[17];

		Serial.println (F("Fetching GSM time"));
		wait (1000);    //  Give time for any trailing data to come in from FONA

		fonaFlush();    //  Flush any trailing data
		fona.sendCheckReply (F("AT+CIPGSMLOC=2,1"), OK);    //  Query GSM location service for time

    // Read date elements from fona
		fona.parseInt();                    //  Ignore first int
    for (uint8_t i = 0; i < 7;i++) { values[i] = fona.parseInt(); }

    // Set the window to the second element of the array of values read from fona
    in = (void *)&(values[1]);

    if (YEAR < 2016 || YEAR > 2050 || HOUR > 23) //  If that obviously didn't work...
    {
      // Set the window to the first element
      in = (void *)&(values[0]);

		  if (YEAR < 2016 || YEAR > 2050 || HOUR > 23) //  If that obviously didn't work...
      {

				Serial.println (F("GSM location failed, trying NTP sync"));
				fona.enableNTPTimeSync (true, F("0.daimakerlab.pool.ntp.org"));
				wait (15000);                 // Wait for NTP server response
				
				fona.println (F("AT+CCLK?")); // Query FONA's clock for resulting NTP time              

        // Read date elements from fona
        for (uint8_t i = 0; i < 6;i++) { values[i] = fona.parseInt(); }
      }
    }

    if (YEAR > 2000) YEAR -= 2000;
    if (YEAR >= 16 || YEAR <= 50 || HOUR <= 23) //  If that obviously didn't work...
    {
      //  Adjust UTC to local time
#define LOCALTIME (HOUR + UTCOFFSET)
      if ( LOCALTIME < 0 )       // TZ takes us back a day
      {
        HOUR = LOCALTIME + 24;
        DAY--;
      }
      else if ( LOCALTIME > 23 ) // TZ takes us to next day
      {
        HOUR = LOCALTIME - 24;
        DAY++;
      }
      
      Serial.print (F("Obtained current time: "));
      sprintf_P(theDate, (prog_char*)F("%d/%d/%d %d:%d"), DAY, MONTH, YEAR, HOUR, MINUTE);
      Serial.print (theDate);

      Serial.println(F(". Adjusting RTC"));
      DateTime dt(YEAR, MONTH, DAY, HOUR, MINUTE, SECOND, 0);
      now = dt;
      RTC.adjust(dt);     //  Adjust date-time as defined above
    }
    else
    {
      Serial.println (F("Network time failed, using RTC"));
      method = 'X';
		}

		wait (200);              //  Give FONA a moment to catch its breath
}


void clockSet (void)
{
		char theDate[17];

		Serial.println (F("Fetching GSM time"));
		wait (1000);    //  Give time for any trailing data to come in from FONA
		fonaFlush();    //  Flush any trailing data

		fona.sendCheckReply (F("AT+CIPGSMLOC=2,1"), OK);    //  Query GSM location service for time
		
		fona.parseInt();                    //  Ignore first int
		int secondInt = fona.parseInt();    //  Ignore second int -- necessary on some networks/towers
		int netYear = fona.parseInt();      //  Get the results -- GSMLOC year is 4-digit
		int netMonth = fona.parseInt();
		int netDay = fona.parseInt();
		int netHour = fona.parseInt();      //  GSMLOC is _supposed_ to get UTC; we will adjust
		int netMinute = fona.parseInt();
		int netSecond = fona.parseInt();    //  Our seconds may lag slightly, of course

		if (netYear < 2016 || netYear > 2050 || netHour > 23) //  If that obviously didn't work...
		{
				Serial.println (F("Recombobulating..."));

				netSecond = netMinute;  //  ...shift everything up one to capture that second int
				netMinute = netHour;
				netHour = netDay;
				netDay = netMonth;
				netMonth = netYear;
				netYear = secondInt;
		}

		if (netYear < 2016 || netYear > 2050 || netHour > 23)   // If that still didn't work...
		{
				// get time from the NTP pool instead: 
				// (https://en.wikipedia.org/wiki/Network_Time_Protocol)
        //
				fona.enableNTPTimeSync (true, F("0.daimakerlab.pool.ntp.org"));
				Serial.println (F("GSM location failed, trying NTP sync"));
				
				wait (15000);                 // Wait for NTP server response
				
				fona.println (F("AT+CCLK?")); // Query FONA's clock for resulting NTP time              
				netYear = fona.parseInt();    // Capture the results
				netMonth = fona.parseInt();
				netDay = fona.parseInt();
				netHour = fona.parseInt();    // We asked NTP for UTC and will adjust below
				netMinute = fona.parseInt();
				netSecond = fona.parseInt();  // Our seconds may lag slightly
		
				method = 'N';
		}
		else 
		{
				method = 'G';
		}

    if (netYear > 2000) { netYear -= 2000; }  // Adjust from YYYY to YY
		if (netYear >= 16 && netYear < 50)        // Date looks valid
		{
				//  Adjust UTC to local time
        //#define localtime (netHour + UTCOFFSET)
        int8_t localtime = (netHour + UTCOFFSET);
				if ( localtime < 0)                   // TZ takes us back a day
				{
				    netDay = netDay - 1;                // adjust the date to UTC - 1
				    netHour = localtime + 24;           // hour % 24
				}
				else if (localtime > 23)              // TZ takes to the next day
        {
            netDay = netDay + 1;                // adjust the date to UTC + 1
            netHour = localtime - 24;           // hour % 24
        }
        else                                  // TZ is same day
        {
            netHour = localtime;                // simply add TZ offset
        }

				Serial.print (F("Obtained current time: "));
				sprintf_P(theDate, (prog_char*)F("%d/%d/%d %d:%d"), netDay, netMonth, netYear, netHour, netMinute);
				Serial.print (theDate);

				Serial.println(F(". Adjusting RTC"));
				DateTime dt(netYear, netMonth, netDay, netHour, netMinute, netSecond, 0);
        now = dt;
				RTC.adjust(dt);     //  Adjust date-time as defined above
		}
		else
		{
				Serial.println (F("Network time failed, using RTC"));
				method = 'X';
		}

		wait (200);              //  Give FONA a moment to catch its breath
}


void wait (uint32_t period)
{
  // Non-blocking delay function
  uint32_t waitend = millis() + period;
  while (millis() <= waitend)
  {
    Serial.flush();
  }
}


void XBeeOn (void)
{
}


void XBeeOff (void)
{
}


void fonaFlush (void)
{
  // Read all available serial input from FONA to flush any pending data.
  while(fona.available())
  {
    char c = fona.read();
    loop_until_bit_is_set (UCSR0A, UDRE0); 
    UDR0 = c;
  }
}


char fonaRead(void)
{
  // read from fona with timeout
  uint32_t timeout = millis() + 1000;

  while(!fona.available())
  {
    if (millis() > timeout)
      break;
    else
      delay(1);
  }
  return fona.read();
}


void fonaPowerOn(void)
{
  if (digitalRead (FONA_PS) == LOW)  //  If the FONA is off
  {
    digitalWrite(FONA_KEY, HIGH);    //  KEY should be high to start

    Serial.print (F("FONA poweron .. "));
    while (digitalRead (FONA_PS) == LOW) 
    {
      digitalWrite(FONA_KEY, LOW);   //  pulse the Key pin low
      wait (500);
    }
    digitalWrite (FONA_KEY, HIGH);   //  and then return it to high
    Serial.println(F(" done."));
  }
}


boolean fonaSerialOn(void)
{
    Serial.println (F("Initializing FONA"));

    fonaSerial.begin (4800);                      //  Open a serial interface to FONA

    if (fona.begin (fonaSerial) == false)         //  Start the FONA on serial interface
    {
      Serial.println (F("FONA not found"));
      return false;
    }
  return true;
}


boolean fonaGSMOn(void) {
  uint32_t gsmTimeout = millis() + 30000;

  Serial.print (F("Connecting GSM... "));
  while (millis() < gsmTimeout)
  {
    uint8_t status = fona.getNetworkStatus();
    wait (500);
    if (status == 1 || status == 5)
    {
      Serial.println(F("done."));
      return true;
    }
  }
  Serial.println (F("timed out. Check SIM card, antenna, and signal."));
  return false;
}


boolean fonaGPRSOn(void) {
  fona.setGPRSNetworkSettings (F(APN));  //  Set APN to local carrier
  wait (5000);    //  Give the network a moment

  //  RSSI is a measure of signal strength -- higher is better; less than 10 is worrying
  uint8_t rssi = fona.getRSSI();
  Serial.print (F("RSSI: ")); Serial.println (rssi);

  if (rssi > 5)
  {
    for (uint8_t attempt = 1; attempt < 5; attempt++)
    {
      Serial.print (F("Turning GPRS on, attempt ")); Serial.println(attempt);
      fona.enableGPRS (true);

      if (fona.GPRSstate() == 1)
      {
        Serial.println (F("GPRS is on."));
        return true;
      }
      Serial.println(F("Failed"));
      wait (2500);
    }
  }
  else
  {
    Serial.println (F("Inadequate signal strength"));
  }
  Serial.println(F("Giving up, GPRS Failed"));
  return false;
}


void fonaGPRSOff(void) {
  if (fona.enableGPRS (false) == false)
  {
    if (fona.GPRSstate() == 1)
    {
      Serial.println (F("GPRS Off Failed"));
      return;
    }
  }
  Serial.println (F("GPRS is off."));
}


boolean fonaOn()
{
  fonaPowerOn();
  if (fonaSerialOn())
  {
    Serial.print (F("FONA online. "));
    if ( fonaGSMOn() )
    {
      return fonaGPRSOn();
    }
  }
}


void fonaOff (void)
{
  wait (5000);        // Shorter delays yield unpredictable results
  fonaGPRSOff();      // turn GPRS off first, for an orderly shutdown

  wait (500);

  if (digitalRead (FONA_PS) == HIGH)           //  If the FONA is on
  {
    fona.sendCheckReply (F("AT+CPOWD=1"), OK); //  send shutdown command
    digitalWrite (FONA_KEY, HIGH);             //  and set Key high
  }
}


int takeReading (void)
{
		//  We will take the mode of seven samples to try to filter spurious readings
		int sample[] = {0, 0, 0, 0, 0, 0, 0};   //  Initial sample values
		
    digitalWrite (RANGE, HIGH);           //  sonar on
    wait (1000);

		for (int sampleCount = 0; sampleCount < 7; sampleCount++)
		{
				sample[sampleCount] = pulseIn (PING, HIGH);
				Serial.print (F("Sample "));
				Serial.print (sampleCount);
				Serial.print (F(": "));
				Serial.println (sample[sampleCount]);
				wait (50);
		}

		int sampleMode = mode (sample, 7);

		int streamHeight = (SENSOR_HEIGHT - (sampleMode / 10)); //  1 µs pulse = 1mm distance

		Serial.print (F("Surface distance from sensor is "));
		Serial.print (sampleMode);
		Serial.println (F("mm."));
		Serial.print (F("Calculated surface height is "));
		Serial.print (streamHeight);
		Serial.println (F("cm."));

    digitalWrite (RANGE, LOW);           //  sonar off
		return streamHeight;
}


int mode (int *x, int n)    
/*   Calculate the mode of an array of readings
 *   From http://playground.arduino.cc/Main/MaxSonar
 */   
{
		int i = 0;
		int count = 0;
		int maxCount = 0;
		int mode = 0;
	  
		int bimodal;
		int prevCount = 0;
		while(i < (n - 1))
		{
				prevCount = count;
				count = 0;
				while(x[i] == x[i + 1])
				{
				        count++;
				        i++;
				}

				if(count > prevCount && count > maxCount)
				{
				        mode = x[i];
				        maxCount = count;
				        bimodal = 0;
				}
				
				if(count == 0)
				{
				        i++;
				}
				
				if(count == maxCount)     //  If the dataset has 2 or more modes
				{
				        bimodal = 1;
				}
				
				if(mode == 0 || bimodal == 1)   //  Return the median if no mode
				{
				        mode = x[(n / 2)];
				}
				
				return mode;
		}
}


//  Determine if panel is charging
boolean solarCharging()
{
    return ( analogRead (SOLAR) > 550 && analogRead (SOLAR) <= 900 );
}


boolean ews1294Post (int streamHeight)
{
        uint16_t status_code = 0;
        uint16_t response_length = 0;
        char post_data[128];

        uint16_t voltage;
        fona.getBattVoltage (&voltage);   //  Read the battery voltage from FONA's ADC
        boolean sol = solarCharging();

        DEBUG_RAM

        // Construct the body of the POST request:
        sprintf_P (post_data,
            (prog_char *)F("api_token=" EWSTOKEN_ID "&data={\"sensorId\":\"" EWSDEVICE_ID "\",\"streamHeight\":\"%d\",\"charging\":\"%d\",\"voltage\":\"%d\",\"timestamp\":\"%d-%d-%dT%d:%d:%d.000Z\"}\r\n"),
              streamHeight,
              sol,
              voltage,
              now.year(), now.month(), now.date(), now.hour(), now.minute(), now.second()
        );

        Serial.println (F("data:"));
        Serial.println (post_data);

        //  ews1294.info does not currently support SSL; if it is added you will need to uncomment the following
        //fona.sendCheckReply (F("AT+HTTPSSL=1"), F("OK"));   //  Turn on SSL
        //fona.sendCheckReply (F("AT+HTTPPARA=\"REDIR\",\"1\""), F("OK"));  //  Turn on redirects (for SSL)

        // Send the POST request we have constructed
        if (fona.HTTP_POST_start ("ews1294.info/api/v1/sensorapi", F("application/x-www-form-urlencoded"), post_data, strlen(post_data), &status_code, &response_length)) {
          // flush response
          while (response_length > 0)
          {
             fonaFlush();
             response_length--;
          }
        }

        fona.HTTP_POST_end();

        if (status_code == 200)
        {
            Serial.println (F("POST succeeded."));
            return true;
        }
        else
        {
            Serial.print (F("POST failed. Status-code: "));
            Serial.println (status_code);
            return false;
        }
}


void smsDeleteAll(void)
{
  fona.sendCheckReply (F("AT+CMGF=1"), OK);            //  Enter text mode
  fona.sendCheckReply (F("AT+CMGDA=\"DEL ALL\""), OK); //  Delete all SMS messages                }
}


// returns -1 for error
int8_t smsCount (void)
{
		/*   During testing on metfone, sometimes the FONA had to be on for more
		 *   than 60 seconds(!) before it would receive a previously sent SMS message,
		 *   so we'll keep looking for 60 seconds.
		 */
		uint32_t smsTimeout = millis() + 60000;
    int8_t NumSMS; // fona.getNumSMS returns -1 on failure
    DEBUG_RAM

		Serial.println (F("Checking for SMS messages..."));

		do {
				NumSMS = fona.getNumSMS();
				wait (5000);
		} while (NumSMS == 0 && millis() <= smsTimeout);

		Serial.print (NumSMS);
		Serial.println (F(" message(s) waiting."));
    return NumSMS;
}


char *parseFilename(char *b)
{
    uint8_t i;
    while (*b == ' ') b++;     // skip spaces

    // read filename
    for ( i = 0; i < 12 && b[i] && b[i] != ' ' ; i++) {
      file_name[i] = b[i];
    }
    file_name[i] = 0;
    return b+i;
}


#define SMS_MAX_LEN 80
#define SMS_SENDER_MAX_LEN 18
//  Check SMS messages received for any valid commands
void smsCheck (void)
{
		char smsBuffer[SMS_MAX_LEN];
		char smsSender[SMS_SENDER_MAX_LEN];
		unsigned int smsLen;
		boolean sendStatus = false;
		int8_t NumSMS;
		uint32_t timeOut = (millis() + 60000);

		fonaFlush();    //  Flush out any unresolved data
		
    NumSMS =  smsCount();

    // For each SMS message
		while (NumSMS > 0)
		{
				fona.readSMS (NumSMS, smsBuffer, SMS_MAX_LEN, &smsLen);  // retrieve the most recent one
				wait (500);                                              // required delay

				fona.getSMSSender (NumSMS, smsSender, SMS_SENDER_MAX_LEN);  // get sender
				wait (500);

				Serial.print (F("Message from "));
				Serial.print (smsSender);
				Serial.println (F(":"));
				Serial.println (smsBuffer);

        // FOTAPASSWD <filename> <filesize>
				if (strncmp_P(smsBuffer, (prog_char*)F(FOTAPASSWORD), sizeof(FOTAPASSWORD)-1) == 0) //  FOTA password...
        {

            // read filename, size, cksum
            Serial.println(F("Received FOTA request"));

            char *b = smsBuffer + sizeof(FOTAPASSWORD);
            b = parseFilename( smsBuffer + sizeof(FOTAPASSWORD) );

            while (*b == ' ') b++;     // skip spaces

            // read file size
            file_size = 0;
            while (*b >= '0' && *b <= '9') {
              file_size = (file_size * 10) + (*b - '0');
              b++;
            }

            Serial.print(F("filename:")); Serial.println(file_name);
            Serial.print(F("size:")); Serial.println(file_size);

            uint8_t status = firmwareGet();
            sprintf_P(smsBuffer, (prog_char *)F("fwGet %s (%d) status: %d, crc: %d"), \
              file_name, file_size, status, status);
            fona.sendSMS(smsSender, smsBuffer);  // return file stat, statuss
        }

        // FLASHPASSWD <filename>
				if (strncmp_P(smsBuffer, (prog_char*)F(FLASHPASSWORD), sizeof(FLASHPASSWORD)-1) == 0) //  FOTA password...
        {
            parseFilename( smsBuffer + sizeof(FLASHPASSWORD) );
            eepromWrite();
            reflash();
        }

        // BEEPASSWORD
				if (strcmp_P (smsBuffer, (prog_char*)F(BEEPASSWORD)) == 0)        //  XBee password...
				{
            //  ...determine the appropriate shutoff time and turn on the XBee until then
            beeShutoffHour = (now.hour() + 1);   // We'll leave the XBee on for 1 hour
            if (beeShutoffHour == 24)
            beeShutoffHour = 0;
            
            digitalWrite (BEEPIN, LOW);          // Turn on the XBee

            // Compose a reply to the sender confirming the action and giving the shutoff time
            sprintf_P(smsBuffer, (prog_char *)F("XBee on until %02d:%02d"), beeShutoffHour, beeShutoffMinute);

            smsPower = true;                     //  Raise the flag 
            fona.sendSMS(smsSender, smsBuffer);  //  Tell the sender what you've done

            Serial.println (F("XBee turned on by SMS."));
				}

				wait (1000);
				fona.deleteSMS (NumSMS);
				wait (1500);
				NumSMS = fona.getNumSMS();

        Serial.print(F("# SMS: ")); Serial.println(NumSMS);

				// Occasionally messages won't delete and this loops forever. If
				// the process takes too long we'll just nuke everything.
				if (millis() >= timeOut)
				{
          smsDeleteAll();
				}
		}
}


/* Get battery reading on ADC pin BATT, in mV
 *
 * VBAT is divided by a 10k/2k voltage divider (ie /6) to BATT, which is then
 * measured relative to AREF, on a scale of 0-1023, so
 *
 *   mV = BATT * (AREF * ( (10+2)/2 ) / 1.023)
 *
 * We use integer math to avoid including ~1.2K of FP/multiplication library
 * AREF ADC->mV factor   approx integer fraction
 * ---- --------------   -----------------------
 * 1.1      6.45          1651/256 (~103/16)   (103 = 64 + 32 + 8 - 1)
 * 3.3     19.35          4955/256 (~155/8)    (155 = 128 + 32 - 4 - 1)
 *
 * Note, if we ONLY needed to compare the ADC reading to a FIXED voltage,
 * we'd simplify by letting the compiler calculate the equivalent 0-1023 value
 * at compile time, and compare against that directly.
 * eg to check voltage is < 3500mV:
 *   if (analogueRead(BATT) < 3500/19.35) {...}
 *
 * Also the calculations don't need to be too accurate, because the ADC
 * itself can be be quite innacurate (up to 10% with internal AREF!)
 */
uint16_t batteryRead(void)
{
  //return ((uint32_t)analogRead(BATT) * 155) / 8;
  uint32_t mV = 0;

  for (uint8_t i = 155; i;i--)
  {
    mV += analogRead(BATT);
  }
  return mV/8;
}


const uint8_t CHIP_SELECT = SS;  // SD chip select pin (SS = 10)
SdCard card;
Fat16 file;
char file_name[13];
uint16_t file_size;

boolean fileInit(void)
{
  digitalWrite (SD_POWER, HIGH);        //  SD card on
  wait (1000);

  // init sd card
  if (!card.begin(CHIP_SELECT))
  {
    Serial.print(F("failed card.begin ")); Serial.println(card.errorCode);
	  return false;
  }
  
  // initialize FAT16 volume
  if (!Fat16::init(&card))
  {
	  Serial.println(F("Fat16::init failed"));
	  return false;
  }

  return true;
}


boolean fileOpen(uint8_t mode)
{

  Serial.print(F("opening file (mode 0x"));
  Serial.print(mode, HEX);
  Serial.print(F("):"));
  Serial.println(file_name);
  return file.open(file_name, mode);
}

boolean fileOpenWrite(void) { return(fileOpen(O_CREAT | O_WRITE | O_TRUNC)); }

boolean fileOpenRead(void)  { return(fileOpen(O_READ)); }

boolean fileClose(void)
{
  file.close();
  digitalWrite (SD_POWER, LOW);        //  SD card on
}


uint32_t fileCRC(uint32_t len)
{
  char c;
  uint32_t crc = ~0L;

  for(int i = 0; i < len; i++)
  {
    c = file.read();
    crc = crc_update(crc, c);
  }
  return ~crc;
}


// write firmware filename to EEPROM and toggle boot-from-SDcard flag at EEPROM[E2END]
void eepromWrite(void)
{
  uint8_t x;

  for (x = 0; x < 12 && file_name[x] != 0; x++)
  {
    EEPROM.write( (EEPROM_FILENAME_ADDR - x), file_name[x] );
  }
  EEPROM.write(E2END, 0); // 0 triggers an attempt to flash from SD card on power-on or reset
}

// serial flow control on
#define XON  17
void xon(void)
{
  digitalWrite(FONA_RTS, HIGH);
}

// serial flow control off
#define XOFF 19
void xoff(void)
{
  digitalWrite(FONA_RTS, LOW);
}


//const static uint32_t key[4] = {
const static PROGMEM prog_uint32_t key[4] = {
  KEY1, KEY2, KEY3, KEY4
};

// decrypt 8 bytes
void xtea(uint32_t v[2])
{
    uint8_t i;
    uint32_t v0=v[0], v1=v[1], delta=0x9E3779B9, sum=delta * 32;

    for (i=0; i < 32; i++)
    {
        v1 -= (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + pgm_read_dword_near(&key[(sum>>11) & 3]));
        sum -= delta;
        v0 -= (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + pgm_read_dword_near(&key[sum & 3]));
    }
    v[0]=v0; v[1]=v1;
}

/*
void xtea(uint32_t v[2])
//void xtea(char *block)
{
    uint8_t i;
    // uint32_t delta=0x9E3779B9, sum=delta * 32;
    uint32_t v0, v1, delta=0x9E3779B9, sum=delta * 32;

    v0  = block[0] << 24;
    v0 += block[1] << 16;
    v0 += block[2] << 8;
    v0 += block[3];
    v1  = block[4] << 24;
    v1 += block[5] << 16;
    v1 += block[6] << 8;
    v1 += block[7];

    for (i=0; i < 32; i++)
    {
        v1 -= (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + pgm_read_dword_near(&key[(sum>>11) & 3]));
        //v1 -= (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + key[(sum>>11) & 3]);
        sum -= delta;
        v0 -= (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + pgm_read_dword_near(&key[sum & 3]));
        //v0 -= (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key[sum & 3]);
    }
}
*/


boolean fileDecrypt() {
  byte c;
  uint8_t i;
  uint32_t crc;
  union xtea_block_t
  {
    char chr[8];
    uint32_t v[2];
  };
  xtea_block_t x;

  crc = ~0L;

      c = c ^ x.chr[i];
      if (i++ == 8)
      {
        i = 0;
        xtea(x.v);
      }
      crc = crc_update(crc, c);
}


const static PROGMEM prog_uint32_t crc_table[16] = {
    0x00000000, 0x1db71064, 0x3b6e20c8, 0x26d930ac,
    0x76dc4190, 0x6b6b51f4, 0x4db26158, 0x5005713c,
    0xedb88320, 0xf00f9344, 0xd6d6a3e8, 0xcb61b38c,
    0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
};
uint32_t crc_update(uint32_t crc, byte data)
{
    byte tbl_idx;
    tbl_idx = crc ^ (data >> (0 * 4));
    crc = pgm_read_dword_near(&crc_table[tbl_idx & 0x0f]) ^ (crc >> 4);
    tbl_idx = crc ^ (data >> (1 * 4));
    crc = pgm_read_dword_near(&crc_table[tbl_idx & 0x0f]) ^ (crc >> 4);
    return crc;
}


// Read len bytes from fona serial and write to file buffer
uint16_t fonaReadBlock(uint16_t len)
{
  uint16_t n = 0;
  uint32_t timeout = millis() + 2500;

  xon();
  while (n < len && millis() < timeout)
  {
    if (fona.available())
    {
      if (!file.write(fona.read())) break;
      n++;
    }
  }
  xoff();

  return n;
}

boolean fonaFileSize()
{
  fona.sendCheckReply (F("AT+FSFLSIZE=C:\\User\\ftp\\tmp.bin"), OK);
}

#define BLOCK_ATTEMPTS 3
#define BLOCK_SIZE 512
boolean fonaFileCopy(uint16_t len)
{
  uint32_t address = 0;
  uint16_t size = BLOCK_SIZE;
  uint16_t n;
  uint8_t retry_attempts = BLOCK_ATTEMPTS;
  DEBUG_RAM

  while (address < len)
  {
    fonaFlush();  // flush any notifications

    if (!retry_attempts--) return false;

    file.seekSet(address);   // rewind to beginning of block
    Serial.print(address); Serial.print(':');

    fona.print(F("AT+FSREAD=C:\\User\\ftp\\tmp.bin,1,"));
    fona.print(size);
    fona.print(',');
    fona.println(address);

    // fona returns \r\n from submitting command
    if (fonaRead() != '\r') continue;  // start again if it's something else
    if (fonaRead() != '\n') continue;

    n = fonaReadBlock(size);
    Serial.print(n);

    if (n == size && fona.expectReply(OK))
    {
      if (!file.sync())
      {
        return false;
      }
    } else { // didn't get a full block, or missing OK status response
      Serial.println();
      continue;
    }

    retry_attempts = BLOCK_ATTEMPTS;
    address += size;
    if (address + size > len)  // CHECKME should by >= ??
    {
      size = len - address;    // CHECKME len-address, or len-address-1 ??
    }
  }
  return true;
}


void ftpEnd(void)
{
  fona.sendCheckReply (F("AT+FTPQUIT"), OK);
  fona.sendCheckReply (F("AT+FTPSHUT"), OK);
}


/*
 * Fetch firmware from FTP server to FONA's internal filesystem
 */
boolean ftpGet(void)
{
  // configure FTP
  fona.sendCheckReply (F("AT+SSLOPT=0,1"), OK); // 0,x dont check cert, 1,x client auth
  fona.sendCheckReply (F("AT+FTPSSL=0"), OK);   // 0 ftp, 1 implicit (port is an FTPS port), 2 explicit
  fona.sendCheckReply (F("AT+FTPCID=1"), OK);
  fona.sendCheckReply (F("AT+FTPMODE=1"), OK);     // 0 ACTIVE, 1 PASV
  fona.sendCheckReply (F("AT+FTPTYPE=\"I\""), OK); // "I" binary, "A" ascii
  fona.sendCheckReply (F("AT+FTPSERV=\"" FTPSERVER "\""), OK);
  fona.sendCheckReply (F("AT+FTPUN=\"" FTPUSER "\""), OK);
  fona.sendCheckReply (F("AT+FTPPW=\"" FTPPW "\""), OK);
  fona.sendCheckReply (F("AT+FTPGETPATH=\"/home/ftpuser/files/\""), OK);

  // remote filename
  Serial.print(F("AT+FTPGETNAME=\""));
  Serial.print(file_name);
  Serial.println(F("\""));

  fona.print(F("AT+FTPGETNAME=\""));
  fona.print(file_name);
  fona.println(F("\""));
  fona.expectReply (OK);

  // local file path on fona
  fona.sendCheckReply (F("AT+FSDEL=C:\\User\\ftp\\tmp.bin"), OK); // delete previous download file

  // start the download to local file
  fona.sendCheckReply (F("AT+FTPGETTOFS=0,\"tmp.bin\""), OK);

  // Wait for download complete; FTPGETOFS status 0
  uint32_t timeout = millis() + 90000;
  while( !fona.sendCheckReply (F("AT+FTPGETTOFS?"), F("+FTPGETTOFS: 0")) ) {
    delay(2000);
    if (millis() > timeout)
    {
      return false;
    }
  }

  // Check the file exists
  if (fona.sendCheckReply (F("AT+FSFLSIZE=C:\\User\\ftp\\tmp.bin"), F("ERROR")))
  {
    return false;
  }

  return true;
}


boolean firmwareGet(void)
{ 
  // Ensure GPRS is on
  fonaOn();
  if (fona.GPRSstate() != 1)
  {
    Serial.println(F("no GPRS"));
    return false;
  }

  Serial.println(F("Fetching FW"));

  for (uint8_t tries=3 ;tries;tries--)
  {
    if ( ftpGet() )
    {
      ftpEnd();

      for (tries=3 ;tries;tries--)
      {
        if ( fileInit() && fileOpenWrite() )
        {
          if (fonaFileCopy(file_size))
          {
            ftpEnd();
            fileClose();
            return true;
          }
        }
      }
      break;
    }
  }
  ftpEnd();
  fileClose();
  Serial.println(F("fona copy failed"));
  return false;
}


void reflash (void) {
    Serial.println(F("updating eeprom...."));
    eepromWrite();

    Serial.println(F("reflashing...."));
    delay(100);

    //SP=RAMEND;
    flash_firmware(file_name);
}


boolean dmisPost (int16_t streamHeight, boolean solar, uint16_t voltage)
{
    uint16_t statusCode;
    uint16_t dataLen;
    char postData[200];
    DEBUG_RAM

    // HTTP POST headers
    fona.sendCheckReply (F("AT+HTTPINIT"), OK);
    //fona.sendCheckReply (F("AT+HTTPSSL=1"), OK);   // SSL required
    fona.sendCheckReply (F("AT+HTTPPARA=\"URL\",\"http://dmis-staging.eu-west-1.elasticbeanstalk.com/api/v1/data/river-gauge\""), OK);
    fona.sendCheckReply (F("AT+HTTPPARA=\"REDIR\",\"1\""), OK);
    fona.sendCheckReply (F("AT+HTTPPARA=\"UA\",\"Tepmachcha/" VERSION "\""), OK);
    fona.sendCheckReply (F("AT+HTTPPARA=\"CONTENT\",\"application/json\""), OK);

    // Note the data_source should match the last element of the url,
    // which must be a valid data_source
    // To add multiple user headers:
    //   http://forum.sodaq.com/t/how-to-make-https-get-and-post/31/18
    fona.sendCheckReply (F("AT+HTTPPARA=\"USERDATA\",\"data_source: river-gauge\\r\\nAuthorization: Bearer " DMISAPIBEARER "\""), OK);

    // json data
    sprintf_P(postData,
      (prog_char*)F("{\"sensorId\":\"" SENSOR_ID "\",\"streamHeight\":%d,\"charging\":%d,\"voltage\":%d,\"timestamp\":\"%d-%02d-%02dT%02d:%02d:%02d.000Z\"}"),
        streamHeight,
        solar,
        voltage,
        now.year(), now.month(), now.date(), now.hour(), now.minute(), now.second());
    int s = strlen(postData);

    // tell fona to receive data, and how much
    Serial.print (F("data size:")); Serial.println (s);
    fona.print (F("AT+HTTPDATA=")); fona.print (s);
    fona.println (F(",2000")); // timeout
    fona.expectReply (OK);

    // send data
    Serial.print(postData);
    fona.print(postData);
    delay(100);

    // do the POST request
    fona.HTTP_action (1, &statusCode, &dataLen, 10000);

    // report status, response data
    Serial.print (F("http code: ")); Serial.println (statusCode);
    Serial.print (F("reply len: ")); Serial.println (dataLen);
    if (dataLen > 0)
    {
      fona.sendCheckReply (F("AT+HTTPREAD"), OK);
      delay(1000);
    }

    fonaFlush();
    fona.HTTP_POST_end();

    return (statusCode == 201);
}


void smsSend() {
  fona.sendSMS("+855969101010", "hello from tepmachcha");
}


int freeRam (void) {
  extern int __heap_start, *__brkval; 
  int v; 
  return (int) &v - (__brkval == 0 ? (int) &__heap_start : (int) __brkval); 
}

void ram(void) {
  Serial.print (F("Ram free: "));
  Serial.println (freeRam());
}

void printMenu(void) {
  Serial.println(F("-------------------------------------"));
  Serial.println(F("[?] Print this menu"));
  Serial.println(F("[b] battery mV"));
  Serial.println(F("[.oO] FONA off/power-on/GPRS-on"));
  Serial.println(F("[G] GPRS connect only"));
  Serial.println(F("[eE] read/write EEPROM"));
  Serial.println(F("[r] re-boot-re-flash"));
  Serial.println(F("[f] ftp firmware to fona"));
  Serial.println(F("[c] copy file from fona to SD"));
  Serial.println(F("[F] ftp firmware to SD"));
  Serial.println(F("[T] Time - clockSet"));
  Serial.println(F("[N] Now = time from RTC"));
  Serial.println(F("[d] dmis"));
  Serial.println(F("[X] set test file_name/size"));
  Serial.println(F("[v] SD file info"));
  Serial.println(F("[i] fona FS info"));
  Serial.println(F("[t] take reading"));
  Serial.println(F("[#] smsCount"));
  Serial.println(F("[s] sms check"));
  Serial.println(F("[D] delete all SMS"));
  Serial.println(F("[C] CRC test"));
  Serial.println(F("[L] A-GPS Location"));
  Serial.println(F("[M] free memory"));
  Serial.println(F("[1] send test sms"));
  Serial.println(F("[:] Passthru command"));
  Serial.println(F("[q] Quit test panel"));
}


void test(void)
{

  while (!Serial.available() ) {
    if (fona.available()) {
      Serial.write(fona.read());
    }
  }

  Serial.print(F("FONA> "));

  char command = Serial.read();
  Serial.println(command);

  switch (command) {
    case 'q': {
       //testmenu = 0;
       break;
    }
    case '?': {
      printMenu();
      break;
    }
    case 'b': {
      Serial.print (F("stalker batt: "));
      Serial.print (batteryRead());
      Serial.println (F("mV"));
      Serial.print (F("fona batt: "));
      uint16_t v;
      fona.getBattVoltage (&v);   //  Read the battery voltage from FONA's ADC
      Serial.println (v);
      Serial.print(F("Solar: "));
      Serial.println(solarCharging());
      break;
    }
    case 'O': {
      if (fonaSerial && fona.sendCheckReply(F("AT"), OK)) {
        if ( fonaGSMOn() ) {
          fonaGPRSOn();
        }
      } else
        fonaOn();
      break;
    }
    case '.': {
      fonaOff();
      break;
    }
    case 'f': {
      Serial.println(ftpGet());
      break;
    }
    case 'F': {
      Serial.println(firmwareGet());
      break;
    }
    case 'T': {
      clockSet();
      break;
    }
    case 'c': {
      if ( !(fileInit() && fileOpenWrite()) ) {
        break;
      }
      boolean s = fonaFileCopy(file_size);
      file.close();
      Serial.print(F("Return: "));
      Serial.println(s);
      break;
    }
    case 'd': {
      //dmisPost(takeReading(), solarCharging(), batteryRead());
      ews1294Post(takeReading());
      break;
    }
    case 'o': {
      fonaPowerOn();
      fonaSerialOn();
      break;
    }
    case 'X': {
      file_size = 29758;
      strcpy_P(file_name, (prog_char*)F("FIRM.BIN"));
      break;
    }
    case 'r': {
      Serial.println(F("reflashing...."));
      delay(100);
      SP=RAMEND;
      flash_firmware(file_name);
      break;
    }
    case 'E': {
      eepromWrite();
      break;
    }
    case 'e': {
      char c;
      Serial.print('|');
      for (int x = 0; x < 12; x++) {
        c = EEPROM.read(EEPROM_FILENAME_ADDR - x);
        Serial.print(c);
      };
      Serial.println('|');
      break;
    }
    case ':': {
      Serial.println();
      uint8_t i;
      char buf[32];
      char c;
      while(c != '\r') {
        if (Serial.available()) {
          c = Serial.read();
          buf[i++] = c;
        }
      }
      buf[i] = 0;
      fona.println(buf);
      delay(500);
      break;
    }
    case 'i': {
      fona.sendCheckReply (F("AT+FSDRIVE=0"), OK);
      fona.sendCheckReply (F("AT+FSMEM"), OK);
		  fona.sendCheckReply (F("AT+FSLS=?"), OK);     //  Enter text mode
      fona.sendCheckReply (F("AT+FSLS=C:\\User\\ftp\\"), OK);
      fona.sendCheckReply (F("AT+FSFLSIZE=C:\\User\\ftp\\tmp.bin"), OK);
      break;
    }
    case 't': {
      takeReading();
      break;
    }
    case 'G': {
      fonaGPRSOn();
      break;
    }
    case 'N': {
		  now = RTC.now();    //  Get the current time from the RTC
      Serial.print(now.year());
      Serial.print(':');
      Serial.print(now.month());
      Serial.print(':');
      Serial.print(now.date());
      Serial.print('T');
      Serial.print(now.hour());
      Serial.print(':');
      Serial.println(now.minute());
      break;
    }
    case 'v': {

      fileInit();
      file.open(file_name, O_READ);
      file.rewind();
      for(int i = 0; i < file_size; i++) {
        char c;
        c = file.read();

        if ( i < 1024) {
          if (c > 31) {
            Serial.print(c);
          } else {
            Serial.print('.');
          }
          if ((i % 16) == 15) {
           Serial.println('|');
          }
        }
      }
      Serial.println();
      file.close();
      uint32_t crc;
      if (fileOpenRead()) crc = fileCRC(file_size);
      Serial.print(F("CRC: "));
      Serial.println(crc, HEX);
      file.close();
      break;
    }
    case 'D': {
      smsDeleteAll();
      break;
    }
    case '#': {
       smsCount();
       break;
    }
    case 's': {
       smsCheck();
       break;
    }
    case '1': {
       smsSend();
       break;
    }
    case 'M': {
      DEBUG_RAM
      break;
    }
    case 'C':{
      union xtea_block_t {
        char chr[8];
        uint32_t x[2];
      };

      xtea_block_t buffer;

      buffer.x[0] = 0xc44106f9; // ECB output
      buffer.x[1] = 0xde62a5c5;

      /*
      buffer.x[0] = 0xe48932fa; // ofb output ?????
      buffer.x[1] = 0x5caaaba9;

      buffer.x[0] = 0xd1b9cf77; // ofb IV???
      buffer.x[1] = 0x0f7f644c;

      buffer.x[0] = 0x48454c4c; // HELL
      buffer.x[1] = 0x474f4f44; // GOOD

      buffer.x[0] = 0;
      buffer.x[1] = 0;

      buffer.x[0] = 0xde159efb; // nOFB IV??
      buffer.x[1] = 0xe9ffb3d;
      */
      xtea(buffer.x);

      //buffer.x[0] ^= 0xe48932fa; ?? output
      //buffer.x[1] ^= 0x5caaaba9;

      //buffer.x[0] ^= 0x4715e50b; // nOFB output
      //buffer.x[1] ^= 0x84933e8f;

      Serial.println(buffer.chr);
      Serial.print(buffer.x[0], HEX);
      Serial.print(",");
      Serial.println(buffer.x[1], HEX);
      {
        uint32_t crc = ~0L;
        const char* s = "HELLO";
        while (*s)
          crc = crc_update(crc, *s++);
        Serial.print(F("HELLO: "));
        Serial.println(~crc, HEX);
      }
      Serial.print(file_name);
      Serial.print(F(": "));
      Serial.println(F(": "));
      if (fileOpenRead()) {
        Serial.println(fileCRC(file_size));
        file.close();
      }
      break;
    }
    case 'L': {
		  //fona.sendCheckReply (F("AT+CIPGSMLOC=1,1"), OK); //  Query GSM location service for lat/long/time
		  fona.sendCheckReply (F("AT+CIPGSMLOC=1,1"), F("+CIPGSMLOC: 0,")); //  Query GSM location service for lat/long/time
      break;
    }
    case 'u': {
      upload(100);
      break;
    }
  }
  fonaFlush();
}
