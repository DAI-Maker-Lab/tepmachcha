#include "tepmachcha.h"

SoftwareSerial fonaSerial = SoftwareSerial (FONA_TX, FONA_RX);
Adafruit_FONA fona = Adafruit_FONA (FONA_RST);
DateTime now;
DS1337 RTC;         //  Create the DS1337 real-time clock (RTC) object

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
        DAY--;
        HOUR = LOCALTIME + 24;
      }
      else if ( LOCALTIME > 23 ) // TZ takes us to next day
      {
        DAY++;
        HOUR = LOCALTIME - 24;
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
		
		fona.parseInt();                    //  Ignore first int - should be status (0)
		int secondInt = fona.parseInt();    //  Ignore second int -- necessary on some networks/towers
		int netYear = fona.parseInt();      //  Get the results -- GSMLOC year is 4-digit
		int netMonth = fona.parseInt();
		int netDay = fona.parseInt();
		int netHour = fona.parseInt();      //  GSMLOC is _supposed_ to get UTC; we will adjust
		int netMinute = fona.parseInt();
		int netSecond = fona.parseInt();    //  Our seconds may lag slightly, of course

		if (netYear < 2017 || netYear > 2050 || netHour > 23) //  If that obviously didn't work...
		{
				Serial.println (F("Recombobulating..."));

				netSecond = netMinute;  //  ...shift everything up one to capture that second int
				netMinute = netHour;
				netHour = netDay;
				netDay = netMonth;
				netMonth = netYear;
				netYear = secondInt;
		}

		if (netYear < 2017 || netYear > 2050 || netHour > 23)   // If that still didn't work...
		{
				// get time from the NTP pool instead: 
				// (https://en.wikipedia.org/wiki/Network_Time_Protocol)
        //
				fona.enableNTPTimeSync (true, F("0.daimakerlab.pool.ntp.org"));
				Serial.println (F("GSM location failed, trying NTP sync"));
				
				wait (15000);                 // Wait for NTP server response
				
				fona.println (F("AT+CCLK?")); // Query FONA's clock for resulting NTP time              
        //AT+CCLK="31/12/00,17:59:59+22"
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

static uint8_t const daysInMonth [] PROGMEM = { 31,28,31,30,31,30,31,31,30,31,30,31 };

    if (netYear > 2000) { netYear -= 2000; }  // Adjust from YYYY to YY
		if (netYear >= 17 && netYear < 50)        // Date looks valid
		{
				// Adjust UTC to local time
        int8_t localtime = (netHour + UTCOFFSET);
				if ( localtime < 0)                   // TZ takes us back a day
				{
				    netHour = localtime + 24;         // hour % 24
            if (!netDay--)                    // adjust the date to UTC - 1
            {
              if (!netMonth--)
              {
                netMonth = 12;
                netYear--;
              }
              netDay = daysInMonth[netMonth];
            }
				}
				else if (localtime > 23)              // TZ takes us to the next day
        {
            netHour = localtime - 24;         // hour % 24
            if (netDay++ > daysInMonth[netMonth]) // adjust the date to UTC + 1
            {
              netDay = 1;
              if (netMonth > 12)
              {
                 netMonth = 1;
                 netYear++;
              }
            }
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
				Serial.println (F("Time sync failed, using RTC"));
				method = 'X';
		}

		wait (200);              //  Give FONA a moment to catch its breath
}


void fonaReadTime(DateTime *dt)
{
    uint16_t y;
    uint8_t m, d, hh, mm, ss;

    y = fona.parseInt();

    m = fona.parseInt();
    d = fona.parseInt();
    hh = fona.parseInt();
    mm = fona.parseInt();
    ss = fona.parseInt();
}


clockUTC(DateTime *dt)
{
}


void clockSet3 (void)
{
}


void fonaToggle(boolean state)
{
  uint32_t timeout = millis() + 4000;

  if (digitalRead (FONA_PS) != state) // If FONA not in desired state
  {
    digitalWrite(FONA_KEY, HIGH);     // KEY should be high to start

    Serial.print (F("FONA toggle .. "));
    while (digitalRead (FONA_PS) != state) 
    {
      digitalWrite(FONA_KEY, LOW);    // pulse the Key pin low
      wait (500);
      digitalWrite (FONA_KEY, HIGH);  // and then return it to high
      if (millis() > timeout)
        break;
    }
    Serial.println(F(" done."));
  }
}


void fonaOff (void)
{
  //wait (5000);        // Shorter delays yield unpredictable results
  wait (1000);
  fonaGPRSOff();      // turn GPRS off first, for an orderly shutdown

  wait (500);

  if (digitalRead (FONA_PS) == HIGH)           //  If the FONA is on
  {
    fona.sendCheckReply (F("AT+CPOWD=1"), OK); //  send shutdown command
    if (digitalRead (FONA_PS) == HIGH)         //  If the FONA is still on
    {
      fonaToggle(LOW);
    }
  }
}



boolean fonaPowerOn(void)
{
  if (digitalRead (FONA_PS) == LOW)  //  If the FONA is off
  {
    fonaToggle(HIGH);
  }
  return (digitalRead(FONA_PS) == HIGH);
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

  // RSSI is a measure of signal strength -- higher is better; less than 10 is worrying
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
  if (fonaPowerOn())
  {
    if (fonaSerialOn())
    {
      Serial.print (F("FONA online. "));
      if ( fonaGSMOn() )
      {
        return fonaGPRSOn();
      }
    }
  }
  return false;
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
  // read from fona, waiting up to <timeout> ms for something at arrive
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


uint16_t fonaBattery(void) {
    uint16_t voltage;
    fona.getBattVoltage (&voltage);   //  Read the battery voltage from FONA's ADC
    if (! (voltage < 6000 && voltage > 1000))
      fona.getBattVoltage (&voltage); // try one more time

    return voltage;
}


void smsDeleteAll(void)
{
  fona.sendCheckReply (F("AT+CMGF=1"), OK);            //  Enter text mode
  fona.sendCheckReply (F("AT+CMGDA=\"DEL ALL\""), OK); //  Delete all SMS messages                }
}


char *parseFilename(char *b)
{
    uint8_t i;

    DEBUG_RAM

    while (*b == ' ') b++; // skip spaces

    // copy into file_name
    for ( i = 0; i < 12 && b[i] && b[i] != ' ' ; i++) {
      file_name[i] = b[i];
    }
    file_name[i] = 0;      // terminate string
    return b+i;            // return postition of char after file_name
}

smsParse()
{
}


#define SIZEOF_SMS 80
#define SIZEOF_SMS_SENDER 18
//  Check SMS messages received for any valid commands
void smsCheck (void)
{
		char smsBuffer[SIZEOF_SMS];
		char smsSender[SIZEOF_SMS_SENDER];
		uint32_t timeout = (millis() + 60000);
		int8_t NumSMS;
		uint16_t smsLen;

		fonaFlush();    //  Flush out any unresolved data
		Serial.println (F("Checking for SMS messages..."));

		do {
				NumSMS = fona.getNumSMS();    // -1 for error
				wait (5000);
		} while (NumSMS == 0 && millis() <= timeout);

		Serial.print (NumSMS);
		Serial.println (F(" message(s) waiting."));

    // For each SMS message
		while (NumSMS > 0)
		{
				fona.readSMS (NumSMS, smsBuffer, sizeof(smsBuffer)-1, &smsLen);  // retrieve the most recent one
				wait (500);                                                      // required delay

				fona.getSMSSender (NumSMS, smsSender, sizeof(smsSender)-1);      // get sender
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

            char *b = parseFilename( smsBuffer + sizeof(FOTAPASSWORD) );

            while (*b == ' ') b++;     // skip spaces

            // read file size
            file_size = 0;
            while (*b >= '0' && *b <= '9') {
              file_size = (file_size * 10) + (*b - '0');
              b++;
            }

            Serial.print(F("filename:")); Serial.println(file_name);
            Serial.print(F("size:")); Serial.println(file_size);

            uint8_t status;
            if (!(status = firmwareGet()))   // If at first we dont succeed
            {
              fonaOn();                      // try again
              status = firmwareGet();
            }

            sprintf_P(smsBuffer, (prog_char *)F("ftp %s (%d) ok:%d, err:%d, crc:%d"), \
              file_name, file_size, status, error, 0);
            fona.sendSMS(smsSender, smsBuffer);  // return file stat, status
        }

        // FLASHPASSWD <filename>
				if (strncmp_P(smsBuffer, (prog_char*)F(FLASHPASSWORD), sizeof(FLASHPASSWORD)-1) == 0) //  FOTA password...
        {
            parseFilename( smsBuffer + sizeof(FLASHPASSWORD) );
            eepromWrite();
            reflash();
        }

        // PINGPASSWORD
				if (strcmp_P(smsBuffer, (prog_char*)F(PINGPASSWORD)) == 0)        //  PING password...
        {
            sprintf_P(smsBuffer, (prog_char *)F(DEVICE " v:%d c:%d h:%d/" STR(SENSOR_HEIGHT)), \
              batteryRead(), solarCharging(), takeReading());
            fona.sendSMS(smsSender, smsBuffer);
        }

        // BEEPASSWORD
				if (strcmp_P(smsBuffer, (prog_char*)F(BEEPASSWORD)) == 0)        //  XBee password...
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
				if (millis() >= timeout)
				{
          smsDeleteAll();
          break;
				}
		}
}
