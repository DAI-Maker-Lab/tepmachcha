#include "tepmachcha.h"

boolean sentData = false;
boolean smsPower = false;       //  Manual XBee power flag
boolean noSMS = false;          //  Flag to turn off SMS checking -- for future use
boolean timeReset = false;      //  Flag indicating whether midnight time reset has already occurred
byte beeShutoffHour = 0;        //  Hour to turn off manual power to XBee
byte beeShutoffMinute = 0;      //  Minute to turn off manual power to XBee
char method;                    //  Method of clock set, for debugging

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

		Serial.println (F(DEVICE));

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
#ifdef BUS_PWR
		pinMode (BUS_PWR, OUTPUT);
#endif

    // Set RTC interrupt handler
		attachInterrupt (RTCINTA, rtcIRQ, FALLING);
		interrupts();

    digitalWrite (RANGE, LOW);           //  sonar off
		digitalWrite (SD_POWER, HIGH);       //  SD card off
    digitalWrite (BEEPIN, LOW);          //  XBee on
		digitalWrite (FONA_KEY, HIGH);       //  Initial state for key pin
#ifdef BUS_PWR
    digitalWrite (BUS_PWR, HIGH);        //  Peripheral bus on
#endif


		/*  If the voltage at startup is less than 3.5V, we assume the battery died in the field
		 *  and the unit is attempting to restart after the panel charged the battery enough to
		 *  do so. However, running the unit with a low charge is likely to just discharge the
		 *  battery again, and we will never get enough charge to resume operation. So while the
		 *  measured voltage is less than 3.5V, we will put the unit to sleep and wake once per 
		 *  hour to check the charge status.
		 */
    wait(1000);
		while (batteryRead() < 3500)
		{
				Serial.println (F("Low power sleep"));
				Serial.flush();
				digitalWrite (BEEPIN, HIGH);      //  Make sure XBee is powered off
				digitalWrite (RANGE, LOW);        //  Make sure sonar is off
#ifdef BUS_PWR
        //digitalWrite (BUS_PWR, LOW);   //  Peripheral bus off
#endif
				RTC.enableInterrupts (EveryHour); //  We'll wake up once an hour
				RTC.clearINTStatus();             //  Clear any outstanding interrupts
				sleep.pwrDownMode();                    //  Set sleep mode to Power Down
				sleep.sleepInterrupt (RTCINTA, FALLING); //  Sleep; wake on falling voltage on RTC pin
		}


    /*
		// We will use the FONA to get the current time to set the Stalker's RTC
		if (fonaOn())
    {

      // set ext. audio, to prevent crash on incoming calls
      // https://learn.adafruit.com/adafruit-feather-32u4-fona?view=all#faq-1
      fona.sendCheckReply(F("AT+CHFA=1"), OK);

#ifndef DEBUG
      // Delete any accumulated SMS messages to avoid interference from old commands
      smsDeleteAll();
#endif

      clockSet();
    }
    fonaOff();
    */

		now = RTC.now();    //  Get the current time from the RTC

    Serial.print(F("Watchdog at "));
    Serial.print(now.hour());
    Serial.print(':');
    Serial.println(now.minute()+2);
    wait(1000);
    //RTC.enableInterrupts2 (now.hour(), now.minute() + 2); // Set daily reset alarm 

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
//#ifndef DEBUG
    //test();
    //return;
//#endif

#ifdef BUS_PWR
    //digitalWrite (BUS_PWR, HIGH);           //  Peripheral bus on
#endif

		now = RTC.now();      //  Get the current time from the RTC

		Serial.print (now.hour());
		Serial.print (F(":"));
		Serial.println (now.minute());

      //takeReading();
		//if (now.minute() % INTERVAL == 0 && !sentData)   //  If it is time to send a scheduled reading...
		if (now.minute() % INTERVAL == 0)   //  If it is time to send a scheduled reading...
		{
				upload ();
    }
		//} else { sentData = false };

		// We will turn on the XBee radio for programming only within a specific
		// window to save power
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

#ifdef BUS_PWR
    //digitalWrite (BUS_PWR, LOW);           //  Peripheral bus off
