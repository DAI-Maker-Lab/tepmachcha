/*
 * arduino-mk.h
 * This is required when compiling with arduino-mk, and gets included with
 * a -I addtion to gcc command line in the makefile
 *
 * Generate this file with cproto (first copy .ino to .c, then cproto -s file.c)
 * there may be a new arduino-preproc command that uses ctags - investigate
 */
#include <Arduino.h>
#include "secrets.h" // override config.h template defs

void setup(void);
void loop(void);
void upload(void);
void wait(unsigned long);
void clockSet(void);
void fonaFlush(void);
char fonaRead(void);
void fonaGPRSOff(void);
boolean fonaOn(void);
void fonaOff(void);
int16_t takeReading(void);
int16_t mode(int16_t*, uint8_t);
int16_t mode2(int16_t*, uint8_t);
void isort(int16_t*, uint8_t);
void checkSMS(void);
boolean sendReading (int);
boolean validate (int);
boolean firmwareGet ();
uint16_t batteryRead(void);
void eepromWrite(void);
boolean fonaSendCheckOK(const __FlashStringHelper*);
boolean ftpGet(void);
boolean fonaPowerOn(void);
boolean fonaSerialOn(void);
void smsDeleteAll(void);
void smsCheck (void);
void test(void);
boolean dmisPost(int, boolean,  uint16_t);
int freeRam(void);
void ram(void);
void xtea_decrypt(uint32_t[2]);
uint32_t crc_update(uint32_t, uint8_t);
boolean solarCharging(void);
extern char file_name[];
extern uint16_t file_size;
boolean sendReading(uint16_t);
void reflash (void);
boolean ews1294Post (int, boolean, uint16_t);
uint16_t fonaBattery(void);

