#include "tepmachcha.h"

void testSMS() {
  fona.sendSMS(TESTPHONE, "hello from tepmachcha");
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
      Serial.println (fonaBattery());
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
        Serial.print(F("open file failed\n"));
        break;
      }
      boolean s = fonaFileCopy(file_size);
      file.close();
      Serial.print(F("Return: "));
      Serial.println(s);
      break;
    }
    case 'd': {
      //dmisPost(takeReading(), solarCharging(), fonaBattery());
      ews1294Post(takeReading(), solarCharging(), fonaBattery());
      break;
    }
    case 'o': {
      fonaPowerOn();
      fonaSerialOn();
      break;
    }
    case 'X': {
      file_size = 25874;
      strcpy_P(file_name, (prog_char*)F("EWS.BIN"));
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
    case 's': {
       smsCheck();
       break;
    }
    case '1': {
       testSMS();
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
      upload();
      break;
    }
  }
  fonaFlush();
}
