![plainhdr](../assets/pangoplain.jpg)
# Using TLS

## TLS support is available for ESP8266 targets only.

# Prerequisites

Unfortunately the standard [ESPAsyncTCP](https://github.com/me-no-dev/ESPAsyncTCP) at version 1.2.2 contains bugs in its SSL implementation which prevent a successful compile.

You must replace that library with this [bugfixed version](https://github.com/philbowles/ESPAsyncTCP)

# Compiling with SSL

SSL Features are NOT available by default. This is because they add about 64k to the binary, *even if you don't use them*! 

In order to use TLS, you first need to enable SSL in both the [bugfixed library](https://github.com/philbowles/ESPAsyncTCP): Edit `async_config.h` 
and PangolinMQTT itself: Edit `config.h`

In both files, find the line which says:
`#define ASYNC_TCP_SSL_ENABLED 0`

and change it to:
`#define ASYNC_TCP_SSL_ENABLED 1`

before compiling.

# Your Code

You must provide a 20-byte SHA1 fingerprint to confirm the server identity, and call `serverFingerprint` before connection, e.g.

```cpp
...
const uint8_t cert[20] = { 0x9a, 0xf1, 0x39, 0x79,0x95,0x26,0x78,0x61,0xad,0x1d,0xb1,0xa5,0x97,0xba,0x65,0x8c,0x20,0x5a,0x9c,0xfa };
...
  mqttClient.serverFingerprint(cert);
  ...
  mqttClient.connect(...
...
```

---

## Find me daily in these FB groups

* [Pangolin Support](https://www.facebook.com/groups/pangolinmqtt/)
* [ESP8266 & ESP32 Microcontrollers](https://www.facebook.com/groups/2125820374390340/)
* [ESP Developers](https://www.facebook.com/groups/ESP8266/)
* [H4/Plugins support](https://www.facebook.com/groups/h4plugins)

I am always grateful for any $upport on [Patreon](https://www.patreon.com/esparto) :)


(C) 2020 Phil Bowles
