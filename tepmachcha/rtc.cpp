#include "tepmachcha.h"

DateTime now;
DS1337 RTC;         //  Create the DS1337 real-time clock (RTC) object


uint8_t daysInMonth(uint8_t month)  // month 1..12
{
  static uint8_t const daysInMonthP [] PROGMEM = { 31,28,31,30,31,30,31,31,30,31,30,31 };
  return pgm_read_byte(daysInMonth + month - 1);
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

		if (netYear < 2017 || netYear > 2050)  // If that still didn't work...
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


    if (netYear > 2000) { netYear -= 2000; }  // Adjust from YYYY to YY
		if (netYear > 17 && netYear < 50 && netHour < 24)  // Date looks valid
		{
				// Adjust UTC to local time
        int8_t localhour = (netHour + UTCOFFSET);
				if ( localhour < 0)                   // TZ takes us back a day
				{
				    netHour = localhour + 24;         // hour % 24
            if (--netDay == 0)                // adjust the date to UTC - 1
            {
              if (--netMonth == 0)
              {
                netMonth = 12;
                netYear--;
              }
              netDay = daysInMonth(netMonth);
            }
				}
				else if (localhour > 23)              // TZ takes us to the next day
        {
            netHour = localhour - 24;         // hour % 24
            if (++netDay > daysInMonth(netMonth)) // adjust the date to UTC + 1
            {
              if (++netMonth > 12)
              {
                 netMonth = 1;
                 netYear++;
              }
              netDay = 1;
            }
        }
        else                                  // TZ is same day
        {
            netHour = localhour;              // simply add TZ offset
        }

				Serial.print (F("Net time: "));
				sprintf_P(theDate, (prog_char*)F("%d/%d/%d %d:%d"), netDay, netMonth, netYear, netHour, netMinute);
				Serial.print (theDate);

				Serial.println(F(". Adjusting RTC"));
				DateTime dt(netYear, netMonth, netDay, netHour, netMinute, netSecond, 0);
				RTC.adjust(dt);     //  Adjust date-time as defined above
		}
		else
		{
				Serial.println (F("Sync failed, using RTC"));
		}

		wait (200);              //  Give FONA a moment to catch its breath
}


/*
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
		wait (2000);    //  Give time for any trailing data to come in from FONA

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
*/


/*
boolean fonaReadTime(DateTime *dt)
{
    uint16_t y;
    uint8_t m, d, hh, mm, ss;

    y = fona.parseInt();
    m = fona.parseInt();
    d = fona.parseInt();
    hh = fona.parseInt();
    mm = fona.parseInt();
    ss = fona.parseInt();

    if (y > 2000) { y -= 2000; }  // Adjust from YYYY to YY

		if (y < 18 || y > 50 || hh > 23 || mm > 59 || ss > 59)
      return false;

    DateTime tmp(y, m, d, hh, mm, ss, 0);
    *dt = tmp;
    return true;
}


boolean fonaGsmLoc(void)
{
    return fona.sendCheckReply (F("AT+CIPGSMLOC=2,1"), OK);    //  Query GSM location service for time
		fona.parseInt();                    //  Ignore first int - should be status (0)
}


void clockSet3 (void)
{
    boolean status;
    DateTime dt;

		Serial.println (F("Fetching GSM time"));
		wait (1000);      //  Give time for any trailing data to come in from FONA
		fonaFlush();      //  Flush any trailing data

    fonaGsmLoc();
		fona.parseInt();  //  Ignore second int -- necessary on some networks/towers
    if (!(status = fonaReadTime(&dt)))
    {
      fonaGsmLoc();   // try again without ignoring second int
      status = fonaReadTime(&dt);
    }
    if (!status)
    {
      fona.enableNTPTimeSync (true, F("0.daimakerlab.pool.ntp.org"));
      Serial.println (F("GSM location failed, trying NTP sync"));
      
      wait (15000);             // Wait for NTP server response
      
      //AT+CCLK="31/12/00,17:59:59+22"
      fona.println (F("AT+CCLK?")); // Query FONA's clock for resulting NTP time              
      status = fonaReadTime(&dt);
    }
    if (status)
    {
      now = dt;
      RTC.adjust(dt);   //  Adjust date-time as defined above
    }
}
*/
