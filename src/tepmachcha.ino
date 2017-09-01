
#define VERSION "1.1"      //  Version number

//  Customize these items for each installation
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
#define TOKEN_ID      "EWS_TOKEN"
#define DEVICE_ID     "EWS_DEVICE"

#define SENSOR_ID     "sensor id"
#define APIBEARER     "YOUR_BEARER_ID"

#define FTPSERVER     "YOUR FTP SERVER"
#define FTPUSER       "YOUR FTP USER"
#define FTPPW         "YOUR FTP PASSWORD"
#define BEEPASSWORD   "XBEE_PASSWORD"  // Password to turn on XBee by SMS
#define FOTAPASSWORD  "FOTA_PASSWORD"  // Password to turn trigger FOTA update
#define APN           "FONAapn"

#define APN           "cellcard"
#define APIBEARER     "TWpZLkRJYWpFZy5VeEZyTTBLOHRvOWttZTAzcVdjbmRpY2pHQTA="
#define FTPSERVER     "hackerspacephnompenh.com"
#define FTPUSER       "ftpuser"
#define FTPPW         "t0ult0mp0ng"
#define BEEPASSWORD   "XBEE_PASSWORD"  //  Password to turn on XBee by SMS
#define FOTAPASSWORD  "FOTA_PASSWORD"  //  Password to turn trigger FOTA update

#define SENSOR_HEIGHT  100  //  Height of top of octagonal gasket from streambed, in cm
#define UTCOFFSET        0  //  Local standard time variance from UTC
#define XBEEWINDOWSTART 14  //  Hour to turn on XBee for programming window
#define XBEEWINDOWEND   17  //  Hour to turn off XBee
#define INTERVAL        15  //  Number of minutes between readings
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

#include <DS1337.h>           //  For the Stalker's real-time clock (RTC)
#include <Sleep_n0m1.h>       //  Sleep library
#include <SoftwareSerial.h>   //  For serial communication with FONA
#include <Wire.h>             //  I2C library for communication with RTC
//#include <I2C.h>             //  replacemnet I2C library
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
#define BATT     A7 //  Battery level

#define DOTIMES(x) for(uint16_t _i=x; _i;_i--)

static const char OK_STRING[] PROGMEM = "OK";
//__FlashStringHelper* OK = (__FlashStringHelper*)OK_STRING;
#define OK ((__FlashStringHelper*)OK_STRING)

// call into bootloader jumptable at top of flash
#define write_flash_page (*((void(*)(const uint32_t address))(0x7ffa/2)))
#define flash_firmware (*((void(*)(void))(0x7ffc/2)))
#define EEPROM_FILENAME_ADDR (E2END - 1)

