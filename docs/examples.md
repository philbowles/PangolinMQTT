![plainhdr](../assets/pangoplain.jpg)

# Examples

- [Examples](#examples)
  - [Quick Start](#quick-start)
  - [ShowMeTheBugs](#showmethebugs)
  - [Theory of the code](#theory-of-the-code)

## Quick Start

The ["A_" version](../examples/QuickStart_A/QuickStart_A.ino) compiles wiht AsyncMqttClient, the ["P_" version](../examples/QuickStart_P/QuickStart_P.ino) uses Pangolin. The two are as close as possible in code, afetr allowing for minor [API](api.md) differences.

Comparing the two side by side should give a quick idea of the changes you need to make to any exisiting code base you may have in order to move your app from AsyncMqttClient to Pangolin

In-depth details of the API changes and the resons for each can be see in the [API](api.md) documentation itself.

## ShowMeTheBugs

In order to run [this example](../examples/ShowMeTheBugs/ShowMeTheBugs.ino), you wil need an MQTT client app on your PC or mobile device. A popular choice on PCs is [mqtt-spy](https://github.com/eclipse/paho.mqtt-spy/wiki/Downloads) but anything that can send PUBLISH requests to a server will do.

## Theory of the code

The code is designed to measure the round-trip time-of-flight from the client to the server and back for a group or "burst" of messages at a chosen QoS.

The time is taken at the start, then again when the last message arrives back. The difference is then divided by the number of messages in the burst to get the simple average round-trip time-of-flight.

All of the variables can be changed by the user via a remote app. They are:

* rate  
Payload=The number of milliseconds between each group or "burst" of messages. 1000 = 1 second
* size
Payload=The size of the payload for each message in bytes
* burst
Payload=The number of consecutive messages to send on each "tick" of the `rate` timer
* qos
Payload=0,1 or 2

The default starting values are:

* QoS 0
* Burst Size 4
* Payload Size	100
* TX Rate	5000

The whole cycle can be started and stopped by publishing `start` and `stop` topics, with no payload







