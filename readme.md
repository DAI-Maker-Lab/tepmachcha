# Tepmachcha

![tepmacha](https://raw.githubusercontent.com/DAI-Maker-Lab/tepmachcha/master/photos/tepmachcha.JPG)

## Flood Early Warning Using Sonar and IVR
Tepmachcha is an open source sonar stream gauge designed to give early warning of flood conditions to residents in vulnerable areas. When a flood condition is detected, Tepmachcha triggers a call with a voice recording via the RapidPro interactive voice response (IVR) system. It also records water levels at regular intervals for later analysis of flooding patterns or to inform more complex hydrological analysis. 

Tepmachcha was designed at the [DAI Maker Lab](http://dai.com/makerlab) in collaboration with [People in Need Cambodia](https://www.clovekvtisni.cz/en/humanitary-aid/country/cambodia) with funding from USAID's [Development Innovations](http://www.development-innovations.org/) project, People in Need, and DAI. It is a fork of the [*Hidrosónico* sonar stream gauge](https://github.com/DAI-Maker-Lab/hidrosonico) piloted by DAI in Honduras as part of the [USAID Proparque](http://en.usaid-proparque.org/) project. It is released under the MIT License.

## Matsya/Tepmachcha
The world-reknowned friezes at Angkor Wat depict scenes from Cambodian history and Hindu mythology, including the [tale of the god Vishnu appearing in the form of a fish to warn mankind of a catastrophic flood](https://en.wikipedia.org/wiki/Matsya). This fish avatar, known in Sanskrit as "Matsya", is called "Tepmachcha" in Khmer, and is the namesake of the project.

## What Tepmachcha Does: The Short Story
Tepmachcha uses sonar to read the level of water in a river, stream, or canal, at regular intervals determined by the user. It uses a cellular data connection to report that level to an internet server. If Tepmachcha detects water level in excess of yellow or red alert limits set by the user, it sends an HTTP POST request that triggers an instance of the RapidPro IVR system to make a voice call that warns users in the affected area(s). Certain operational commands can be sent to the unit by SMS.

## The DAI Maker Lab Design Approach
The DAI Maker Lab leverages emerging tools and approaches associated with the maker movement to build devices and capacity that empower people in developing countries to apply technology to problems. The goal is hardware that can be locally built, repaired, maintained and extended. Toward that end, DAI Maker Lab designs:

* Are always released as open source.
* Utilize off-the-shelf parts when possible.
* Utilize open source hardware when possible, so that discontinuted components can be later recreated or replaced if necessary. 
* Prefer well-documented component hardware, and hardware with an existing community of development and support, to maximize value to end users.
* Prefer transparency to non-expert users over optimal computational or engineering design.

## Hardware
The bill of materials for Tepmachcha includes:

* The [**Seeeduino Stalker v3.0**](http://www.seeedstudio.com/wiki/Seeeduino-Stalker_v3) Arduino-compatible microcontroller board. The Stalker is not the most commonly available Arduino-compatible board, and to be honest, the English documentation -- while much improved over the last year -- is not perfect. But its optimization for low-power operation; inclusion of a solar battery charging circuit, real-time clock (RTC), and XBee socket; and reasonable price make it a good choice for Tepmachcha. 
* The [**Adafruit FONA 808**](https://learn.adafruit.com/adafruit-fona-808-cellular-plus-gps-breakout/overview) cellular breakout board. There are less expensive GSM boards on the market, but Adafruit's standard of documentation, support, and community are difficult to beat. We use the 808 rather than the slightly cheaper 800 because it supports SSL, which RapidPro requires.
* The [**Maxbotix MB7363 HRXL-MaxSonar-WRLS**](http://www.maxbotix.com/Ultrasonic_Sensors/MB7363.htm) sonar. 
* [A simple custom PCB](https://oshpark.com/shared_projects/et6LqUSw) to connect the various parts. This is optional but keeps everything neat and connected inside the enclosure.
* An [**XBee XB24-AWI-001**](http://www.digikey.com/product-detail/en/digi-international/XB24-AWI-001/XB24-AWI-001-ND/935965) radio enables wireless reprogramming in proximity to the unit. Because the units are generally installed in places that are not easy to get to, taking them down and opening them up to flash updated firmware is a real pain. But the XBee can be tempremental; it costs money, uses power and there is a fair amount of code supporting its use, so a non-crazy person might elect to exclude it.
* A **power subsystem**. The Stalker has a charging circuit, but a [LiPo battery](https://www.adafruit.com/products/1781) and a [solar panel](https://www.adafruit.com/products/500) are required; the solar panel will probably have to be spliced onto a [JST-PH 2mm connector](https://www.adafruit.com/products/261) to plug into the Stalker.

The complete bill of materials is included in the repository.

## Libraries
Tepmachcha uses:

* The [Adafruit FONA library](https://github.com/adafruit/Adafruit_FONA_Library).
* [Seeed Studio's DS1337](https://github.com/Seeed-Studio/Sketch_Stalker_V3_1/blob/master/libraries/DS1337/DS1337.h) real time clock library.
* The [Sleep_n0m1](https://github.com/n0m1/Sleep_n0m1) sleep library.
* SoftwareSerial
* Wire
