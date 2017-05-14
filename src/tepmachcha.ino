
#define VERSION "1.03"      //  Version number

//  Customize these items for each installation
// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 
#define PUBLIC_KEY "YOUR_PUBLIC_KEY"      //  Public key for phant stream
#define PRIVATE_KEY "YOUR_PRIVATE_KEY"    //  Private key for phant stream
#define APITOKEN "YOUR_RAPIDPRO_API_TOKEN"  //  Rapidpro API token
#define TARGETCONTACT "TARGET_CONTACT_UUID" //  Rapidpro needs at least a dummy contact 
				                            //  to start a flow
#define SENSOR_HEIGHT 100   //  Height of top of octagonal gasket from streambed, in cm
#define UTCOFFSET 0         //  Local standard time variance from UTC
#define XBEEWINDOWSTART 14  //  Hour to turn on XBee for programming window
#define XBEEWINDOWEND 17    //  Hour to turn off XBee
#define INTERVAL 15         //  Number of minutes between readings
#define BEEPASSWORD "XBEE_PASSWORD"  //  Password to turn on XBee by SMS
#define CLEARYELLOW "CLEAR_YELLOW_PASSSWORD" //  Password to clear yellow alerts
#define CLEARRED "CLEAR_RED_PASSWORD"        //  Password to clear red alerts


/*   Tepmachcha is written to accommodate different alert levels for separate zones --
 *   a low-lying zone in the region might need a lower yellow alert level than another
 *   zone. Note that using more than three zones will cause an overflow in the SMS
 *   reply function as currently written.
 */
#define ZONES 2             //  Number of separate zones to be covered

/*   If you do not define yellow, red, yellowFlow, redFlow and alert for all of your zones, 
 *   weird things will happen. Don't forget that zones will be numbered starting from zero.
 */
 
const int yellow[ZONES] = {550, 500};        //  Yellow alert level for Zones 0 & 1
const int red[ZONES] = {600, 550};           //  Red alert level for Zones 0 & 1
const char* yellowFlow[ZONES] = {"RAPIDPRO_YELLOWALERT_FLOW_UUID_ZONE_0", "RAPIDPRO_YELLOWALERT_FLOW_UUID_ZONE_1"};
const char* redFlow[ZONES] = {"RAPIDPRO_REDALERT_FLOW_UUID_ZONE_0", "RAPIDPRO_REDALERT_FLOW_UUID_ZONE_1"};
char alert[ZONES] = {'G', 'G'};              //  Green, yellow, or red alert state (G, Y, R)
boolean sendYellow[ZONES] = {false, false};
boolean sendRed[ZONES] = {false, false};

// - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - 

#include <Adafruit_FONA.h>
#include <DS1337.h>           //  For the Stalker's real-time clock (RTC)
#include <Sleep_n0m1.h>       //  Sleep library
#include <SoftwareSerial.h>   //  For serial communication with FONA
#include <Wire.h>             //  I2C library for communication with RTC
//#include <I2C.h>             //  replacemnet I2C library
#include <Fat16.h>             //  FAT16 library for SD card

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


#define DOTIMES(x) for(uint8_t _i=x; _i;_i--)
#define XON  17
#define XOFF 19

#if defined(__AVR_ATmega168__)  || defined(__AVR_ATmega328P__)
  #define WRITE_EEPROM(_addr, _value) \
    { \
      while(EECR & (1<<EEPE)); \
 			EEAR = (uint16_t)(void *)(_addr); \
 			EEDR = (uint8_t)(_value); \
 			EECR |= (1<<EEMPE);\
 			EECR |= (1<<EEPE); \
    }
#else
  #define WRITE_EEPROM(_addr, _value) \
    eeprom_write_byte(_addr, _value);
