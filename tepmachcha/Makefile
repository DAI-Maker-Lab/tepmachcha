AVRDUDE_ARD_BAUDRATE = 57600
ARDUINO_PORT = /dev/tty.usbserial-*
ARDUINO_PORT = /dev/tty.SLAB_USBtoUART
#AVRDUDE_OPTS = -V -v -v -v -v
#ISP =1						  # Use the ISP programer options
#ISP_PROG = usbasp	# set the isp programmer
#ISP_PORT = usb			# set ISP port
#ISP_NOERASE =1			# don't erase flash before program (eg keeps bootloader, or app)

# tepmachcha stalker
F_CPU=8000000L
BOARD_TAG    = uno
ARDUINO_LIBS = DS1337 Sleep_n0m1 Adafruit_FONA_Library SoftwareSerial Wire Fat16 EEPROM

ARDUINO_HEADER=arduino-mk.h		# include our own header, which includes Arduino.h, and function prototypes
ARDUINO_DIR   = /Applications/Arduino.app/Contents/Java
ARDUINO_TOOLS_DIR = /Applications/Arduino.app/Contents/Java/hardware/tools

# Use our own Library dir
# https://forum.arduino.cc/index.php?topic=445230.0
# USER_LIB_PATH=./src

include ~/src/Arduino/arduino-mk/Arduino.mk
