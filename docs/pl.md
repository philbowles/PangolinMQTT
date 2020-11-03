![plainhdr](../assets/pangoplain.jpg)
# MQTT Payload handling

***N.B. You MUST read this before using any of the 'x' (expert) functions.***

## MQTT payloads

An MQTT payload consists of an arbitrary block of data and a length.

The ***ONLY*** correct type for the address of the block of data on ESP8266 / ESP32 / STM32 is `uint8_t*` (or `byte*`)

Given that PangolinMQTT allows for much bigger payloads than most other libraries (up to 0.5* the amount of the free heap) its defintion is `size_t`.

## Simple publish

```cpp
void publish(const char* topic,uint8_t* payload, size_t length,  bool retain=false, uint8_t qos=0);
```

The most common case of a simple, fixed non-retained message @ QoS0 is reflected in the defaults, such that the majority of calls will resemble:

```cpp
#define MY_PLENGTH 42
uint8_t my_data[MY_PLENGTH];
...
mqttClient.publish("my/topic/subtopic",my_data, MY_PLENGTH);
...
```

Obviously the length does not need to be fixed, nor does he buffer need to be statically declared; for variable-length payloads, the user may define / allocate the data how he/she sees fit. It is then up to the user to know or calculate the length of the buffer.

```cpp
...
mqttClient.publish("my/topic/subtopic",some_data_pointer, some_data_length);
...
```

For differing payloads, the length calcualtion ***must*** be correctly performed and the memory allocated prior to every call - and usually freed after it. This is both time-consuming and error-prone, hence the `xPublish` variants exist to do this routine work for you correctly in all cases depending on the type of data being sent.

If you don't understand the last paragraph, then simply stick to the simple `publish` call and ignore the rest of this document.

## Received payloads

First, remind yourself that an MQTT payload consists of an arbitrary block of data and a length. The MQTT specification has ***nothing*** to say about what that data represents. Only the programmer / system that placed the data and its correct length in the message actually "knows" what the data ***should*** "look" like. Did the sender publish a printable string? A JSON-formatted printable string? A 32-bit (4 byte) binary value? Who knows? *Only the sender*.

Thus there can be no escape from the fact that it is up to the user to "unpack" the payload and "reconstruct" the data that he/she *thinks* it represents. In a perfect world, it will always look as expected and be within acceptable range / bounds / length of what is expected. In our imperfect world, any programmer relying on the previous assumption has just created a massive bug. Don't let that programmer be *you*.

### Golden rule 1

***NEVER*** Assume you *know* what the contents of the payload and its length are. Despite what you think, *they **can** be anything*, If you don't confirm they are "sensible" for your app, you have just created a bug.

### Golden rule 2

***ALWAYS*** Validate the length of the payload as being within the bounds of acceptable lengths for the expected content. For example, if you are expecting a 32-bit (4-byte) binary value and the length is 8 then *you have a problem*. Reject the message with some sensible user logging.

### Golden rule 3

***MQTT payloads are NOT strings (of any kind!)*** They are - as we already know - an arbitrary block of data and a length.

Let that sink in for a moment. In the vast majority of cases, users will send "string" payloads and hence they may find that a confusing statement. Let's state it another way: Most payloads consisting of a series of printable characters that most people think look like "strings" of some kind. But MQTT is perfectly happy to accept ***any*** character whether its a valid printable "string" character or not: "0x01" for example. From MQTT's point of view your data of "mydata" is just a 6-byte payload. As is `{0x01,0x02,0x03,0x04,0x05,0x06}` which is most definitely ***not** anything like anything anyone would call any kind of "string". But its a perfectly valid 6-byte MQTT payload.

Most things you put in the mail are letters. But some people post parcels. Or postcards. To the mail company, they are all just "postal items". Yes, 90% of mail is made up of "letters" in the same way that 90% of MQTT payloads contain printable strings. But only a fool would treat *all* postal items as "letters". Only a programmer who enjoys debugging his own errors will treat MQTT payloads as "strings". They are arbitrary blocks of data plus a length. End of story.

## "Unpacking" a payload safely

The correct way to "unpack" a payload then is:

1   Validate that it contains a sensible length for the expected data type else reject and skip all remaining steps
2   Allocate (or have previously statically declared) a buffer of the correct length
3   Copy <length> bytes of data from the payload to your own buffer
4   use the data
5   Free the buffer (if previously allocated dynamically)

