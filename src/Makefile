
#BOARD_TAG    = usbasploader_board
#BOARD_TAG    = atmega8
#ARDUINO_PORT = /dev/tty.SLAB_USBtoUART
BOARD_TAG    = atmega328
#BOARD_TAG    = t85
#ARDUINO_PORT = usb
#AVRDUDE_ARD_BAUDRATE = 115200
#AVRDUDE_ARD_OPTS = -D -c $(

#AVRDUDE_ARD_PROGRAMMER = usbasp
ARDUINO_PORT = usb
#ARDUINO_PORT = /dev/tty.usbserial
ARDUINO_PORT = /dev/null
F_CPU=8000000L

ARDUINO_HEADER=arduino-mk.h		# include our own header, which includes Arduino.h, and function prototypes

# tepmachcha, serial
BOARD_TAG    = atmega328
BOARD_TAG    = uno
AVRDUDE_ARD_BAUDRATE = 28800
AVRDUDE_ARD_BAUDRATE = 9600
AVRDUDE_ARD_BAUDRATE = 57600

ARDUINO_PORT = /dev/tty.usbmodem1411
ARDUINO_PORT = /dev/tty.usbserial
ARDUINO_PORT = /dev/tty.SLAB_USBtoUART
ARDUINO_DIR   = /Applications/Arduino2.app/Contents/Java
#ARDUINO_TOOLS_DIR = /Applications/Arduino2.app/Contents/Java/hardware/tools

# normally ISP_PROG is defined BOARD_TAG config entries in boards.txt
#ISP_PROG = -c usbasp -B 32

ARDUINO_LIBS = DS1337 Sleep_n0m1 Adafruit_FONA_Library SoftwareSerial Wire Fat16
include ~/src/Arduino/arduino-mk/Arduino.mk

