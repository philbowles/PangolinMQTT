/*
MIT License

Copyright (c) 2020 Phil Bowles with huge thanks to Adam Sharp http://threeorbs.co.uk
for testing, debugging, moral support and permanent good humour.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#define PANGO_VERSION "1.0.0"

/*
    Debug levels: 
    0 - No debug messages
    1 - connection / disconnection messages
    2 - level 1 + MQTT packet types
    3 - level 2 + MQTT packet data
    4 - everything
*/

#define PANGO_DEBUG 0

#define ASYNC_TCP_SSL_ENABLED 0
// Don't forget to edit also async_config.h in the PATCHED ESPAsyncTCP lib folder!!!

#define PANGO_POLL_RATE      2
// per second - depend on LwIP implementation, may need to change as keepalive is scaled from this value
// e.g 15 seconds = 30 poll "ticks"

#define PANGO_MAX_RETRIES    2

// safety margin to measure biggest payload size
#define PANGO_HEAP_SAFETY    2048
