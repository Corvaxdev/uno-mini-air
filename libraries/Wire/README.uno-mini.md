# Project-local Arduino Wire

Copied from the pinned Arduino AVR Boards 1.8.8 package, library Wire 1.0.
Source: https://github.com/arduino/ArduinoCore-avr/tree/master/libraries/Wire
The exact core archive is recorded in ../../toolchain-lock.json.
Original copyright and LGPL notices are preserved in the source files.

The only source changes are the default BUFFER_LENGTH and TWI_BUFFER_LENGTH
from 32 to 22. All five buffers save 10 bytes each. The barometer calibration
read is exactly 22 bytes; all other requests and writes are smaller.
The original Wire/TWI timeout and interrupt implementation is retained.

tools/rebuild.py explicitly selects this library. The installed core and other
projects are not modified. The sketch asserts the selected buffer capacity;
revalidate this limit before adding a device requiring longer transactions.
