#include "tepmachcha.h"

SoftwareSerial fonaSerial = SoftwareSerial (FONA_TX, FONA_RX);
Adafruit_FONA fona = Adafruit_FONA (FONA_RST);


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
  // read a char from fona, waiting up to <timeout> ms for something at arrive
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


#define SIZEOF_SMS 80
#define SIZEOF_SMS_SENDER 18
void smsParse(uint8_t NumSMS)
{
		char smsBuffer[SIZEOF_SMS];
		char smsSender[SIZEOF_SMS_SENDER];
		uint16_t smsLen;

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
        XBeeOn();
        XBeeOnMessage(smsBuffer);
        fona.sendSMS(smsSender, smsBuffer);  //  Tell the sender what you've done
        Serial.println (F("XBee turned on by SMS."));
    }
}


//  Check SMS messages received for any valid commands
void smsCheck (void)
{
		uint32_t timeout;
		int8_t NumSMS;

		fonaFlush();    //  Flush out any unresolved data
		Serial.println (F("Checking for SMS messages..."));

		timeout = millis() + 60000;       // it can take a while for fona to receive queued SMS
    do {
        NumSMS = fona.getNumSMS();    // -1 for error
        wait (5000);
    } while (NumSMS == 0 && millis() <= timeout);

		Serial.print (NumSMS);
		Serial.println (F(" message(s) waiting."));

    // For each SMS message
		while (NumSMS > 0)
		{
      smsParse(NumSMS);

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