boolean sentData = false;
boolean smsPower = false;       //  Manual XBee power flag
boolean noSMS = false;          //  Flag to turn off SMS checking -- for future use
boolean timeReset = false;      //  Flag indicating whether midnight time reset has already occurred
byte beeShutoffHour = 0;        //  Hour to turn off manual power to XBee
byte beeShutoffMinute = 0;      //  Minute to turn off manual power to XBee
char method = 0;                //  Method of clock set, for debugging

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

    // Compile-in the date and time; helps identify software uploads
		// Note: C compiler concatenates adjacent strings
		Serial.println (F("Tepmachcha v" VERSION " " __DATE__ " " __TIME__));

		analogReference(DEFAULT); // atmega328: DEFAULT=3.3V, INTERNAL=1.1V, EXTERNAL=3.3V
    analogRead(BATT);         // must read once after changing reference

		Serial.print (F("Battery: "));
		Serial.print (readBattery());
		Serial.println (F("mV"));

		pinMode (BEEPIN, OUTPUT);
		pinMode (RANGE, OUTPUT);
		pinMode (FONA_KEY, OUTPUT);
		pinMode (FONA_RX, OUTPUT);

    // Set RTC interrupt handler
		attachInterrupt (RTCINT, rtcIRQ, FALLING);
		interrupts();


		/*   If the voltage at startup is less than 3.5V, we assume the battery died in the field
		 *   and the unit is attempting to restart after the panel charged the battery enough to
		 *   do so. However, running the unit with a low charge is likely to just discharge the
		 *   battery again, and we will never get enough charge to resume operation. So while the
		 *   measured voltage is less than 3.5V, we will put the unit to sleep and wake once per 
		 *   hour to check the charge status.
		 */
		while (readBattery() < 3500)   
		{
				Serial.println (F("Low power sleep"));
				Serial.flush();
				digitalWrite (BEEPIN, HIGH);      //  Make sure XBee is powered off
				digitalWrite (RANGE, LOW);        //  Make sure sonar is off
				RTC.enableInterrupts (EveryHour); //  We'll wake up once an hour
				RTC.clearINTStatus();             //  Clear any outstanding interrupts
				//attachInterrupt (RTCINT, rtcIRQ, FALLING);
				//interrupts();
				sleep.pwrDownMode();                    //  Set sleep mode to Power Down
				sleep.sleepInterrupt (RTCINT, FALLING); //  Sleep; wake on falling voltage on RTC pin
		}

		digitalWrite (RANGE, HIGH);          //  If set low, sonar will not range
		digitalWrite (FONA_KEY, HIGH);       //  Initial state for key pin

    fonaPowerOn();
    fonaSerialOn();

    // set ext. audio, to prevent crash on incoming calls
    // https://learn.adafruit.com/adafruit-feather-32u4-fona?view=all#faq-1
    //fona.sendCheckReply(F("AT+CHFA=1"), OK);

    /*
		// We will use the FONA to get the current time to set the Stalker's RTC
		fonaOn();
		clockSet();
		
		// Delete any accumulated SMS messages to avoid interference from old commands
    smsDeleteAll();
    fona.sendCheckReply (F("AT+CHFA=1"), OK);   // set audio, prevents crash?
		
    fonaOff();
    */

		now = RTC.now();    //  Get the current time from the RTC

		RTC.enableInterrupts (EveryMinute);  //  RTC will interrupt every minute
		RTC.clearINTStatus();                //  Clear any outstanding interrupts
		
		// We'll keep the XBee on for an hour after startup to assist installation
		if (now.hour() == 23)
		{
				beeShutoffHour = 0;
		}
		else
		{
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
test();
return;

		now = RTC.now();      //  Get the current time from the RTC

		Serial.print (now.hour());
		Serial.print (F(":"));
		Serial.println (now.minute());

		int streamHeight = takeReading();
    //return;         // JACK

		/*  One failure mode of the sonar -- if, for example, it is not getting enough power -- 
	   *	is to return the minimum distance the sonar can detect; in the case of the 10m sonars
	   *	this is 50cm. This is also what would happen if something were to block the unit -- a
	   *	plastic bag that blew onto the enclosure, for example. We very much want to avoid false
	   *	positive alerts, so for the purposes of yellow and red alerts, we will ignore anything
	   *	less than 55cm from the sensor. 
     *
	   *	Per discussions with PIN, alerts will be cleared manually, by sending an SMS to the 
	   *	unit.
     */

		if (now.minute() % INTERVAL == 0 && sentData == false)   //  If it is time to send a scheduled reading...
		{
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
				                                  Serial.println (F("Turning XBee off..."));
				                                  Serial.flush();
				                                  wait (500);
				                        }
		
				                        digitalWrite (BEEPIN, HIGH);
				                        smsPower = false;
				                }
				        }
				}
		}

		Serial.flush();                         //  Flush any output before sleep

		sleep.pwrDownMode();                    //  Set sleep mode to "Power Down"
		RTC.clearINTStatus();                   //  Clear any outstanding RTC interrupts
		sleep.sleepInterrupt (RTCINT, FALLING); //  Sleep; wake on falling voltage on RTC pin

}


void upload(int streamHeight)
{
		fonaOn();
		//sendReading (streamHeight);
		dmis (streamHeight);

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

		sentData = true;
}



