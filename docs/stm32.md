![plainhdr](../assets/pangoplain.jpg)
# STM32 Targets

## Treat as "Experimental"

The code has only been tested *only* on STM32-NUCLEOF429ZI using its built-in Ethernet adapter.

Furthermore it has only been tested using the ST-approved boards manager.

*In theory* it should work on any device with e.g. an Arduino Ethernet shield, but... you're into unknown territory.

# Prerequisites

* [ST boards 1.9.0](https://github.com/stm32duino/Arduino_Core_STM32)
* [LwIP](https://github.com/stm32duino/LwIP)
* [STM32Ethernet](https://github.com/stm32duino/STM32Ethernet)
* [STM32AsyncTCP](https://github.com/philbowles/STM32AsyncTCP)

# Example sketches

* [STM32 Tester](../examples/STM32_TESTER/STM32_TESTER.ino)

# Known issues

See [elsewhere](../README.md#important-note-for-platformio-users) for problems when using PlatformIO due to that environment being broken for a long time.

The main problem is its non-standard use of architecture defines. The test sketch will probably fail compilation and pull in ESP libraries unless you use the ***correct*** define `ARDUINO_ARCH_STM32` in your build options.

As mentioned in that other document, I will *not* provide any support for PlatformIO build issues and that's final.