#endif


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
		uint32_t nn = 0;
		uint16_t n = 0;

		Wire.begin(); //  Begin the I2C interface
		//I2c.begin(); //  Begin the I2C interface
		RTC.begin();  //  Begin the RTC        
		Serial.begin (57600);
		Serial.print (F("Tepmachcha version "));
		Serial.print (VERSION);
		Serial.print (F(" "));
		Serial.print (__DATE__);    //  Compile data and time helps identify software uploads
		Serial.print (F(" "));
		Serial.println (__TIME__);

		//analogReference (INTERNAL); // 1.1 on atmega328
		analogReference(DEFAULT);  // 3.3?
		//analogReference(EXTERNAL); // 3.3?

		//{ uint8_t i=5; while(i--) { analogRead(A7); delay(100); }}

		//for(uint8_t i=5 ;i;i--) { analogRead(A7); delay(100); }

		//DOTIMES(5) { analogRead(A7); delay(100); }


		Serial.print (F("Startup voltage "));

    // After change reference, first few readings are wrong
    /*
    for(n=8; n;n--)
		  analogRead(A7);
      */

    n = analogRead(A7);

		// Get battery reading on A7 (0-1023) in mV
		// VBAT (AREF 3.3v) is divided by a 10k/2k divider to A7, so (3.3v * ((10+2)/2) / 1.024)
		// AREF integer fraction
		// 1.1  825/128
		// 3.3  4950/256 (~155/8) 

		// Use integer math 4950/256 (~155/8) to avoid including >1.2k of FP/multiplication logic
		DOTIMES(155) nn += n;

		//for(uint8_t i=155; i;i--) nn += n;
		Serial.println (nn/8);

		//uint16_t n = analogRead(A7);
		//Serial.println (n*16 + n*4);

		
		pinMode (BEEPIN, OUTPUT);
		pinMode (RANGE, OUTPUT);
		pinMode (FONA_KEY, OUTPUT);
		pinMode (FONA_RX, OUTPUT);

		/*   If the voltage at startup is less than 3.5V, we assume the battery died in the field
		 *   and the unit is attempting to restart after the panel charged the battery enough to
		 *   do so. However, running the unit with a low charge is likely to just discharge the
		 *   battery again, and we will never get enough charge to resume operation. So while the
		 *   measured voltage is less than 3.5V, we will put the unit to sleep and wake once per 
		 *   hour to check the charge status.
		 */


		//while (analogRead (A7) < 547)   
    /*
    for(n=8; 
		while (analogRead (A7) < 180)   
		{
				Serial.println (F("Sleeping to save power..."));
				Serial.flush();
				digitalWrite (BEEPIN, HIGH);      //  Make sure XBee is powered off
				digitalWrite (RANGE, LOW);        //  Make sure sonar is off
				RTC.enableInterrupts (EveryHour); //  We'll wake up once an hour
				RTC.clearINTStatus();             //  Clear any outstanding interrupts
				attachInterrupt (RTCINT, rtcIRQ, FALLING);
				interrupts();
				sleep.pwrDownMode();                    //  Set sleep mode to Power Down
				sleep.sleepInterrupt (RTCINT, FALLING); //  Sleep; wake on falling voltage on RTC pin
		}
    */

		digitalWrite (RANGE, HIGH);          //  If set low, sonar will not range
		digitalWrite (FONA_KEY, HIGH);       //  Initial state for key pin

    // JACK
		fonaOn();
    int x = 3;
    while( x-- ) if (getFirmware()) break;
		fonaOff();
    while (1);

		//  We will use the FONA to get the current time to set the Stalker's RTC
		fonaOn();
		clockSet();
		
		// Delete any accumulated SMS messages to avoid interference from old commands
		fona.sendCheckReply (F("AT+FSDRIVE=0"), F("OK"));            //  Enter text mode
		fona.sendCheckReply (F("AT+FSLS=?"), F("OK"));            //  Enter text mode
		
		fonaOff();
		
		RTC.enableInterrupts (EveryMinute);  //  RTC will interrupt every minute
		RTC.clearINTStatus();                //  Clear any outstanding interrupts
		attachInterrupt (RTCINT, rtcIRQ, FALLING);
		interrupts();

		now = RTC.now();    //  Get the current time from the RTC
		
		//  We'll keep the XBee on for an hour after startup to assist installation
		if (now.hour() == 23)
		{
				beeShutoffHour = 0;
		}
		else
		{
				beeShutoffHour = (now.hour() + 1);
		}
		beeShutoffMinute = now.minute();

		Serial.print (F("XBee powered on until at least "));
		Serial.print (beeShutoffHour);
		Serial.print (F(":"));
		Serial.println (beeShutoffMinute);
		Serial.flush();
		smsPower = true;
}


