/*
 * arduino-mk.h
 * This is required when compiling with arduino-mk, and gets included with
 * a -I addtion to gcc command line in the makefile
 *
 * Generate this file with cproto (first copy .ino to .c, then cproto -s file.c)
 * there may be a new arduino-preproc command that uses ctags - investigate
 */
#include <Arduino.h>

void setup(void);
void loop(void);
void upload(int);
void wait(unsigned long);
void clockSet(void);
void fonaFlush(void);
char fonaRead(void);
boolean fonaOn(void);
void fonaOff(void);
int takeReading(void);
int mode(int*, int);
void checkSMS(void);
boolean sendReading (int);
boolean validate (int);
boolean ivr (const char*);
boolean getFirmware ();
uint16_t readBattery(void);
void writeEeprom(void);
boolean fonaSendCheckOK(const __FlashStringHelper*);
boolean ftpGet(void);
void fonaPowerOn(void);
boolean fonaSerialOn(void);
void smsDeleteAll(void);
void smsCheck (void);
void test(void);
boolean dmis (int);
int freeRam(void);
void ram(void);
