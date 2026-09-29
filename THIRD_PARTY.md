# Third-party notices

The root MIT license applies to original AIR / 328 source, web UI, bitmap
patterns, tooling, tests and documentation by Corvaxdev. It does not replace
the following upstream licenses or their copyright notices.

| Component | License | Origin / local changes |
|---|---|---|
| `libraries/MiniW5500` | GPL-2.0-only OR LGPL-2.1-only; this project uses the LGPL option | Arduino Ethernet 2.0.2 register transport, specialized for ATmega328P / W5500 / CS D10 / SPI mode 3 |
| `libraries/Wire` | LGPL-2.1-or-later | Arduino AVR Boards 1.8.8 Wire; five buffers reduced from 32 to 22 bytes |
| Arduino AVR core / SPI | Upstream licenses, including LGPL-2.1 and the SPI GPL-2.0-or-LGPL-2.1 choice | Installed by Arduino CLI, not vendored here; exact archive/checksum in `toolchain-lock.json` |

Original notices remain in every upstream source file. License texts are in
`licenses/`. The modified libraries' complete source is included. When
redistributing a linked firmware binary, comply with the applicable LGPL
terms, including the user's ability to relink with modified library code;
do not label the combined binary as MIT-only. This repository publishes
source and build instructions, not prelinked firmware binaries.

Build-time dependencies are installed separately: Terser (BSD-2-Clause),
Zopfli (Apache-2.0), Arduino CLI (GPL-3.0), AVR GCC/binutils/avr-libc under
their respective licenses and runtime exceptions. Their own distributions
carry the applicable notices; the npm lockfile pins JavaScript dependencies.

Upstream sources:
- https://github.com/arduino-libraries/Ethernet/tree/2.0.2
- https://github.com/arduino/ArduinoCore-avr
- https://opensource.org/license/mit
- https://www.gnu.org/licenses/old-licenses/lgpl-2.1.html