void loop (void)
{
    return;

		now = RTC.now();      //  Get the current time from the RTC

		Serial.print (now.hour());
		Serial.print (F(":"));
		Serial.println (now.minute());

		int streamHeight = takeReading();

		/*  One failure mode of the sonar -- if, for example, it is not getting enough power -- 
		is to return the minimum distance the sonar can detect; in the case of the 10m sonars
		this is 50cm. This is also what would happen if something were to block the unit -- a
		plastic bag that blew onto the enclosure, for example. We very much want to avoid false
		positive alerts, so for the purposes of yellow and red alerts, we will ignore anything
		less than 55cm from the sensor. 
		
		Per discussions with PIN, alerts will be cleared manually, by sending an SMS to the 
		unit. */

		//  Cycle through zones to check for new yellow alerts

		for (int i = 0; i < ZONES; i++)
		{
				if (streamHeight >= yellow[i] && streamHeight < (SENSOR_HEIGHT - 55) && alert[i] == 'G')
				{
				        if (validate (yellow[i] == true))
				        {
				                sendYellow[i] = true;
				                alert[i] = 'Y';
				                upload (streamHeight);
				        }
				}
		}

		//  Do the same for red alerts

		for (int i = 0; i < ZONES; i++)
		{
				if (streamHeight >= red[i] && streamHeight < (SENSOR_HEIGHT - 55) && alert[i] != 'R')
				{
				        if (validate (red[i] == true))
				        {
				                sendRed[i] = true;
				                alert[i] = 'R';
				                upload (streamHeight);
				        }
				}
		}
		
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
		sendReading (streamHeight);

		for (int i = 0; i < ZONES; i++)
		{
				if (sendYellow[i] == true)
				{
				        Serial.print (F("Triggering yellow alert for Zone "));
				        Serial.println (i);
				        if (ivr (yellowFlow[i]) == false) //  If it doesn't work...
				        {
				                Serial.println (F("Trigger appeared to fail. Will retry in 90 seconds..."));
				                wait (90000);         //  ...wait 90 seconds...
				                ivr (yellowFlow[i]);  //  ...and try again.
				                /*   We could try repeatedly; the worry is that if
				                 *   the trigger appeared to fail on our end but
				                 *   the flow really was triggered, we could end
				                 *   up sending many messages to subscribers. 
				                 *   
				                 *   Trying twice strikes a balance between making
				                 *   sure the message went out and avoiding the
				                 *   system being seen as a nuisance.
				                 */
				        }
				        sendYellow[i] = false;
				}

				if (sendRed[i] == true)
				{
				        Serial.print (F("Triggering red alert for Zone "));
				        Serial.println (i);
				        if (ivr (redFlow[i]) == false)
				        {
				                Serial.println (F("Trigger appeared to fail. Will retry in 90 seconds..."));
				                wait (90000);
				                ivr (redFlow[i]);
				        }
				        sendRed[i] = false;
				}
		}
		
		if (noSMS == false)
		{
				checkSMS();
		}

		/*   The RTC drifts more than the datasheet says, so we'll reset the time every day at
		 *   midnight. 
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



void wait (unsigned long period)
{
		//  Non-blocking delay function
		unsigned long waitend = millis() + period;
		while (millis() <= waitend)
		{
				Serial.flush();
		}
}



boolean fonaOn()
{

		pinMode (FONA_RTS, OUTPUT);
		digitalWrite(FONA_RTS, HIGH);

		if (digitalRead (FONA_PS) == LOW)             //  If the FONA is off...
		{
				digitalWrite(FONA_KEY, HIGH);  //  KEY should be high to start

				Serial.print (F("Powering FONA on..."));
				while (digitalRead (FONA_PS) == LOW) 
				{
				        digitalWrite(FONA_KEY, LOW);  //  ...pulse the Key pin low...
				        wait (500);
				}
				digitalWrite (FONA_KEY, HIGH);        //  ...and then return it to high
				Serial.println(F(" done."));
		}
		
		Serial.println (F("Initializing FONA..."));
		
		fonaSerial.begin (4800);                      //  Open a serial interface to FONA
		
		if (fona.begin (fonaSerial) == false)         //  Start the FONA on serial interface
		{
				Serial.println (F("FONA not found. Check wiring and power."));
				return false;
		}
		else
		{
				Serial.print (F("FONA online. "));
    fona.sendCheckReply (F("AT+IFC=?"), F("OK"));
		fona.sendCheckReply (F("AT+FTPSSL?"), F("OK"));
		fona.sendCheckReply (F("AT+FTPSSL=2"), F("OK"));
		fona.sendCheckReply (F("AT+FTPSSL?"), F("OK"));
		fona.sendCheckReply (F("AT+FTPGETTOFS?"), F("OK"));
				

				unsigned long gsmTimeout = millis() + 30000;
				boolean gsmTimedOut = false;

				Serial.print (F("Waiting for GSM network... "));
				while (1)
				{
				        byte network_status = fona.getNetworkStatus();
				        if(network_status == 1 || network_status == 5) break;
				        
				        if(millis() >= gsmTimeout)
				        {
				                gsmTimedOut = true;
				                break;
				        }
				        
				        //wait (250);
				        wait (500);
				}

				if(gsmTimedOut == true)
				{
				        Serial.println (F("timed out. Check SIM card, antenna, and signal."));
				        return false;
				}
				else
				{
				        Serial.println(F("done."));
				}

				//  RSSI is a measure of signal strength -- higher is better; less than 10 is worrying
				byte rssi = fona.getRSSI();
				Serial.print (F("RSSI: "));
				Serial.println (rssi);

				wait (3000);    //  Give the network a moment

				fona.setGPRSNetworkSettings (F("cellcard"));    //  Set APN to your local carrier

				if (rssi > 5)
				{
				        if (fona.enableGPRS (true) == false);
				        {
				              //  Sometimes enableGPRS() returns false even though it succeeded
				              if (fona.GPRSstate() != 1)
				              {
				                      for (byte GPRSattempts = 0; GPRSattempts < 10; GPRSattempts++)
				                      {
				                            Serial.println (F("Trying again..."));
				                            wait (2000);
				                            fona.enableGPRS (true);
				                            
				                            if (fona.GPRSstate() == 1)
				                            {
				                                    Serial.println (F("GPRS is on."));
				                                    break;
				                            }
				                            else
				                            {
				                                    Serial.print (F("Failed to turn GPRS on... "));
				                            }
				                      }
				              }
				        }
				}
				else
				{
				        Serial.println (F("Can't transmit, network signal strength is poor."));
				        gsmTimedOut = true;
				}
				
				return true;
		}
}



void clockSet (void)
{
		wait (1000);    //  Give time for any trailing data to come in from FONA

		int netOffset;

		char theDate[17];

		Serial.println (F("Attempting to get time from GSM location service..."));

		flushFona();    //  Flush any trailing data

		fona.sendCheckReply (F("AT+CIPGSMLOC=2,1"), F("OK"));    //  Query GSM location service for time
		
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
				Serial.println (F("GSM location service failed."));
				/*   ...the we'll get time from the NTP pool instead: 
				 *   (https://en.wikipedia.org/wiki/Network_Time_Protocol)
				 */
				fona.enableNTPTimeSync (true, F("0.daimakerlab.pool.ntp.org"));
				Serial.println (F("Attempting to enable NTP sync."));
				
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
				sprintf (theDate, "%d/%d/%d %d:%d", netDay, netMonth, netYear, netHour, netMinute);
				Serial.println (theDate);
				
				Serial.println(F("Adjusting RTC."));
				DateTime dt(netYear, netMonth, netDay, netHour, netMinute, netSecond, 0);
				RTC.adjust(dt);     //  Adjust date-time as defined above
		}
		else
		{
				Serial.println (F("Didn't find reliable time. Will continue to use RTC's current time."));
				method = 'X';
		}

		wait (200);              //  Give FONA a moment to catch its breath
}



