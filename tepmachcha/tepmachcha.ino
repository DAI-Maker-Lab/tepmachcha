#include "tepmachcha.h"

boolean freshboot = true; // Newly rebooted
Sleep sleep;              //  Create the sleep object

static void rtcIRQ (void)
{
		RTC.clearINTStatus(); //  Wake from sleep and clear the RTC interrupt
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
		pinMode (WATCHDOG, INPUT_PULLUP);
		//pinMode (WATCHDOG, INPUT);

		pinMode (BEEPIN, OUTPUT);
		pinMode (RANGE, OUTPUT);
		pinMode (FONA_KEY, OUTPUT);
		pinMode (FONA_RX, OUTPUT);
		pinMode (SD_POWER, OUTPUT);
#ifdef BUS_PWR
		pinMode (BUS_PWR, OUTPUT);
#endif

    digitalWrite (RANGE, LOW);           // sonar off
		digitalWrite (SD_POWER, HIGH);       // SD card off
    digitalWrite (BEEPIN, LOW);          // XBee on
		digitalWrite (FONA_KEY, HIGH);       // Initial state for key pin
#ifdef BUS_PWR
    digitalWrite (BUS_PWR, HIGH);        // Peripheral bus on
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

    // Set RTC interrupt handler
		attachInterrupt (RTCINTA, rtcIRQ, FALLING);
		interrupts();

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

		now = RTC.now();    //  Get the current time from the RTC

		RTC.enableInterrupts (EveryMinute);  //  RTC will interrupt every minute
		RTC.clearINTStatus();                //  Clear any outstanding interrupts
		
    // turn XBee on for an hour
    XBeeOn();
    char buffer[32]; // only 20 required currently
    XBeeOnMessage(buffer);
    Serial.println(buffer);
}


void loop (void)
{

#ifdef BUS_PWR
    //digitalWrite (BUS_PWR, HIGH);           //  Peripheral bus on
    //wait(500);
#endif

		now = RTC.now();      //  Get the current time from the RTC

		Serial.print (now.hour());
		Serial.print (F(":"));
		Serial.println (now.minute());

    // The RTC drifts more than the datasheet says, so we
    // reset the time every day at midnight, by soft reboot
    if (!freshboot && now.hour() == 1 && now.minute() == 0)
    {
      Serial.println(F("reboot"));
      WDTCSR = _BV(WDE);
      while (1); // 16 ms
    } else {
      freshboot = false;
    }

    XBee();

		if (now.minute() % INTERVAL == 0)   //  If it is time to send a scheduled reading...
		{
				upload ();
    }

		Serial.println(F("sleeping"));
		Serial.flush();                         //  Flush any output before sleep

#ifdef BUS_PWR
    //digitalWrite (BUS_PWR, LOW);           //  Peripheral bus off
#endif

		sleep.pwrDownMode();                    //  Set sleep mode to "Power Down"
		RTC.clearINTStatus();                   //  Clear any outstanding RTC interrupts
		sleep.sleepInterrupt (RTCINTA, FALLING); //  Sleep; wake on falling voltage on RTC pin
}


void upload()
{

  int16_t streamHeight;
  uint8_t status;
  boolean charging;
  uint16_t voltage;

  if (fonaOn())
  {

    /*
    if (now.hour() == 0 && now.minute() == 0)
    {
       clockSet()
    }
    */

    /*  One failure mode of the sonar -- if, for example, it is not getting enough power -- 
     *	is to return the minimum distance the sonar can detect; in the case of the 10m sonars
     *	this is 50cm. This is also what would happen if something were to block the unit -- a
     *	plastic bag that blew onto the enclosure, for example.
     *  We send the result anyway, as the alternative is send nothing
     */
    if ((streamHeight = takeReading()) <= 0)
    {
      streamHeight = takeReading();     // take a second reading
    }

    charging = solarCharging();
    voltage = fonaBattery();

    dweetPost(streamHeight, charging, voltage);
    if (!(status = ews1294Post(streamHeight, charging, voltage)))
    {
      status = ews1294Post(streamHeight, charging, voltage);    // try once more
    }

    // reset fona if upload failed
    if (!status)
    {
      fonaOff();
      wait(2000);
      fonaOn();
    }

    // process SMS messages
    smsCheck();

  }
  fonaOff();
}


boolean ews1294Post (int16_t streamHeight, boolean solar, uint16_t voltage)
{
    uint16_t status_code = 0;
    uint16_t response_length = 0;
    char post_data[240];

    DEBUG_RAM

    // Construct the body of the POST request:
    sprintf_P (post_data,
        // uptime, freeRam, analogCharge, version, internalTemp, RSSI, statusCode
        (prog_char *)F("api_token=" EWSTOKEN_ID "&data={\"sensorId\":\"" EWSDEVICE_ID "\",\"version\":\"" VERSION "\",\"streamHeight\":\"%d\",\"charging\":\"%d\",\"voltage\":\"%d\",\"timestamp\":\"%d-%d-%dT%d:%d:%d.000Z\"}\r\n"),

        //(prog_char *)F("api_token=" EWSTOKEN_ID "&data={\"sensorId\":\"" EWSDEVICE_ID "\",\"streamHeight\":\"%d\",\"charging\":\"%d\",\"voltage\":\"%d\",\"timestamp\":\"%d-%d-%dT%d:%d:%d.000Z\"}\r\n"),
          streamHeight,
          solar,
          voltage,
          now.year(), now.month(), now.date(), now.hour(), now.minute(), now.second()
    );

    Serial.println (F("data:"));
    Serial.println (post_data);

    // ews1294.info does not currently support SSL; if it is added you will need to uncomment the following
    //fona.sendCheckReply (F("AT+HTTPSSL=1"), F("OK"));   //  Turn on SSL
    //fona.sendCheckReply (F("AT+HTTPPARA=\"REDIR\",\"1\""), F("OK"));  //  Turn on redirects (for SSL)

    // Send the POST request we have constructed
    if (fona.HTTP_POST_start ("ews1294.info/api/v1/sensorapi",
                              F("application/x-www-form-urlencoded"),
                              (uint8_t *)post_data, strlen(post_data),
                              &status_code,
                              &response_length))
    {
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

    //TODO don't need http:// in url, in fact it breaks when using https://
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


boolean dweetPost (int16_t streamHeight, boolean solar, uint16_t voltage)
{
    uint16_t statusCode;
    uint16_t dataLen;
    char postData[200];
    DEBUG_RAM

    // HTTP POST headers
    fona.sendCheckReply (F("AT+HTTPINIT"), OK);
    fona.sendCheckReply (F("AT+HTTPSSL=1"), OK);   // SSL required
    fona.sendCheckReply (F("AT+HTTPPARA=\"URL\",\"dweet.io/dweet/quietly/for/" DWEETDEVICE_ID "\""), OK);
    fona.sendCheckReply (F("AT+HTTPPARA=\"REDIR\",\"1\""), OK);
    fona.sendCheckReply (F("AT+HTTPPARA=\"UA\",\"Tepmachcha/" VERSION "\""), OK);
    fona.sendCheckReply (F("AT+HTTPPARA=\"CONTENT\",\"application/json\""), OK);

    // json data
    sprintf_P(postData,
      (prog_char*)F("{\"streamHeight\":%d,\"charging\":%d,\"voltage\":%d,\"uptime\":%ld,\"version\":\"" VERSION "\",\"internalTemp\":%d,\"freeRam\":%d}"),
        streamHeight,
        solar,
        voltage,
        millis(),
        internalTemp(),
        freeRam());
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