#endif
		sleep.pwrDownMode();                    //  Set sleep mode to "Power Down"
		//sleep.adcMode();
		//sleep.idleMode();
		RTC.clearINTStatus();                   //  Clear any outstanding RTC interrupts
		sleep.sleepInterrupt (RTCINTA, FALLING); //  Sleep; wake on falling voltage on RTC pin
}


void upload()
{
		if (fonaOn())
    {

      /*  One failure mode of the sonar -- if, for example, it is not getting enough power -- 
       *	is to return the minimum distance the sonar can detect; in the case of the 10m sonars
       *	this is 50cm. This is also what would happen if something were to block the unit -- a
       *	plastic bag that blew onto the enclosure, for example.
       *  We send the result anyway, as the alternative is send nothing
       */

      int16_t streamHeight = takeReading();
      uint8_t status = ews1294Post(streamHeight, solarCharging(), fonaBattery());

      // reset fona if upload failed
      if (!status)
      {
        fonaOff();
        fonaOn();
      }

      if (noSMS == false)
      {
          smsCheck();
      }

      /*   The RTC drifts more than the datasheet says, so we'll reset the time every day at
       *   midnight. 
       *
       */
      if (now.hour() == 0 && now.minute() == 0)
      {
        WDTCSR = _BV(WDE);
        while (1); // 16 ms
      }
      /*
      if (now.hour() == 0 && timeReset == false)
      {
          clockSet();
          timeReset = true;
      }
      //else { sentData = false }

      if (now.hour() != 0)
      {
          timeReset = false;
      }
      */
      
    }
		fonaOff();
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


#define SAMPLES 13
int16_t takeReading (void)
{
		//  We will take the mode of seven samples to try to filter spurious readings
		int16_t sample[SAMPLES];
		
    digitalWrite (RANGE, HIGH);           //  sonar on
    wait (1000);

		for (uint8_t sampleCount = 0; sampleCount < SAMPLES; sampleCount++)
		{
				sample[sampleCount] = pulseIn (PING, HIGH);
				Serial.print (F("Sample "));
				Serial.print (sampleCount);
				Serial.print (F(": "));
				Serial.println (sample[sampleCount]);
				wait (50);
		}

		int16_t sampleMode = mode (sample, SAMPLES);

		int16_t streamHeight = (SENSOR_HEIGHT - (sampleMode / 10)); //  1 µs pulse = 1mm distance

		Serial.print (F("Surface distance from sensor is "));
		Serial.print (sampleMode);
		Serial.println (F("mm."));
		Serial.print (F("Calculated surface height is "));
		Serial.print (streamHeight);
		Serial.println (F("cm."));

    digitalWrite (RANGE, LOW);           //  sonar off
		return streamHeight;
}


// insertion sort
void sort(int16_t *a, uint8_t n) {
  for (uint8_t i = 1; i < n; i++) {

    for (uint8_t j = i; j > 0 && a[j] < a[j-1]; j--) {
      int16_t tmp = a[j-1];
      a[j-1] = a[j];
      a[j] = tmp;
    }
  }
}


int16_t mode (int16_t *a, uint8_t n)    
/*   Calculate the mode of an array of readings
 *   FIXED From http://playground.arduino.cc/Main/MaxSonar
 */   
{
		uint8_t i = 0;
		uint8_t count = 0;
		uint8_t maxCount = 0;
		int16_t mode = 0;
		boolean bimodal;

		sort (a, n);

		while(i < (n - 1))
		{
      count = 0;
      while(a[i] == a[i + 1])
      {
        count++;
        i++;
      }

      if(count > maxCount)
      {
        mode = a[i];
        maxCount = count;
        bimodal = 0;
      }
      else if(count == 0)
      {
        i++;
      }
      else if(count == maxCount)     // the dataset has 2 or more modes
      {
        bimodal = 1;
      }
    }
				
    if(mode == 0 || bimodal == 1) // Return the median if no mode
    {
      mode = a[(n / 2)];
    }

    return mode;
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
 * ==== ==============   =======================
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
  uint32_t mV = 0;
  //return ((uint32_t)analogRead(BATT) * 155) / 8;

  for (uint8_t i = 155; i;i--)
  {
    mV += analogRead(BATT);
  }
  return mV/8;
}


// Solar panel charging status
//
// Detect status of the CN3065 charge-controller 'charging' and 'done' pins,
// which have a voltage divider between vbatt and status pins:
//  vbatt----10M----+-----+---1M----DONE
//                  |     |
//             SOLAR(A6)  +---2M----CHARGING
//
// SLEEPING: Both pins are gnd when solar voltage is < battery voltage +40mv
//
// AREF      1.1    3.3
// =====    ====   ====
// ERROR      0+     0+
// DONE     350+   115+  ( vbatt(4.2v) / (10M + 1M)/1M ) => 0.38v
// CHARGING 550+   180+  ( vbatt(3.6v+) / (10M + 2M)/2M ) => 0.6v
// SLEEPING 900+   220+  ( vbatt ) => 3.6v -> 4.2v
boolean solarCharging()
{
    int16_t solar = analogRead(SOLAR);
    Serial.print (F("solar analog: "));
    Serial.println (solar);
    return ( solar > 180 && solar <= 220 );  // 3.3v analogue ref
}


boolean ews1294Post (int16_t streamHeight, boolean solar, uint16_t voltage)
{
        uint16_t status_code = 0;
        uint16_t response_length = 0;
        char post_data[200];

        DEBUG_RAM

        // Construct the body of the POST request:
        sprintf_P (post_data,
            (prog_char *)F("api_token=" EWSTOKEN_ID "&data={\"sensorId\":\"" EWSDEVICE_ID "\",\"streamHeight\":\"%d\",\"charging\":\"%d\",\"voltage\":\"%d\",\"timestamp\":\"%d-%d-%dT%d:%d:%d.000Z\"}\r\n"),
              streamHeight,
              solar,
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

        fonaFlush();
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
/*
boolean ews1294Post2 (int16_t streamHeight, boolean solar, uint16_t voltage)
{
    uint16_t statusCode;
    uint16_t dataLen;
    char postData[128]; // was 200
    DEBUG_RAM

    // HTTP POST headers
    fona.sendCheckReply (F("AT+HTTPINIT"), OK);
    //fona.sendCheckReply (F("AT+HTTPSSL=1"), OK);   // SSL required
    fona.sendCheckReply (F("AT+HTTPPARA=\"URL\",\"http://dmis-staging.eu-west-1.elasticbeanstalk.com/api/v1/data/river-gauge\""), OK);

    fona.sendCheckReply (F("AT+HTTPPARA=\"REDIR\",\"1\""), OK);
    fona.sendCheckReply (F("AT+HTTPPARA=\"UA\",\"Tepmachcha/" VERSION "\""), OK);
    fona.sendCheckReply (F("AT+HTTPPARA=\"CONTENT\",\"application/json\""), OK);

        sprintf_P (post_data,
            (prog_char *)F("api_token=" EWSTOKEN_ID "&data={\"sensorId\":\"" EWSDEVICE_ID "\",\"streamHeight\":\"%d\",\"charging\":\"%d\",\"voltage\":\"%d\",\"timestamp\":\"%d-%d-%dT%d:%d:%d.000Z\"}\r\n"),
              streamHeight,
              solar,
              voltage,
              now.year(), now.month(), now.date(), now.hour(), now.minute(), now.second()
        );
        if (fona.HTTP_POST_start ("ews1294.info/api/v1/sensorapi", F("application/x-www-form-urlencoded"), post_data, strlen(post_data), &status_code, &response_length)) {


    // Note the data_source should match the last element of the url,
    // which must be a valid data_source
    // To add multiple user headers:
    //   http://forum.sodaq.com/t/how-to-make-https-get-and-post/31/18
    fona.sendCheckReply (F("AT+HTTPPARA=\"USERDATA\",\"data_source: river-gauge\\r\\nAuthorization: Bearer " DMISAPIBEARER "\""), OK);

    // json data
    sprintf_P(postData,
      (prog_char*)F("{\"sensorId\":\"" DMISSENSOR_ID "\",\"streamHeight\":%d,\"charging\":%d,\"voltage\":%d,\"timestamp\":\"%d-%02d-%02dT%02d:%02d:%02d.000Z\"}"),
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
*/


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
      (prog_char*)F("{\"sensorId\":\"" DMISSENSOR_ID "\",\"streamHeight\":%d,\"charging\":%d,\"voltage\":%d,\"timestamp\":\"%d-%02d-%02dT%02d:%02d:%02d.000Z\"}"),
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