void clockSet (void)
{
		wait (1000);    //  Give time for any trailing data to come in from FONA

		int netOffset;

		char theDate[17];

		Serial.println (F("Fetching GSM time"));

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
				netSecond = netMinute;  //  ...shift everything up one to capture that second int
				netMinute = netHour;
				netHour = netDay;
				netDay = netMonth;
				netMonth = netYear;
				netYear = secondInt;

				Serial.println (F("Recombobulating..."));
		}

		if (netYear < 2016 || netYear > 2050 || netHour > 23)   // If that still didn't work...
		{
				Serial.println (F("GSM location service failed"));
				/*   ...the we'll get time from the NTP pool instead: 
				 *   (https://en.wikipedia.org/wiki/Network_Time_Protocol)
				 */
				fona.enableNTPTimeSync (true, F("0.daimakerlab.pool.ntp.org"));
				Serial.println (F("enabling NTP sync"));
				
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

		if ((netYear < 1000 && netYear >= 16 && netYear < 50) || (netYear > 1000 && netYear >= 2016 && netYear < 2050))
		//  If we got something that looks like a valid date...
		{
				//  Adjust UTC to local time
				if((netHour + UTCOFFSET) < 0)                   //  If our offset + the UTC hour < 0...
				{
				        netHour = (24 + netHour + UTCOFFSET);   //  ...add 24...
				        netDay = (netDay - 1);                  //  ...and adjust the date to UTC - 1
				}
				else
				{
				        if((netHour + UTCOFFSET) > 23)          //  If our offset + the UTC hour > 23...
				        {
				                netHour = (netHour + UTCOFFSET - 24); //  ...subtract 24...
				                netDay = (netDay + 1);                //  ...and adjust the date to UTC + 1
				        }
				        else
				        {
				                netHour = (netHour + UTCOFFSET);      //  Otherwise it's straight addition
				        }
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
				Serial.println (F("Network time failed, continue with RTC"));
				method = 'X';
		}

		wait (200);              //  Give FONA a moment to catch its breath
}


void wait (unsigned long period)
{
  // Non-blocking delay function
  unsigned long waitend = millis() + period;
  while (millis() <= waitend)
  {
    Serial.flush();
  }
}


void fonaFlush (void)
{
  // Read all available serial input from FONA to flush any pending data.
  while(fona.available())
  {
    char c = fona.read();
    Serial.print (c);
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
  unsigned long gsmTimeout = millis() + 30000;

  Serial.print (F("Connecting GSM... "));
  while (millis() < gsmTimeout)
  {
    uint8_t status = fona.getNetworkStatus();
    if (status == 1 || status == 5)
    {
      Serial.println(F("done."));
      return true;
    }
    wait (500);
  }
  Serial.println (F("timed out. Check SIM card, antenna, and signal."));
  return false;
}


boolean fonaGPRSOn(void) {
  //  RSSI is a measure of signal strength -- higher is better; less than 10 is worrying
  uint8_t rssi = fona.getRSSI();

  Serial.print (F("RSSI: ")); Serial.println (rssi);

  fona.setGPRSNetworkSettings (F(APN));    //  Set APN to your local carrier
  wait (3000);    //  Give the network a moment

  if (rssi > 5)
  {
    for (uint8_t attempt = 1; attempt < 10; attempt++)
    {
      Serial.print (F("Turning GPRS on, attempt ")); Serial.println(attempt);
      fona.enableGPRS (true);

      if (fona.GPRSstate() == 1) {
        Serial.println (F("GPRS is on."));
        return true;
      }
      Serial.println(F("Failed"));
      wait (1000);
    }
  } else {
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
  if (fonaSerialOn()) {
    Serial.print (F("FONA online. "));
    if ( fonaGSMOn() ) {
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
		
		for (int sampleCount = 0; sampleCount < 7; sampleCount++)
		{
				sample[sampleCount] = pulseIn (PING, HIGH);
				Serial.print (F("Sample "));
				Serial.print (sampleCount);
				Serial.print (F(": "));
				Serial.println (sample[sampleCount]);
				wait (10);
		}

		int sampleMode = mode (sample, 7);

		int streamHeight = (SENSOR_HEIGHT - (sampleMode / 10)); //  1 µs pulse = 1mm distance

		Serial.print (F("Surface distance from sensor is "));
		Serial.print (sampleMode);
		Serial.println (F("mm."));
		Serial.print (F("Calculated surface height is "));
		Serial.print (streamHeight);
		Serial.println (F("cm."));

		return streamHeight;
}


boolean validate (int alertThreshold)
{
  /*   False positives would undermine confidence in the IVR alerts, so we must take
   *   pains to avoid them. Before triggering an alert flow, we will validate the
   *   reading by taking five readings and making sure they _all_ agree. If the
   *   levels are marginal, that might mean we don't send an alert for a while (because
   *   some readings might come in below the threshold).
   */

  boolean valid = true;

  for (int i = 0; i < 5; i++)
  {
	  wait (5000);
	  Serial.print (F("Validation reading #"));
	  Serial.println (i);

	  int doubleCheck = takeReading();

	  if (doubleCheck < alertThreshold)
    {
	    Serial.println (F("Validation does not agree."));
	    valid = false;
	    break;
	  }
  }
  return valid;
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
boolean solarCharging() {
  if (analogRead (6) <= 900 && analogRead (6) > 550)
  {
    return true;
  }
  else
  {
    return false;
  }
}

boolean sendReading (int streamHeight)
{
        uint16_t statuscode = 0;
        int16_t length = 0;
        char data[255];

        unsigned int voltage;
        fona.getBattVoltage (&voltage);   //  Read the battery voltage from FONA's ADC

        int sol = solarCharging();

        // Construct the body of the POST request:
        sprintf (data, "api_token=%s&data={\"sensorId\":\"%s\",\"streamHeight\":\"%d\",\"charging\":\"%d\",\"voltage\":\"%d\",\"timestamp\":\"%d-%d-%dT%d:%d:%d.000Z\"}\r\n", TOKEN_ID, DEVICE_ID, streamHeight, sol, voltage, now.year(), now.month(), now.date(), now.hour(), now.minute(), now.second());

        Serial.println (F("Sending data:"));
        Serial.println (data);

        //  ews1294.info does not currently support SSL; if it is added you will need to uncomment the following
        //fona.sendCheckReply (F("AT+HTTPSSL=1"), F("OK"));   //  Turn on SSL
        //fona.sendCheckReply (F("AT+HTTPPARA=\"REDIR\",\"1\""), F("OK"));  //  Turn on redirects (for SSL)

        // Send the POST request we have constructed
        fona.HTTP_POST_start ("ews1294.info/api/v1/sensorapi", F("application/x-www-form-urlencoded"), (uint8_t *)data, strlen (data), &statuscode, (uint16_t *) &length);
        fona.println (F("AT+HTTPREAD"));
        
        while (length > 0)
        {
                fonaFlush();

                length--;
                if (!length)
                {
                        break;
                }
        }

        if (statuscode == 200)
        {
                Serial.println (F("POST request succeeded."));
                return true;
        }
        else
        {
                Serial.print (F("POST request appeared to fail. Status-code: "));
                Serial.println (statuscode);
                return false;
        }

        fona.HTTP_POST_end();
}


void smsDeleteAll(void)
{
  fona.sendCheckReply (F("AT+CMGF=1"), OK);            //  Enter text mode
  fona.sendCheckReply (F("AT+CMGDA=\"DEL ALL\""), OK); //  Delete all SMS messages                }
}


uint16_t smsCount (void)
{
		/*   During testing on metfone, sometimes the FONA had to be on for more
		 *   than 60 seconds(!) before it would receive a previously sent SMS message,
		 *   so we'll keep looking for 60 seconds.
		 */
		uint32_t smsTimeout = millis() + 60000;
    uint16_t NumSMS;

		Serial.println (F("Checking for SMS messages..."));
    ram();

		do {
				NumSMS = fona.getNumSMS();
				wait (5000);
		} while (NumSMS == 0 && millis() <= smsTimeout);

		Serial.print (NumSMS);
		Serial.println (F(" message(s) waiting."));
    return NumSMS;
}

#define SMS_MAX_LEN 160
#define SMS_SENDER_MAX_LEN 20
//#define SMS_REPLY_LEN 60
#define FORMAT_LEN 24
//  Check SMS messages received for any valid commands
void smsCheck (void)
{
		char smsBuffer[SMS_MAX_LEN];  //??
		char smsSender[SMS_SENDER_MAX_LEN];
		unsigned int smsLen;
		boolean sendStatus = false;
		uint16_t NumSMS;

    ram();

		fonaFlush();    //  Flush out any unresolved data
		
    NumSMS =  smsCount();

		unsigned long timeOut = (millis() + 60000);
		while (NumSMS > 0)          //  If there are messages...
		{
				fona.readSMS (NumSMS, smsBuffer, SMS_MAX_LEN, &smsLen);  //  ...retrieve the last one...
				wait (500);                                              //  ...give FONA a moment to catch up...

				fona.getSMSSender (NumSMS, smsSender, SMS_SENDER_MAX_LEN);   //  ...find out who send it...
				wait (500);

				Serial.print (F("Message from "));
				Serial.print (smsSender);
				Serial.println (F(":"));
				Serial.println (smsBuffer);

				//  Now check to see if any of the declared passwords are in the SMS message and respond accordingly
				if (strcmp_P (smsBuffer, (prog_char*)F(FOTAPASSWORD)) == 0)        //  FOTA password...
        {
            // read filename, size, cksum
        }

				if (strcmp_P (smsBuffer, (prog_char*)F(BEEPASSWORD)) == 0)        //  XBee password...
				{
				        //  ...determine the appropriate shutoff time and turn on the XBee until then
                beeShutoffHour = (now.hour() + 1);    //  We'll leave the XBee on for 1 hour
                if (beeShutoffHour == 24)
                  beeShutoffHour = 0;
				        
				        digitalWrite (BEEPIN, LOW);        //  Turn on the XBee

				        // Compose a reply to the sender confirming the action and giving the shutoff time
				        sprintf_P(smsBuffer, (prog_char *)F("XBee on until %02d:%02d"), beeShutoffHour, beeShutoffMinute);

				        smsPower = true;                    //  Raise the flag 
				        fona.sendSMS(smsSender, smsBuffer);    //  Tell the sender what you've done

				        Serial.println (F("XBee turned on by SMS."));
				}

				wait (1000);
				fona.deleteSMS (NumSMS);
				wait (1500);
				NumSMS = fona.getNumSMS();

				//  Occasionally messages won't delete and this loops forever. If
				//  the process takes too long we'll just nuke everything.
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
 * 3.3     19.35          4955/256 (~155/8)
 *
 * Note, if we ONLY needed to compare the ADC reading to a FIXED voltage,
 * we'd simplify by letting the compiler calculate the equivalent 0-1023 value
 * at compile time, and compare against that directly.
 * eg to check voltage is < 3500mV:
 *   if (analogueRead(BATT) < 3500/19.35) {...}
 *
 * Also the calculations don't really need to be too accurate, because the
 * ADC itself can be be quite innacurate (up to 10% with internal AREF!)
 */
uint16_t readBattery(void)
{
  uint16_t adc;
  uint32_t mV;
  
  adc = analogRead(BATT);

  // We could just add 155 times, but it's quicker to add/substract
  // powers-of-two multiples of adc, which can be easily be calculated
  // by shifting bits, eg shift <<2 bits for *4
  // 155 = 128 + 32 - 4 - 1
  mV = adc * 128;  // compiler smarts should convert * to equivalent <<
  mV += adc * 32;
  mV -= adc * 4;
  mV -= adc;
  return mV / 8;
}


const uint8_t CHIP_SELECT = SS;  // SD chip select pin (SS = 10)
SdCard card;
Fat16 file;
const char file_name[13] = "TEP2.BIN";

boolean fat_init(void)
{
  // init sd card
  if (!card.begin(CHIP_SELECT)) {
    Serial.print(F("failed card.begin ")); Serial.println(card.errorCode);
	  return false;
  }
  
  // initialize a FAT16 volume
  if (!Fat16::init(&card)) {
	  Serial.println(F("Fat16::init failed"));
	  return false;
  }

  return true;
}


boolean fileOpen(void)
{
  Serial.print(F("opening: ")); Serial.println(file_name);
  file.open(file_name, O_CREAT | O_WRITE | O_TRUNC);

  return file.isOpen();
}


// write firmware filename to EEPROM and toggle boot-from-SDcard flag at EEPROM[E2END]
void writeEeprom(void)
{
  uint8_t x;

  for (x = 0; x < 12 && file_name[x] != 0; x++) {
    EEPROM.write( (EEPROM_FILENAME_ADDR - x), file_name[x] );
  }
  EEPROM.write(E2END, 0); // 0 triggers an attempt to flash from SD card on power-on or reset
}

// serial flow control on
#define XON  17
void xon(void)
{
  //fonaSerial.write(XOFF);
  digitalWrite(FONA_RTS, HIGH);
}

// serial flow control off
#define XOFF 19
void xoff(void)
{
  //fonaSerial.write(XON);
  digitalWrite(FONA_RTS, LOW);
}


const static PROGMEM prog_uint32_t key[4] = {
    //0x9b64c2b0, 0x86d3d2d4, 0xa00ae278, 0xbdbdf21c
    0x01020304, 0x05060708, 0x090a0b0c, 0x0d0e0f00
};

/*
const static uint32_t key[4] = {
  0x01020304, 0x05060708, 0x090a0b0c, 0x0d0e0f00
};
*/
// decrypt 8 bytes
void xtea_decrypt(uint32_t v[2])
{
    uint8_t i;
    uint32_t v0=v[0], v1=v[1], delta=0x9E3779B9, sum=delta * 32;

    for (i=0; i < 32; i++)
    {
        v1 -= (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + pgm_read_dword_near(&key[(sum>>11) & 3]));
        //v1 -= (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + key[(sum>>11) & 3]);
        sum -= delta;
        v0 -= (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + pgm_read_dword_near(&key[sum & 3]));
        //v0 -= (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + key[sum & 3]);
    }

    v[0]=v0; v[1]=v1;
}


static uint32_t file_crc;
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
    crc = pgm_read_dword_near(crc_table + (tbl_idx & 0x0f)) ^ (crc >> 4);
    tbl_idx = crc ^ (data >> (1 * 4));
    crc = pgm_read_dword_near(crc_table + (tbl_idx & 0x0f)) ^ (crc >> 4);
    return crc;
}

uint32_t crc_string(char *s)
{
  uint32_t crc = ~0L;
  while (*s)
    crc = crc_update(crc, *s++);
  crc = ~crc;
  return crc;
}


// Read len bytes from fona serial and write to file buffer
uint16_t readBlock(uint16_t len)
{
  uint16_t n = 0;
  uint32_t crc = file_crc;
  uint32_t timeout = millis() + 2000;

  xon();
  while (n < len && millis() < timeout) {
    if (fona.available()) {
      //if (!file.write(fona.read())) break;
      char c = fona.read();
      crc = crc_update(crc, c);

      if (!file.write(c)) break;
      n++;
    }
  }
  xoff();

  if (n == len) file_crc = crc;
  return n;
}



uint16_t decryptBlock(uint16_t len)
{
  union {
    char chr[8];
    uint32_t x[2];
  } buffer;

  uint16_t n;
  while (n < len) {
    file.read(buffer.chr, 8);
    xtea_decrypt(buffer.x);
    for (uint8_t x = 0; x < 8; x++) {
      file_crc = crc_update(file_crc, buffer.chr[x]);
    }
    file.write(buffer.chr, 8);
    n += 8;
  }
}


boolean ftpCopyFile(uint16_t len)
{
  uint32_t address = 0;
  uint16_t size = 512;
  uint16_t n;
  uint8_t block_retries = 2;

  file_crc = ~0L;

  // quiet mode - commands don't return status
  fona.println(F("ATQ1"));

  while (address < len) {    // FIXME need to add timeout otherwise could loop forever looking for \r, \n, complete blocks etc
    fonaFlush();  // flush any notifications

    file.seekSet(address);   // rewind to beginning of block
    Serial.print(address); Serial.print(':');

    fonaSerial.print(F("AT+FSREAD=C:\\User\\ftp\\tmp.bin,1,"));
    fonaSerial.print(size);
    fonaSerial.print(',');
    fonaSerial.println(address);

    delay(120);
    // fona returns \r\n from submitting command despite ATQ1
    if (fonaRead() != '\r') continue;  // start again if it's something else
    if (fonaRead() != '\n') continue;

    n = readBlock(size);
    Serial.print(n);

    if (n == size && fona.expectReply(OK)) {
      if (!file.sync()) {
        return false;
      }
    } else {
      Serial.println();
      if (block_retries--) {
        continue;
      } else {
        return false;
      }
    }

    address += size;
    if (address + size > len)  // CHECKME should by >= ??
      size = len - address;    // CHECKME len-address, or len-address-1 ??
  }
  file_crc = ~file_crc;
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

  // remote filename
  Serial.print(F("AT+FTPGETNAME=\""));
  Serial.print(file_name);
  Serial.println(F("\""));

  fonaSerial.print(F("AT+FTPGETNAME=\""));
  fonaSerial.print(file_name);
  fonaSerial.println(F("\""));
  fona.expectReply (OK);

  // local file path on fona
  fona.sendCheckReply (F("AT+FSDEL=C:\\User\\ftp\\tmp.bin"), OK); // delete previous download file
  fona.sendCheckReply (F("AT+FTPGETPATH=\"/home/ftpuser/files/\""), OK);

  // start the download to local file
  fona.sendCheckReply (F("AT+FTPGETTOFS=0,\"tmp.bin\""), OK);

  // Wait for download complete; FTPGETOFS status 0
  uint32_t timeout = millis() + 90000;
  while( !fona.sendCheckReply (F("AT+FTPGETTOFS?"), F("+FTPGETTOFS: 0")) ) {
    delay(2000);
    if (millis() > timeout) {
      return false;
    }
  }

  // Check the file exists
  if (fona.sendCheckReply (F("AT+FSFLSIZE=C:\\User\\ftp\\tmp.bin"), F("ERROR"))) {
    return false;
  }

  return true;
}



boolean getFirmware(void)
{ 
  if ( !(fat_init() && fileOpen()) ) {
    return false;
  }

  if (fona.GPRSstate() != 1) {
    Serial.println(F("no GPRS"));
    return false;
  }

  Serial.println(F("Fetching FW"));

  if ( ftpGet() ) {
    ftpEnd();

    if (ftpCopyFile(23258)) {
      file.close();
      fonaOff();

      Serial.println(F("updating eeprom...."));
      writeEeprom();

      Serial.println(F("reflashing...."));
      delay(100);

      SP=RAMEND;
      flash_firmware();
    }
  }

  fonaOff();
  return false;
}


boolean dmis (int streamHeight)
{
        int statusCode;
        int dataLen;

        // HTTP POST headers
        fona.sendCheckReply (F("AT+HTTPINIT"), OK);
        //fona.sendCheckReply (F("AT+HTTPSSL=1"), OK);   // SSL required
        fona.sendCheckReply (F("AT+HTTPPARA=\"URL\",\"http://dmis-staging.eu-west-1.elasticbeanstalk.com/api/v1/data/river-gauge\""), OK);
        //fona.sendCheckReply (F("AT+HTTPPARA=\"URL\",\"http://www.hackerspacephnompenh.com/api/v1/data/river_gauge\""), OK);
        fona.sendCheckReply (F("AT+HTTPPARA=\"REDIR\",\"1\""), OK);
        fona.sendCheckReply (F("AT+HTTPPARA=\"UA\",\"Tepmachcha/" VERSION "\""), OK);
        fona.sendCheckReply (F("AT+HTTPPARA=\"CONTENT\",\"application/json\""), OK);

        // To add multiple user headers:
        //   http://forum.sodaq.com/t/how-to-make-https-get-and-post/31/18
        // Note the data_source must match the last element of the url, which must be a valid data_source
        /*
        fona.print (F("AT+HTTPPARA=\"USERDATA\",\"data_source: river-gauge\\r\\nAuthorization: Bearer "));
        fona.print (F(APIBEARER));
        fona.println (F("\""));
        fona.expectReply (OK);
        */
        fona.sendCheckReply(F("AT+HTTPPARA=\"USERDATA\",\"data_source: river-gauge\\r\\nAuthorization: Bearer " APIBEARER "\""), OK);

        char data[120];
        ram();

        sprintf_P(data,
          (prog_char*)F("{\"sensorId\":\"" SENSOR_ID "\",\"streamHeight\":%d,\"charging\":%d,\"voltage\":%d,\"timestamp\":\"%d-%02d-%02dT%02d:%02d:%02d.000Z\"}"),
            streamHeight,
            solarCharging(),
            readBattery(),
            now.year(), now.month(), now.date(), now.hour(), now.minute(), now.second());
        int s = strlen(data);

        Serial.print(F("data size:"));
        Serial.println(s);
        fona.print (F("AT+HTTPDATA="));
        fona.print (s);
        fona.println (F(",2000")); // timeout
        fona.expectReply (OK);

        Serial.print(data);
        fona.print(data);

        delay(100);
        fona.HTTP_action (1, &statusCode, &dataLen, 10000);   //  Send the POST request we've constructed

        Serial.print (F("Status code: "));
        Serial.println (statusCode);
        Serial.print (F("Reply length: "));
        Serial.println (dataLen);

        if (dataLen > 0) {
          fona.sendCheckReply (F("AT+HTTPREAD"),F("+HTTPREAD"));
          while (dataLen-- >= 0)
          {
            Serial.write(fonaRead());
          }
        }
        
        fona.HTTP_POST_end();

        if (statusCode == 201)
        {
          return true;
        }
        else
        {
          return false;
        }
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
  Serial.println(F("[b] read the Battery"));
  Serial.println(F("[E] write EEPROM"));
  Serial.println(F("[e] read EEPROM"));
  Serial.println(F("[.oO] FONA off/serial-on/full-on"));
  Serial.println(F("[G] Enable GPRS"));
  Serial.println(F("[r] re-boot-flash"));
  Serial.println(F("[f] ftpGet"));
  Serial.println(F("[c] ftp copy file from fona to SD card"));
  Serial.println(F("[T] Time - clockSet"));
  Serial.println(F("[N] Now get time from RTC"));
  Serial.println(F("[d] dmis"));
  Serial.println(F("[v] read sd file"));
  Serial.println(F("[i] FS info"));
  Serial.println(F("[t] take reading"));
  Serial.println(F("[#] smsCount"));
  Serial.println(F("[s] sms check"));
  Serial.println(F("[C] CRC test"));
  Serial.println(F("[L] A-GPS Location"));
  Serial.println(F("[:] Passthru command"));
}


void test(void)
{
  Serial.print(F("FONA> "));
  while (!Serial.available() ) {
    if (fona.available()) {
      Serial.write(fona.read());
    }
  }

  char command = Serial.read();
  Serial.println(command);

  switch (command) {
    case '?': {
      printMenu();
      break;
    }
    case 'b': {
      Serial.print (readBattery());
      Serial.println (F("mV"));
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
      ftpGet();
      break;
    }
    case 'T': {
      clockSet();
      break;
    }
    case 'c': {
      if ( !(fat_init() && fileOpen()) ) {
        break;
      }
      boolean s = ftpCopyFile(23258);
      file.close();
      Serial.print(F("return: "));
      Serial.println(s);
      Serial.print(F("CRC: "));
      Serial.println(file_crc);
      Serial.print(F("CRC: "));
      Serial.println(file_crc, HEX);
      break;
    }
    case 'd': {
      dmis(55);
      break;
    }
    case 'o': {
      fonaPowerOn();
      fonaSerialOn();
      break;
    }
    case 'E': {
      writeEeprom();
      break;
    }
    case 'r': {
      Serial.println(F("reflashing...."));
      delay(100);
      SP=RAMEND;
      flash_firmware();
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
      fat_init();
      file.open(file_name, O_READ);
      file.rewind();
      if (file.isOpen())
        Serial.println(F("open"));
      char c;
      for(int i = 0; i < 1024; i++) {
        c = file.read();
        if (c > 31) {
          Serial.print(c);
        } else {
          Serial.print('.');
        }
        if ((i % 16) == 15) {
           Serial.println('|');
        }
      }
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
    case 'M': {
      ram();
      break;
    }
    case 'C':{
      union xtea_block_t {
        char chr[8];
        uint32_t x[2];
      };

      xtea_block_t buffer;
      
      buffer.x[0] = 0xc44106f9;
      buffer.x[1] = 0xde62a5c5;
      xtea_decrypt(buffer.x);
      Serial.println(buffer.chr);
      Serial.print(buffer.x[0], HEX);
      Serial.print(",");
      Serial.println(buffer.x[1], HEX);
      Serial.print(F("HELLO: "));
      Serial.println(crc_string("HELLO"), HEX);
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
