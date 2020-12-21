![plainhdr](../assets/pangoplain.jpg)
# MQTT Payload handling

***N.B. You MUST read this before using any of the 'x' (expert) functions.***

---

## MQTT payloads: general

An MQTT payload consists of an arbitrary block of data and its length.

The ***ONLY*** correct data type for the data on ESP8266 / ESP32 / STM32 is `uint8_t` (or `byte`) and thus the correct type for a pointer to that data is  `uint8_t*`.

Given that PangolinMQTT allows for much bigger payloads than most other libraries (up to 0.5* the amount of the free heap) its defintion is `size_t`.

---

## Simple publish

User provides the address of (or pointer to) a block of data, and its length. 

```cpp
void publish(const char* topic,const uint8_t* payload, size_t length, uint8_t qos=0,  bool retain=false);
// There is also a degenerate version to help people who STILL think char* and uint8_t* are the same thing: they are NOT :) )
void publish(const char* topic,const char* payload, size_t length, uint8_t qos=0,  bool retain=false);
```

In either case, the user *must* also provide the length of the data to be published. The most common case of a simple, fixed non-retained message @ QoS0 is reflected in the defaults, such that the majority of calls will resemble:

```cpp
#define MY_PLENGTH 42
uint8_t my_data[MY_PLENGTH];
...
mqttClient.publish("my/topic/subtopic",&my_data[0], MY_PLENGTH);
...
```

Obviously the length does not need to be fixed, nor does the buffer need to be statically declared; for variable-length payloads, the user may define / allocate the data any way he/she sees fit. It is then up to the user to know or calculate the length of the buffer.

```cpp
...
mqttClient.publish("my/topic/subtopic",some_data_pointer, some_data_length);
...
```

For differing payloads, the length calcualtion ***must*** be correctly performed and the memory allocated prior to every call - and usually freed after it. This is both time-consuming and error-prone, hence the `xPublish` variants exist to do this routine work for you correctly in all cases depending on the type of data being sent.

---

## Publishing numeric "strings"

PangolinMQTT Provides a version of the `publish` API which operates on common arithmetic types e.g. `int` `unsigned` `float` etc. It will automatically convert the data to a string according to `printf`-style qualifiers and send it with the appropriate length

```cpp
int myvalue=123456;

mqttClient.publish("mytopic/integer",myvalue); // publishes "123456" with length=6 (default format is %d)
mqttClient.publish("mytopic/integer",myvalue,"0x%08X"); // publishes "0x0001E240" with length=10
...
```

---

# Received payloads

First, remind yourself that an MQTT payload consists of an arbitrary block of data and a length. The MQTT specification has ***nothing*** to say about what that data represents. Only the programmer / system that placed the data and its correct length in the message actually "knows" what the data ***should*** "look" like. Did the sender publish a printable string? A JSON-formatted printable string? A 32-bit (4 byte) binary value? Who knows? *Only the sender*.

Thus there can be no escape from the fact that it is *up to the user* to "unpack" the payload and "reconstruct" the data that he/she *thinks* it represents. In a perfect world, it will always look as expected and be within acceptable range / bounds / length of what is expected. In our imperfect world, any programmer relying on the previous assumption has just created a massive bug. Don't let that programmer be *you*.

## Golden rule 1

***NEVER*** Assume you *know* what the contents of the payload and its length are. Despite what you may think, *they **can** be anything*. If you don't confirm they are "sensible" for your app, you have just created a bug.

## Golden rule 2

***ALWAYS*** Validate the length of the payload as being within the bounds of acceptable lengths for the expected content. For example, if you are expecting a 32-bit (4-byte) binary value and the length is 8 then *you have a problem*. Reject the message with some sensible user logging.

## Golden rule 3

***MQTT payloads are NOT strings (of any kind!)*** They are - as we already know - an arbitrary block of data and a length.

Let that sink in for a moment. In the vast majority of cases, users will send "string" payloads and hence they may find that a confusing statement. Let's state it another way: Most payloads consisting of a series of printable characters that most people think look like "strings" of some kind. But MQTT is perfectly happy to accept ***any*** character whether its a valid printable "string" character or not: "0x01" or 0xFE for example. From MQTT's point of view your data of "mydata" is just a 6-byte payload. As is `{0x01,0x02,0x03,0x04,0x05,0x06}` which is most definitely ***not*** like anything anyone would call any kind of "string". But it ***is*** a perfectly valid 6-byte MQTT payload.

## "Unpacking" a payload safely

The correct way to "unpack" a payload then is:

1   Validate that it contains a sensible length for the expected data type else reject and skip all remaining steps
2   Allocate (or have previously statically declared) a buffer of the correct length
3   Copy <length> bytes of data from the payload to your own buffer
4   use the data
5   Free the buffer (if previously allocated dynamically)

Skipping any of these steps ***will*** create a potential bug. Whether that bug actually bites you is a matter of luck. Let's look at some examples.

### Example 1 - potential errors: fixed buffer

Your app receives positive temperature values in topic "temp" as up to 3 ASCII characters in the range 0-999. This means that your payload can be length 1,2 or 3 so as long as you receive "1" or "25" or "666" everything should be fine.

Now consider the following code:

```cpp
char tmp[]={0,0,0,0};
memcpy(tmp,payload,length);
printf("Temp is %s\n",&tmp[0]);
```

Because you have cleverly allowed for the fact that even a 3-character payload wil be NUL-terminated, you can treat tmp as a C-style string.

Next, remember that ***anyone*** can publish any topic to MQTT. Then imagine me publishing "ABC" to "temp". There is nothing you can do to stop me - or anyone else - doing that, and you code will break. Maybe not fatally yet, but your temp will be wrong and even *more* wrong if you try to convert it to an int and do maths on it, because you didn't validate that what was received was numeric.

