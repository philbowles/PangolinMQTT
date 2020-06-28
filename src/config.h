#define PANGO_VERSION "0.0.7"

//#define PANGO_DEBUG

#ifdef PANGO_DEBUG
    #define PANGO_PRINT(...) Serial.printf(__VA_ARGS__)
#else
    #define PANGO_PRINT(...)
#endif

#define PANGO_POLL_RATE      2
// per second - depend on LwIP implementation, may need to change as keepalive is scaled from this value
// e.g 15 seconds = 30 poll "ticks"

#define PANGO_MAX_RETRIES    2

// safety margin to measure biggest payload size
#define PANGO_HEAP_SAFETY    2048
