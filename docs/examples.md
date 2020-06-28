![plainhdr](../assets/pangoplain.jpg)

# Examples

- [Examples](#examples)
  - [Quick Start](#quick-start)
  - [ShowMeTheBugs](#showmethebugs)
    - [Choosing the library](#choosing-the-library)
    - [Theory of the code](#theory-of-the-code)
- [Session Recovery 1 and 2](#session-recovery-1-and-2)
    - [Theory of the code](#theory-of-the-code-1)

## Quick Start

The ["A_" version](../examples/QuickStart_A/QuickStart_A.ino) compiles with AsyncMqttClient, the ["P_" version](../examples/QuickStart_P/QuickStart_P.ino) uses Pangolin. The two are as close as possible in code, afetr allowing for minor [API](api.md) differences.

Comparing the two side by side should give a quick idea of the changes you need to make to any exisiting code base you may have in order to move your app from AsyncMqttClient to Pangolin

In-depth details of the API changes and the reasons for each can be see in the [API](api.md) documentation itself.

---

## ShowMeTheBugs

### Choosing the library

At the top of the sketch is the line:

```cpp
#define USE_PANGOLIN
```

This will compile the sketch using the PangolinMQTT library. Barring any "teething troubles" in this beta release, this version should just repsond correctly to sensible values sent to it as described below.

Removing the line will cause the sketch to run with the AsyncMqttClient library. A  large proportion of those same tests will fail, for one or more of the reasons explained in the [bugs documentation](bugs.md)

In order to run [this example](../examples/ShowMeTheBugs/ShowMeTheBugs.ino), with either library you wil need an MQTT client app on your PC or mobile device. A popular choice on PCs is [mqtt-spy](https://github.com/eclipse/paho.mqtt-spy/wiki/Downloads) but anything that can send PUBLISH requests to a server will do.

### Theory of the code

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

---

# Session Recovery 1 and 2

Both these sketches are based on very similar code with minor differences for QoS1 and QoS2.

### Theory of the code

The code for both sends and receives a message with an ever-increasing integer as the payload. It then randomly disconnects 5% of the time after each send. On reconnection At QoS1 (where duplicates are allowed) it will count the number of duplicates re-sent. Any duplicate at QoS2 is a breach of the MQTT protocol.

When compiled using the Pangolin library, both sketches will record the receipt of every single value sent in ascending order *at least once* t QoS1 and *exactly once* at QoS2 as required by the MQTT protocol.

On the other hand, when running the sketches with the AsyncMqttClient library, bith will fail to deliver messages, thereby a) breaching the MQTT protocol and b) proving that AsyncMqttClient ***simply does not work at either QoS1 or QoS2***

More details (and many other bugs) can be seen in the [bugs documentation](bugs.md)