Skipping any of these steps ***will*** create a potential bug. Whether that bug actually bites you is a matter of luck. Let's look at some examples.

### Example 1 - fixed buffer

Your app receives positive temperature values in topic "temp" as up to 3 ASCII characters in the range 0-999. This means that your payload can be length 1,2 or 3 so as long as you receive "1" or "25" or "666" everything should be fine.

Now consider the following code:

```cpp
char tmp[4]={0,0,0,0};
memcpy(tmp,payload,length);
printf("Temp is %s\n",&tmp[0]);
```

Because you have cleverly allowed for the fact that even a 3-character payload wil be NUL-terminated, you can treat tmp as a C-style string.

Next, remember that ***anyone*** can publish any topic to MQTT. Then imagine me publishing "ABC" to "temp". There is nothing you can do to stop me - or anyone else - doing that, and you code will break. Not fatally, but your tmep will be wrong and even more wrong if you try to convert it to an int and do maths on it, because you didnt validate that what was received was numeric.

Now let's ramp up the volume and publish "1234567890" at you. You will now write 10 bytes of data into a 4-byte buffer: pretty much anything can happen, but most likely, you will get a crash and reboot. You did not confirm length was < 4 before proceeding.

### Example 2 - dynamic buffer

Your app expects to receive someone's name in topic "name". It can be up to 256 bytes long. To avoid holding on to an expensively large buffer, you allocate it dynamically each time. You want to treat it as a string.

```cpp
uint8_t* buf=malloc(length+1); // C-strings are NUL terminated!!! MQTT payloads are not!
memcpy(buf,payload,length);
buf[length]=\0; // poke in the terminating NUL to make buf LOOK LIKE a C-string
printf("Name is %s\n",static_cast<char*>(&tmp[0])); // coerce buf to correct type to treat as C-string
free(buf); // memory leak if you forget this
```

Better? No, same problem as above: If I publish a 300-byte string to "name" your code will crash.

Also, if you forget the +1 or adding the final \0 then you *may not* have a properly terminated string and again your code will probably crash. The worst problem with this code is that you *accidentally* may get what *looks like* a valid string. You *may* be handling corrupt data, or your code *may* work or it may crash, apparently randomly, since each different length message will behave differently. Try debugging *that*.

Finally, if you forget the `free(buf);` call you will eventually run out of memory and crash. How soon that happens will depend entirely on what messages you receive and how long each is.

### Example 3 - you think you know better...

All of this sound like and unnecessarily complex process: you know the payload is a string so you are just going to use the payload pointer, like this:

```cpp
printf("Name is %s\n",static_cast<char*>(&payload[0])); // coerce buf to correct type to treat as C-string
```

This is probably *THE* most common payload handling error / potential bug. In many cases, but ***purely by chance*** the library's internal buffer *could be* followed by a NUL byte (\0) in which case your code will work as expected. On the (*possibly totally random*) occasions when it isn't you will get a) unexpected results or b) a crash, both of which will be almost very difficult - if not impossible - to replicate and/or debug.

## xPublish

For all of the above reasons, the xPublish variants exist to do most of that "uncessarily complex" (but *essential*) processing for you and calculate the correct payload length for the chosen data type. This then allows you to reverse the process simply with the matching `xPayloadAsXXX` calls, to directly create / assign a variable of the chosen type from the expected value in the payload.

Obviously this is only going to work if you use matching `xPublish`and `xPayloadAsXXX` calls. Publishing  an int and then unpacking it as a string in't goiung to work! Nor is using the bog-standard `publish` then using a specific `xPayloadAsXXX` call - unless you deliberately and pointlessly replicate all the correct code that the corresponding `xPublish` call would have done.

While some simple validation / rejection can be done by the  `xPayloadAsXXX` calls, ***it is always up to the programmer to ensure the payload data is valid before acting upon it***. For example: `int my32bits=xPayload(payload,length);` can reject any payload with a length not=sizeof(int) it cannot make sure it is +ve for example if that's what you are expecting.

In summary: 

1 ***DO NOT MIX AND MATCH 'x' CALLS WITH STANDARD publish***
2 ***ALWAYS USE MATCHING 'xPublish' and 'xPayload' CALLS***
3 ***If you don't "get" all of this, stick to simple publish and do your own unpacking***