Now let's ramp up the volume and publish "1234567890" at you. You will now write 10 bytes of data into a 4-byte buffer: pretty much anything can happen, but most likely, you will get a crash and reboot. You did not confirm length was < 4 before proceeding.

### Example 2 - potential errors: dynamic buffer

Your app expects to receive someone's name in topic "name". It can be up to 256 bytes long. To avoid holding on to an expensively large buffer, you allocate it dynamically each time. You want to treat it as a string.

```cpp
uint8_t* buf=malloc(length+1); // C-strings are NUL terminated!!! MQTT payloads are not!
memcpy(buf,payload,length);
buf[length]=\0; // poke in the terminating NUL to make buf LOOK LIKE a C-string
printf("Name is %s\n",static_cast<char*>(&tmp[0])); // coerce buf to correct type to treat as C-string
free(buf); // memory leak if you forget this
```

Better? No, it has the same problem as above: If I publish a 300-byte string to "name" your code will crash.

Also, if you forget the +1 or adding the final \0 then you *may not* have a properly terminated string and again your code will probably crash. The worst problem with this code is that you *accidentally* may get what *looks like* a valid string. You *may* be handling corrupt data, or your code *may* work or it may crash, apparently randomly, since each different length message will behave differently. Try debugging *that*.

Finally, if you forget the `free(buf);` call you will eventually run out of memory and crash. How soon that happens will depend entirely on what messages you receive and how long each is.

### Example 3 - potential errors: directly using the payload pointer

All of this sound like and unnecessarily complex process: you "know" the payload is a string so you are just going to use the payload pointer, like this:

```cpp
printf("Name is %s\n",static_cast<char*>(&payload[0])); // coerce buf to correct type to treat as C-string
```

This is probably *THE* most common payload handling error / potential bug. In many cases, but ***purely by chance*** the library's internal buffer *could be* followed by a NUL byte (\0) in which case your code will work as expected. On the occasions when it is NOT followed by chance by a \0, (*possibly totally random*) you will get a) unexpected results or b) a crash, both of which will be very difficult - if not impossible - to replicate and/or debug.

***NEVER ASSUME THE PAYLOAD IS A VALID C-STYLE STRING!!!***

---

# The 'x functions' 

## xPublish / xPayload

For all of the above reasons, the xPublish variants exist to do most of that "uncessarily complex" (but *essential*) processing for you and calculate the correct payload length for the chosen data type. This then allows you to reverse the process simply with the matching `xPayload` calls, to directly create / assign a variable of the chosen type from the expected value in the payload.

Obviously this is only going to work if you use matching `xPublish`and `xPayload` calls. Publishing  an int and then unpacking it as a string isn't going to work! Nor is using the bog-standard `publish` then using a specific `xPayload` call - unless you deliberately and pointlessly replicate all the correct code that the corresponding `xPublish` call would have done.

While some simple validation / rejection can be done by the  `xPayload` calls, ***it is always up to the programmer to ensure the payload data is valid before acting upon it***.

In summary: 

1 ***DO NOT MIX AND MATCH 'x' CALLS WITH STANDARD publish***
2 ***ALWAYS USE MATCHING 'xPublish' and 'xPayload' CALLS***
3 ***If you don't "get" all of this, stick to simple publish and do your own length calcualtions and unpacking***

---

## xPublish

### General form

```cpp
void xPublish(const char* topic,  [ VALUE ] , uint8_t qos=0,  bool retain=false);

```

Where [ VALUE ] can be pretty much any data type, e.g.

* const char*
* std::string
* String
* int
* float
* bool
* ... etc etc

PangolinMQTT will automatically calculate the length from whatever data type is given for the value;

### Example

```cpp
uint32_t myValue=0xdeadbeef;
std::string ss="abc";

mqttClient.xPublish("myinteger",myvalue); // publishes 4 bytes [ sizeof(uint32_t) ]
mqttClient.xPublish("mystring",ss)); // publishes 3 bytes [ ss.size() ]

```

---

## xPayload

### General form

```cpp

void xPayload(const uint8_t* payload,size_t len,[ VALUE REFERENCE ]);

```

Where [ VALUE REFERENCE ] is the name of the variable to hold the unpacked value, and may be pretty much any data type, e.g.

* const char*
* std::string
* String
* int
* float
* bool
* ... etc etc

PangolinMQTT will automatically calculate the length from whatever data type is given for the variable to hold the value;

### Examples

```cpp
int myInt;
mqttClient.xPayload(&payload[0],payload_length,myInt); // unpacks 4 bytes and puts binary value in myInt. ERROR if payload_length != 4
...
uint16_t myInt16;
mqttClient.xPayload(&payload[0],payload_length,myInt16); // unpacks 2 bytes and puts binary value in myInt16. ERROR if payload_length != 2
...
std::string ss;
mqttClient.xPayload(&payload[0],payload_length,ss); // unpacks <payload_length> bytes and constructs std::string containing those bytes.

```

See the [ExpertFunctions example](../examples/ExpertFunctions/ExpertFunctions.ino) for a working sketch using both `xPublish` and `xPayload`

---

## Find me daily in these FB groups

* [Pangolin Support](https://www.facebook.com/groups/pangolinmqtt/)
* [ESP8266 & ESP32 Microcontrollers](https://www.facebook.com/groups/2125820374390340/)
* [ESP Developers](https://www.facebook.com/groups/ESP8266/)
* [H4/Plugins support](https://www.facebook.com/groups/h4plugins)

I am always grateful for any $upport on [Patreon](https://www.patreon.com/esparto) :)


(C) 2020 Phil Bowles
