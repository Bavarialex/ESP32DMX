# ESP32DMX
DMX -> MQTT

Recieve E1.31 DMX from xLights p.ex. and send via mqtt to IOBroker. To have IOBroker change lights due to DMX.

![IOBroker screenshot](https://github.com/Bavarialex/ESP32DMX/blob/main/esp32dmx01.JPG?raw=true)

- use with PlatformIO in Visual Studio Code
- see platformio.ini

Using following libraries:

<WiFi.h>

<ESPmDNS.h>

<WiFiUdp.h>

<ArduinoOTA.h>

- OTA updates directly ot of platformIO


<TelnetSpy.h>

- Telnet output instead of serial out. Just connect via telnet and see all your debug info.


<WiFiClient.h>

<AsyncTCP.h>

<ESPAsyncWebServer.h>

<AsyncElegantOTA.h>

- updates possible via http://IPofESP/update


<PubSubClient.h>

- mqtt stuff


<Wire.h>

<ESPAsyncE131.h>

- DMX stuff




Settings for xLights:

![xLights screenshot](https://github.com/Bavarialex/ESP32DMX/blob/main/xl01.JPG?raw=true)

Uses RGB-led to display:
Red GPIO05 - Wifi connected
Blue GPIO19 - Mqtt connected
Green GPIO18 - DMX active


