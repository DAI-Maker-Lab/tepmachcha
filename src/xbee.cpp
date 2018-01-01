#include "tepmachcha.h"

// XBee is turned on:
// 1) For 1 hour after boot
// 3) For 1 hour after receiving XBee SMS
// 2) During the XBee window, daily
//
//  We maintain a power state flag, and whenever we set it, we set a shutoff hour/minute
//

boolean xBeeState = HIGH;         // XBee power state
uint8_t xBeeShutoffHour = 0;       // Hour to turn off manual power to XBee
uint8_t xBeeShutoffMinute = 0;     // Minute to turn off manual power to XBee

void XBeeOn ()
{
		// We'll keep the XBee on for an hour after startup to assist installation
    xBeeShutoffHour = (now.hour() + 1) % 24;
		xBeeShutoffMinute = now.minute();

}

void XBeeOnMessage(char *buffer)
{
    sprintf_P(buffer, (prog_char *)F("XBee on until %02d:%02d"), xBeeShutoffHour, xBeeShutoffMinute);
		xBeeState = LOW;
    digitalWrite (BEEPIN, xBeeState);
}


// Check if we hit trigger times (window start, shutoff time)
// Apply XBee state
void XBee (void)
{
  // Turn on at start of window
  if ( now.hour() == XBEEWINDOWSTART )
  {
    xBeeShutoffHour = XBEEWINDOWEND;
    xBeeShutoffMinute = 0;
    xBeeState = LOW;
  }

  // Turn off when we reach shutoff time
  if ( now.hour() == xBeeShutoffHour && now.minute() >= xBeeShutoffMinute )
  {
    xBeeShutoffHour = 0;
    xBeeShutoffMinute = 0;
    xBeeState = HIGH;
  }

  // Turn the Xbee on/off according to flag
  digitalWrite (BEEPIN, xBeeState);
}