void flushFona (void)
{
		// Read all available serial input from FONA to flush any pending data.
		while(fona.available())
		{
				char c = fona.read();
				Serial.print (c);
		}
}



void fonaOff (void)
{
		wait (5000);        //  Shorter delays yield unpredictable results

		//  We'll turn GPRS off first, just to have an orderly shutdown
		if (fona.enableGPRS (false) == false)
		{
				if (fona.GPRSstate() == 1)
				{
				        Serial.println (F("Failed to turn GPRS off."));
				}
				else
				{
				        Serial.println (F("GPRS is off."));
				}
		}
	  
		wait (500);
	  
		// Power down the FONA if it needs it
		if (digitalRead (FONA_PS) == HIGH)      //  If the FONA is on...
		{
				fona.sendCheckReply (F("AT+CPOWD=1"), F("OK")); //  ...send shutdown command...
				digitalWrite (FONA_KEY, HIGH);                  //  ...and set Key high
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



boolean sendReading (int streamHeight)
{
		char url[255];

		/*   A common error mode of the sonar is to return a stream height that is
		 *   at the minimum (50cm) or maximum (10m) range of the sensor. Because we
		 *   plan to trigger alerts from the cloud data in future, we will want to
		 *   filter out those readings so they do not trigger an alert. The commented
		 *   code below would do that -- the server would need to recognize 99999 as
		 *   an error condition.
		
		if (streamHeight > (SENSOR_HEIGHT - 55))
		{
				streamHeight = 99999;
		}
		 */
		
		unsigned int voltage;
		fona.getBattVoltage (&voltage);   //  Read the battery voltage from FONA's ADC

		/*    We'll read the solar charging status so that we can diagnose any charging issues.
		 *    This will only tell us if it's charging (1) or not (0), not the strength of the
		 *    charge.
		 *    
		 *    In testing, this seemed to not always yield an accurate result; don't panic if
		 *    you get a 0 result when it should be charging.
		 */
		int solar;
		if (analogRead (6) <= 900 && analogRead (6) > 550)
		{
				solar = 1;
		}
		else
		{
				solar = 0;
		}

		//  Generate the HTTP GET URL for the Phant server 
		sprintf (url, "data.sparkfun.com/input/%s?private_key=%s&1_streamheight=%d&2_charging=%d&3_voltage=%d", PUBLIC_KEY, PRIVATE_KEY, streamHeight, solar, voltage);
		Serial.print (F("Sending: "));
		Serial.println (url);

		unsigned int httpStatus;
		unsigned int datalen;

		boolean success = false;
		int attempts = 0;

		wait (7500);    //  Seems to greatly improve reliability on metfone and Cellcard
		//wait (1000);

		while (success == false && attempts < 5)   //  We'll attempt up to five times to upload data
		{
				fona.HTTP_GET_start (url, &httpStatus, &datalen);
				fona.HTTP_GET_end();
		
				if (httpStatus == 200)    //  If the HTTP GET request returned a 200, it succeeded
				{
				        Serial.println (F("Upload succeeded."));
				        success = true;
				}
				else
				{
				        Serial.println (F("Upload failed."));
				        success = false;
				}

				/*   Occasionally the FONA returns something other than a 200 even though the upload
				 *   succeeded -- maybe that's coming from the server and maybe it isn't -- and so 
				 *   you get a duplicate reading sent to Phant. It's not very common, but it does happen.
				 */

				attempts++;

				wait (1000);
		}

		return success;
}



const uint8_t CHIP_SELECT = SS;  // SD chip select pin.
SdCard card;
Fat16 file;

void writeNumber(uint32_t n) {
  uint8_t buf[11];
  uint8_t i = 0;
  do {
	i++;
	buf[sizeof(buf) - i] = n%10 + '0';
	n /= 10;
  } while (n);
  file.write(&buf[sizeof(buf) - i], i); // write the part of buf with the number
}


char file_name[] = "FIRMWARE.HEX";
boolean fat_init() {

  // init sd card
  if (!card.begin(CHIP_SELECT)) {
    Serial.print(F("failed card.begin "));
    Serial.println(card.errorCode);
	  return false;
  } else {
	  Serial.println(F("card.begin"));
  }
  
  // initialize a FAT16 volume
  if (!Fat16::init(&card)) {
	  Serial.println(F("failed Fat16::init"));
	  return false;
  } else {
	  Serial.println(F("Fat16::init"));
  }

  // create a new file
  /*
  char name[] = "FIRMWARE.HEX";
  for (uint8_t i = 0; i < 1000; i++) {
	name[5] = i/100 + '0';
	name[6] = i/10 + '0';
	name[7] = i%10 + '0';
	// O_CREAT - create the file if it does not exist
	// O_EXCL - fail if the file exists
	// O_WRITE - open for write
	  if (file.open(name, O_CREAT | O_EXCL | O_WRITE)) break;
  }
  */
  file.open(file_name, O_CREAT | O_WRITE);
  Serial.println(file_name);
  return true;
}


  /*
  if (!file.isOpen()) {
	Serial.println("failed file.open");
	//return;
  } else {
	Serial.println("file.open");
  }
  Serial.println("Writing to: ");
  Serial.println(name);
  
  // write 100 line to file
  for (uint16_t i = 0; i < 3000; i++) {
	file.write("line "); // write string from RAM
	writeNumber(i);
	file.write_P(PSTR(" millis = ")); // write string from flash
	writeNumber(millis());
	file.write("\r\n"); // file.println() would work also
  }
  // close file and force write of all data to the SD card
  file.close();
  Serial.println("Done");
}
*/



/*
SdFat SD;
File myFile;

file_stuff() {
  if (!SD.begin(SS)) {
	Serial.println("initialization failed!");
	return;
  }
  myFile = SD.open("test.txt", FILE_WRITE);
  if (myFile) {
	Serial.print("Writing to test.txt...");
	myFile.println("testing 1, 2, 3.");
	// close the file:
	myFile.close();
	Serial.println("done.");
  } else {
	// if the file didn't open, print an error:
	Serial.println("error opening test.txt");
  }
}
*/


boolean getFirmware()
{ 
  unsigned int httpStatus = 0;
  unsigned int datalen = 0;
  char url[128];

  boolean success = false;
  int attempts = 0;



  //file_stuff();
  if (!fat_init()) {
    //return;
  }

  if (!file.isOpen()) {
	  Serial.println(F("file open failed"));
  }

    //fonaOn();
				//fonaSerial.write(XON);
		//pinMode (FONA_RTS, OUTPUT);
		//digitalWrite(FONA_RTS, HIGH);
				//fona.sendCheckReply (F("AT+IFC=2,2"), F("OK"));     // set RTS

  Serial.println(F("fetching firmware"));

  //sprintf (url, "data.sparkfun.com/input/%s?private_key=%s&1_streamheight=%d&2_charging=%d&3_voltage=%d", PUBLIC_KEY, PRIVATE_KEY, streamHeight, solar, voltage);
  //sprintf (url, "http://csb.stanford.edu/class/public/pages/sykes_webdesign/05_simple.html");
  //sprintf (url, "https://hckrnews.com/");
  //sprintf (url, "http://hckrnews.com/");

		fona.sendCheckReply (F("AT+FTPGETTOFS=?"), F("OK"));
/*
AT+FTPSERV=<value>
AT+FTPUN=<value>
AT+FTPPW=<value>
AT+FTPGETNAME=<value>
AT+FTPGETPATH=<value>
AT+FTPGET ->  +FTPGET:1,0
AT+FTPSSL=2
*/



		fona.sendCheckReply (F("AT+HTTPINIT"), F("OK"));
		fona.sendCheckReply (F("AT+HTTPPARA=\"CID\",1"), F("OK"));
		//fona.sendCheckReply (F("AT+HTTPSSL=1"), F("OK"));   //  RapidPro requires SSL
		fona.sendCheckReply (F("AT+HTTPPARA=\"REDIR\",\"1\""), F("OK"));
		fona.sendCheckReply (F("AT+HTTPPARA=\"UA\",\"FONA\""), F("OK"));

    //fona.sendCheckReply (F("AT+HTTPPARA=\"URL\",\"csb.stanford.edu/class/public/pages/sykes_webdesign/05_simple.html\""), F("OK"));
		//fona.sendCheckReply (F("AT+HTTPPARA=\"URL\",\"drive.google.com/open?id=0B50UoDy6jxg3NkxheUduMVJFekU\""), F("OK"));
		fona.sendCheckReply (F("AT+HTTPPARA=\"URL\",\"jackbyte.fastmail.fm/pin/blink.hex\""), F("OK"));

    //HTTP_para(F("UA"), fona.useragent);
    //fona.HTTP_ssl(true);
    //fona.HTTP_setup(url);

		//fona.sendCheckReply (F("AT+HTTPPARA=\"BREAK\",\"60000\""), F("OK"));
		//fona.sendCheckReply (F("AT+HTTPPARA=\"BREAK\",\"100\""), F("OK"));

    uint8_t tries;
    for (tries = 5; tries > 0; tries--) {
      fona.HTTP_action(0, &httpStatus, &datalen, 30000);
      //fona.sendCheckReply (F("AT+HTTPACTION=\"0\""), F("OK"));


      Serial.print("status ");
      Serial.println(httpStatus);
      Serial.print("size ");
      Serial.println(datalen);
      if (httpStatus == 200) break;
      delay(1000);
    }

        /*
        while (fona.available()) {
          char c = fona.read();
          // Serial.write is too slow, we'll write directly to Serial register!
#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
          loop_until_bit_is_set(UCSR0A, UDRE0); // Wait until data register empty
          UDR0 = c;
#else
          Serial.write(c);
#endif
        }
        */


    fona.HTTP_readall(datalen);

  //if (!fona.HTTP_GET_start (url, &httpStatus, &datalen)) {
    //Serial.print("Failed! - ");
    //Serial.println(httpStatus);
	  //return false;
  //}

  //fona.HTTP_GET_start (url, &httpStatus, &datalen);
  //fona.HTTP_GET_end();
		
  if (httpStatus == 200)    //  If the HTTP GET request returned a 200, it succeeded
  {
    Serial.println(F("http OK"));
    Serial.println(F(">>>>>"));

    //if (false) {
    //if (file.isOpen()) {
      //Serial.print(F("writing "));
      uint8_t n = 0;
      while (datalen > 0) {
        if (fona.available()) {

          url[n++] = fona.read();

          if (n == 32) {
            //fonaSerial.write(XOFF);
            digitalWrite(FONA_RTS, LOW); // XOFF

            n = 0;
            file.write(url, 32);
            loop_until_bit_is_set(UCSR0A, UDRE0); // Wait until data register empty.
            UDR0 = '.';
            //delay(500);

            //fonaSerial.write(XON);
            digitalWrite(FONA_RTS, HIGH); // XON
          }

          datalen--;
        }

      }
      if (n) file.write(url, n); // write remaining buffer

      Serial.println(F("\n****"));
      file.close();
      int x;
      for (x = 0; x < 8; x++) {
        WRITE_EEPROM((1022-x), file_name[x])
      }
      WRITE_EEPROM(1012, 'X');
      WRITE_EEPROM(1013, 'E');
      WRITE_EEPROM(1014, 'H');

      /*
      WRITE_EEPROM(1015, 'E');
      WRITE_EEPROM(1016, 'R');
      WRITE_EEPROM(1017, 'A');
      WRITE_EEPROM(1018, 'W');
      WRITE_EEPROM(1019, 'M');
      WRITE_EEPROM(1020, 'R');
      WRITE_EEPROM(1021, 'I');
      WRITE_EEPROM(1022, 'F');
      */
      WRITE_EEPROM(1023, 1);
         
	WDTCSR = _BV(WDE);
	while (1); // 16 ms

      success = true;

    } else {
    }

      /*
      while (datalen > 0) {
        while (fona.available()) {
          delay(20);
          char c = fona.read();

          // Serial.write is too slow, we'll write directly to Serial register!
#if defined(__AVR_ATmega328P__) || defined(__AVR_ATmega168__)
          loop_until_bit_is_set(UCSR0A, UDRE0); // Wait until data register empty
          UDR0 = c;
#else
          Serial.write(c);
#endif
          datalen--;
          if (! datalen) break;
        }
      }
      */

  Serial.println(F("\n****"));

  fona.HTTP_GET_end();
  //fonaOff();

  // set eeprom filename, toggle, and reboot
  return success;
}




boolean ivr (const char* flow)
{
		Serial.print (F("Triggering flow with UUID "));
		Serial.println (flow);
		
		//  Manually construct the HTTP POST headers necessary to trigger the RapidPro flow
		fona.sendCheckReply (F("AT+HTTPINIT"), F("OK"));
		fona.sendCheckReply (F("AT+HTTPSSL=1"), F("OK"));   //  RapidPro requires SSL
		fona.sendCheckReply (F("AT+HTTPPARA=\"URL\",\"push.ilhasoft.mobi/api/v1/runs.json\""), F("OK"));
		fona.sendCheckReply (F("AT+HTTPPARA=\"REDIR\",\"1\""), F("OK"));
		fona.sendCheckReply (F("AT+HTTPPARA=\"CONTENT\",\"application/json\""), F("OK"));
		
		fona.print (F("AT+HTTPPARA=\"USERDATA\",\"Authorization: Token "));
		fona.print (APITOKEN);
		fona.println (F("\""));
		Serial.print (F("AT+HTTPPARA=\"USERDATA\",\"Authorization: Token "));
		Serial.print (APITOKEN);
		Serial.println (F("\""));
		
		fona.expectReply (F("OK"));

		int dataSize = (strlen(flow) + strlen(TARGETCONTACT) + 32);

		fona.print (F("AT+HTTPDATA="));
		fona.print (dataSize);
		fona.println (F(",2000"));
		fona.expectReply (F("OK"));
		
		fona.print (F("{\"flow_uuid\": \""));
		fona.print (flow);
		fona.print (F("\",\"contact\": \""));
		fona.print (TARGETCONTACT);
		fona.println (F("\"}"));
		fona.expectReply (F("OK"));

		Serial.print (F("{\"flow_uuid\": \""));
		Serial.print (flow);
		Serial.print (F("\",\"contact\": \""));
		Serial.print (TARGETCONTACT);
		Serial.println (F("\"}"));

		uint16_t statusCode;
		uint16_t dataLen;

		fona.HTTP_action (1, &statusCode, &dataLen, 10000);   //  Send the POST request we've constructed

		while (dataLen > 0)
		{
				while (fona.available())
				{
				        char c = fona.read();
				        loop_until_bit_is_set (UCSR0A, UDRE0); 
				        UDR0 = c;
				}

				dataLen--;
				if (!dataLen)
				{
				        break;
				}
		}
		
		Serial.print (F("Status code: "));
		Serial.println (statusCode);
		Serial.print (F("Reply length: "));
		Serial.println (dataLen);
		
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



void checkSMS (void)
{
		//  Check SMS messages received for any valid commands
		
		char smsBuffer[255];
		char smsSender[20];
		unsigned int smsLen;
		char smsMsg[57];
		boolean sendStatus = false;
		int NumSMS;

		flushFona();    //  Flush out any unresolved data

		/*   During testing on metfone, sometimes the FONA had to be on for more
		 *   than 60 seconds(!) before it would receive a previously sent SMS message,
		 *   so we'll keep looking for 60 seconds.
		 */

		unsigned long smsTimeout = millis() + 60000;

		Serial.println (F("Checking for SMS messages..."));

		do
		{
				NumSMS = fona.getNumSMS();
				wait (5000);
		}
		while (NumSMS == 0 && millis() <= smsTimeout);
		
		Serial.print (NumSMS);
		Serial.println (F(" message(s) waiting."));

		unsigned long timeOut = (millis() + 60000);
		
		while (NumSMS > 0)          //  If there are messages...
		{
				fona.readSMS (NumSMS, smsBuffer, 250, &smsLen);  //  ...retrieve the last one...
				
				wait (500);                                      //  ...give FONA a moment to catch up...
				
				fona.getSMSSender (NumSMS, smsSender, 250);      //  ...find out who send it...
				
				wait (500);

				Serial.print (F("Message from "));
				Serial.print (smsSender);
				Serial.println (F(":"));
				Serial.println (smsBuffer);

				//  Now check to see if any of the declared passwords are in the SMS message and respond accordingly
				if (strcmp (smsBuffer, BEEPASSWORD) == 0)        //  If the message is the XBee password...
				{
				        //  ...determine the appropriate shutoff time and turn on the XBee until then
				        
				        if (now.hour() == 23)
				        {
				                beeShutoffHour = 0;
				        }
				        else
				        {
				                beeShutoffHour = now.hour() + 1;    //  We'll leave the XBee on for 1 hour
				        }
				        
				        beeShutoffMinute = now.minute();
				        
				        digitalWrite (BEEPIN, LOW);        //  Turn on the XBee

				        char leadingZero[3];
				        if (beeShutoffMinute < 10)         //  Add a leading zero to the minute if necessary
				        {
				                sprintf (leadingZero, ":0");
				        }
				        else
				        {
				                sprintf (leadingZero, ":");
				        }

				        //  Compose a reply to the sender confirming the action and giving the shutoff time
				        sprintf (smsMsg, "XBee on until %d%s%d", beeShutoffHour, leadingZero, beeShutoffMinute);
				        
				        Serial.println (F("XBee turned on by SMS."));
				        smsPower = true;                    //  Raise the flag 
				        fona.sendSMS(smsSender, smsMsg);    //  Tell the sender what you've done
				}

				int redComparator = strlen (CLEARRED);

				if (strncmp (smsBuffer, CLEARRED, redComparator) == 0)    //  If the command is to clear red status...
				{
				        int stringLength = strlen (smsBuffer);
				        char* zoneNumber;
				        strncpy (zoneNumber, smsBuffer + redComparator, (stringLength - redComparator));
				        int zoneToClear = atoi (zoneNumber);
				        
				        if (alert[zoneToClear] == 'R')     //  ...and the status is indeed red...
				        {
				                alert[zoneToClear] = 'Y';  //  ...downgrade it to yellow.
				        }

				        Serial.print (F("Clearing red alert, Zone "));
				        Serial.print (zoneToClear);
				        Serial.println (F("..."));

				        sendStatus = true;
				}

				int yellowComparator = strlen (CLEARYELLOW);

				if (strncmp (smsBuffer, CLEARYELLOW, yellowComparator) == 0)    //  If the command is to clear red status...
				{
				        int stringLength = strlen (smsBuffer);
				        char* zoneNumber;
				        strncpy (zoneNumber, smsBuffer + yellowComparator, (stringLength - yellowComparator));
				        int zoneToClear = atoi (zoneNumber);
				        
				        if (alert[zoneToClear] == 'Y')
				        {
				                alert[zoneToClear] = 'G';  //  Downgrade it to green
				        }

				        Serial.print (F("Clearing yellow alert, Zone "));
				        Serial.print (zoneToClear);
				        Serial.println (F("..."));

				        sendStatus = true;
				}
			  
				wait (1000);
				fona.deleteSMS (NumSMS);
				wait (1500);
				NumSMS = fona.getNumSMS();

				//  Occasionally messages won't delete and this loops forever. If
				//  the process takes too long we'll just nuke everything.
				if (millis() >= timeOut)
				{
				        fona.sendCheckReply (F("AT+CMGF=1"), F("OK"));            //  Enter text mode
				        fona.sendCheckReply (F("AT+CMGDA=\"DEL ALL\""), F("OK")); //  Delete all SMS messages                }
				}
		}

		//  If we have changed the alert status, send a confirmation SMS
		if (sendStatus == true)
		{
				char alertStatus[6][ZONES];
				char zoneMessage[13][ZONES];

				for (int i = 0; i < ZONES; i++)
				{
				        switch (alert[i])
				        {
				                case 'G': sprintf (alertStatus[i], "green");  break;
				                case 'Y': sprintf (alertStatus[i], "yellow"); break;
				                case 'R': sprintf (alertStatus[i], "red");    break;
				                default: sprintf (alertStatus[i], "unknown"); break;
				        }

				        int attempts = 0;

				        sprintf (zoneMessage[i], "Zone %d: %s. ", i, alertStatus[i]);
				        while (fona.sendSMS (smsSender, zoneMessage[i]) == false && attempts < 3)
				        {
				                attempts++;
				        }

				        sendStatus = false;
				}
		}
}



boolean validate (int alertThreshold)
{
  /*   False positives would undermine confidence in the IVR alerts, so we must take
   *   pains to avoid them. Before triggering an Ilhapush alert flow, we will validate
   *   the reading by taking five readings and making sure they _all_ agree. If the
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
