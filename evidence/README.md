# Measurement scope

These are historical results from the deployed 0.12.3 Terra model. The same
Terra source is used by 0.12.6 and the public source edition.

- `model-24h/`: 13 scenarios x 20 seeds; 240 ordinary runs retain both species,
  all 20 hot/dry/dim runs collapse.
- `model-72h/`: held-out seeds 21..40, 3 scenarios; 59/60 retain both species,
  one constant-room run loses predators. These are simulations, not measured
  biological outcomes or universal survival guarantees.
- `hardware-probe.json`: diagnostic firmware, 128 occupied animal slots,
  90 sensor/world request pairs from one external VPS (180 responses).
  Static SRAM was 1503 B because instrumentation added 12 B. The 413 B gap is
  observed in that workload, not a worst-case stack proof or current capacity.

An earlier pre-Terra direct HTTP 10-client repeat lost connectivity and needed
a manual reset. Cause remains unconfirmed. No claim of stable 10-client
capacity is made. This publication does not run load against the live board.
