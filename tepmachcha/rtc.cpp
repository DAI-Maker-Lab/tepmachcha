#include "tepmachcha.h"

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
            netHour = localtime;              // simply add TZ offset
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
