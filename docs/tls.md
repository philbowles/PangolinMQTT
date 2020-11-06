![plainhdr](../assets/pangoplain.jpg)
# Using TLS

## TLS support is available for ESP8266 targets only.

# Prerequisites

Unfortunately the standard [ESPAsyncTCP](https://github.com/me-no-dev/ESPAsyncTCP) at version 1.2.2 contains bugs in its SSL implementation which prevent a successful compile.

You must replace that library with this [bugfixed version](https://github.com/philbowles/ESPAsyncTCP)

# Code

You must provide a SHA1 fingerprint to confirm the server identity, and call `serverFingerprint` before connection, e.g.

```cpp
...
const uint8_t cert[20] = { 0x9a, 0xf1, 0x39, 0x79,0x95,0x26,0x78,0x61,0xad,0x1d,0xb1,0xa5,0x97,0xba,0x65,0x8c,0x20,0x5a,0x9c,0xfa };
...
  mqttClient.serverFingerprint(cert);
...
```

# Example sketches

Apart from the "QuickStart_X" and STM32 sketches, the examples include a simple `#define ASYNC_TCP_SSL_ENABLED` at the head of the sketch. Set your own `cert` finfingerprint and make sure `ASYNC_TCP_SSL_ENABLED` is defined - the rest happens automatically.
