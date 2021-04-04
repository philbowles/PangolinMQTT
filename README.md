![mbm](assets/pangoplain.jpg)

# ArduinoIDE MQTT client library for ESP8266, ESP32

###### Version 2.0.0 [Release Notes](docs/rn200.md)

* [Features](#features)
* [Performance](#performance)
* [Unique features](#features-you-may-not-find-in-other-libraries)
* [Installation](#installation)
* [Issues / Support](#issues--support)
* [IMPORTANT NOTE FOR PLATFORMIO USERS](#important-note-for-platformio-users)

# Other Documents
* [Getting Started](101.md)
* [Payload Handling and "expert" functions](pl.md)
* [Full API specification](docs/api.md)
* [Using TLS](docs/tls.md)
* [Challenges of embedded MQTT](docs/qos.md)
  
---

# Features

 * Full* MQTT v3.1.1 Qos0/1/2 compliance, session management and recovery
 * Payload size limited only by available heap (~20kB on ESP8266, ~120kb on ESP32)
 * TLS support (ESP8266 only)
 * Compile time multilevel diagnostic
 * Utility functions to view binary payloads and handle as:
   * C-style string (`char*`)
   * `std::string`
   * Arduino `String`
   * < any arithmetic type >
 * Full error-handling including:
   * QoS failures*
   * Subscribe failure
   * "Killer" packets ( > free available heap) both inbound and outbound

**NB** *No device can *fully* comply with MQTT 3.1.1 QoS unless it has unlimited permanent storage to hold failed QoS messages across reboots. For more in-depth explanation of the reasons, read [Challenges of MQTT QoS on embedded systems](docs/qos.md) 

---

# Performance

![perf](assets/performance.jpg)

---


---

# Features you may not find in other libraries:

## Large Payloads

Pangolin automatically fragments outbound packets and reassembles inbound packets of any size up to about 1/2 the free heap. User code simply gets a copy of the full packet - irrespective of its size - without any fuss and requiring *zero* code on the user's part.

## Full QoS1 / QoS2 compatibility / recovery

PangolinMQTT's author is unaware of any similar libraries for ESP8266/ESP32 that *fully* and *correctly* implement QoS1/2 *in all circumstances*.

They *may* sometimes work, e.g. with only small packets and/or slow send rates but fail when either increases. None will correctly recover "lost" messages on unexpected disconnect. Given that this is pretty much the *only* purpose of QoS1/2 then any library *not* fulfilling this promise *cannot* legitimately claim to be QoS1/2 compatible. If a library does not do this:

![mqtt spec](assets/pv2mqtt.jpg)

Then it ***does not support QoS1/2*** no matter what claims it may make.

Pangolin has been tested to be QoS1/2 compatible and contains 2 [example sketches](examples/SessionRecovery1/SessionRecovery1.ino) to demonstrate this feature.

---

# Installation

Pangolin depends upon [ESPAsyncTCP](https://github.com/me-no-dev/ESPAsyncTCP) (for ESP8266 targets) or [AsyncTCP](https://github.com/me-no-dev/AsyncTCP) for ESP32 targets.

You will need to install one or both of those before using Pangolin.

HOWEVER: [ESPAsyncTCP](https://github.com/me-no-dev/ESPAsyncTCP) ***contains a serious bug which prevents it compiling with SSL when using TLS secure connections***. 

PangolinMQTT's author has provided a bugfix version of the above library, see [Using TLS](docs/tls.md) for additional information

## Compiling

![performance](assets/lwip.jpg)

---

# Issues / Support

## IMPORTANT NOTE FOR PLATFORMIO USERS

Pangolin is an *Arduino library*, and is 100% compatible with the ArduinoIDE and its build system. PlatformIO, sadly, ***is not***. If PlatformIO has problems with code that compiles and runs correctly under ArduinoIDE, then it is a ***PlatformIO problem***, not an issue with this - or any other - valid Arduino library.

For this reason, I will not accept any issues relating to build problems with PlatformIO, nor any pull requests nor other suggestions which involve any changes that render this library less than 100% ArduinoIDE compatible. 

## Non PlatformIO-related issues

Your **first** point of contact should be one of the facebook groups below, if only to let me know you have raised an issue here. Obviously I will check the issues from time to time, but I do no have the time to check every day.

If you want a rapid response, I am daily moderating those FB groups, including a new one especially set up for Pangolin users:

## Before submitting an issue

If you do not provide sufficient information for me to be able to replicate the problem, ***I CANNOT FIX IT***

So, always provide: the MCU/board type, a good description of the problem, how / when / why it happens and how to recreate it, as well as the full source code, relevant Serial output messages and a DECODED stack trace in the event of a crash.

## And finally...

This is open-source, I do it in my own time, for free. If you want professional-level support because you are using *my* work to benefit your own commercial gain, then I'm happy to talk privately about a paid support contract. Or you can support me on [Patreon](https://www.patreon.com/esparto) 

---

## Find me daily in these FB groups

* [Pangolin Support](https://www.facebook.com/groups/pangolinmqtt/)
* [ESP8266 & ESP32 Microcontrollers](https://www.facebook.com/groups/2125820374390340/)
* [ESP Developers](https://www.facebook.com/groups/ESP8266/)
* [H4/Plugins support](https://www.facebook.com/groups/h4plugins)

I am always grateful for any $upport on [Patreon](https://www.patreon.com/esparto) :)


(C) 2021 Phil Bowles