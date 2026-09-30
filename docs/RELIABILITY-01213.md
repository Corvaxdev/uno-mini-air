# Reliability audit — 0.12.13

The preceding [0.12.12 RX fix](RX-BOUNDS.md) prevents a changing W5500 receive count from escaping the copy bound. That defect could overwrite sensor and view-counter state. This release addresses the other confirmed issues from the review.

## Corrections

- **Slow page responses:** the previous four-second absolute deadline could truncate a progressing transfer. Completed chunks now refresh a four-second idle deadline, with a twelve-second total cap. The consumed request counter stores the response start time, using no extra SRAM. Request headers remain bounded to 1.5 seconds and 2,048 bytes.
- **Clock rollover:** an old action timestamp could make an expired cooldown recur after about 49.7 days. World service rebases expired timestamps, while preserving active cooldowns across rollover.
- **Cell feedback:** an old global action result could replace a newly selected water/shelter hint. The browser consumes a result only after its own accepted command and a subsequent step; reselection and restart clear that expectation.
- **Sensor storage:** unused sample/error/max-cycle counters were removed. 30,000 differential cases compare values, freshness, timing and I2C transactions with the prior drivers.

## Verification

The [15-group host suite](../evidence/host-tests-01213.json) includes the actual extracted HTTP state machine (126 cases), 3,006,507 cooldown assertions over two millis() periods, sensor failures, I2C bus recovery and 3,310,720 barometer arithmetic comparisons. RX bounds, placement, model, header and EEPROM fault regressions also pass under ASAN/UBSAN.

Browser checks cover stale and awaited action results, visibility and page lifecycle, 127 packed animal entries, capacity and world restart, plus 48 route/viewport/system-theme combinations. The built document runs with synthetic inputs and external traffic blocked.

After upload, all sensors were checked in 31 observations over five minutes, allowing CO2 warmup. The counter survived upload, all four page hashes matched, and the world advanced. An owned Amsterdam VPS completed **80/80 ordinary requests** with four mixed workers over 90 seconds; a paced receiver obtained the complete **11,399-byte** gzip page in **5.868 s**. No retries. This is a bounded smoke test, not a maximum-load result or an RTT simulation. [Structured results](../evidence/reliability-01213.json).

Production uses **32,076 B Flash / 1,465 B static SRAM**, saving **148 B / 28 B** from 0.12.12. Flash headroom is 180 B. The 583 B above globals must accommodate stack and interrupts; no new peak stack margin was measured. The 512 B workspace and page chunk count remain unchanged. The public example-network build is measured separately in [public-build.json](../evidence/public-build.json).

## EEPROM and limits

The affected installation required a separate recovery: two matching full backups, invalidation of five corrupted checkpoint markers, and complete readback after reset. The other 1,019 bytes remained unchanged. The last trustworthy count was recovered; later lost visits cannot be reconstructed. Firmware replacement alone does not fix existing corrupted checkpoints. Preserve EEPROM before any installation-specific repair.

Four TCP peers can still be exhausted by sustained traffic. Deadlines bound held connections but cannot guarantee availability during a flood. Historical load failures remain in [LOAD.md](../evidence/LOAD.md); this smoke test does not resolve their original cause conclusively. No physical power dip, bus short or weeks-long uptime test was performed.

Direct HTTP is intentional and provides no transport confidentiality or authentication. Public actions have placement, reserve and cooldown guards; there are no user accounts or a remote firmware-upload endpoint. Production retains its existing external analytics script; the public edition excludes it. Host sanitizers do not prove AVR stack safety. The findings above are fixed and tested; this is not a claim that every possible defect has been found.
