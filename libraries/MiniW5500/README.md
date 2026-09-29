# MiniW5500

Project-local specialization of the Arduino Ethernet 2.0.2 register transport
(`libraries/EthernetMode3/src/utility/w5100.*`). Original copyright and the
GPL-2.0-or-LGPL-2.1 license choice are retained in both source files.

Supported hardware is explicitly ATmega328P at 16 MHz, W5500, CS D10/PB2,
SPI mode 3 at 8 MHz. Four HTTP peers use existing 2 KiB W5500 socket banks.
No network memory is borrowed for application state. The driver retains the
original transfer loops, bounded reset/identity checks and transaction framing.
Other Ethernet chips, generic pin selection, DNS, DHCP and IPAddress objects
are intentionally absent. `W5100Class` / `W5100` names are retained internally
to minimize changes to the already-tested AirSocket state machine.

W5500 RTR is at 0x0019..0x001A and RCR at 0x001B, unlike W5100. Firmware sets
2000 x 100 us = 200 ms and three retries. This also corrects the legacy local
driver's W5100 register constants. The prior calls wrote 0x07 to SIR(0x17),
0xD0 to SIMR(0x18), and 0x03 to RTR high(0x19); with default low0xD0,
RTR became0x03D0=97.6 ms while RCR stayed8. This behavior change needs a real
network regression, especially with packet loss; host tests validate registers.

References:
- https://github.com/arduino-libraries/Ethernet/tree/2.0.2
- https://github.com/Wiznet/ioLibrary_Driver/blob/master/Ethernet/W5500/w5500.h
- https://docs.wiznet.io/Product/Chip/Ethernet/W5500/datasheet
