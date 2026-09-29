# RX bounds / 0.12.12

The server requested at most 128 bytes from W5500 per loop, using
`min(size(s,false),capacity)`. The Arduino AVR core implements min as a macro:
its first argument can be evaluated twice. A packet arriving between those
evaluations could make the copy exceed capacity and the 512-byte workspace.
The adjacent globals include the I2C sensor objects and EEPROM view journal.

The fix reads a stable count once and clamps that saved value. It adds no
static RAM, preserves the existing buffers and avoids the duplicate counter
read. The defect is present in archived 0.8.0 through 0.12.11.

Validation:

- Original header with a hardware-counter mock: 0 then 800 available bytes
  produced an 800-byte copy despite capacity 128; the post-buffer guard changed.
- Fixed header: 81,962 ASAN/UBSAN cases cover changing counts, capacities
  0/1/22/128/512, unstable counters and a pending hardware command.
- Both calls are visible in the original AVR disassembly; the fixed RX branch
  contains one call.
- Deployed board: 24 fragmented ordinary HTTP requests with 600/1400-byte padding
  headers succeeded. 31 sensor samples over five minutes had no missing I2C
  readings; CO2 was checked after warm-up. Four pages matched their asset hash
  and the world advanced.

The exact packet responsible for the observed live failure was not captured.
The reproduced overwrite matches the failed sensors and corrupted view count,
but this short verification is not a long-term reliability or load-capacity
guarantee. Existing HTTP deadline, UI hint and long-uptime cooldown findings
are separate from this patch.

A corrupted RAM count can be checkpointed with a valid EEPROM CRC. Updating
the firmware does not reconstruct lost visits. Back up the complete EEPROM
before any installation-specific journal repair; do not erase it on upload.
