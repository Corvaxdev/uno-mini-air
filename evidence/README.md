# Measurement scope

- `public-build.json`: independently compiled public 0.12.10 source, example
  network and no deployment analytics. It is not the deployed binary.
- `source-parity.json`: normalized source hashes matching the deployed runtime;
  network constants are intentionally substituted. Assets are separately rebuilt.
- `host-tests.json`: current ASAN/UBSAN suite, including light dose, shelters and water placement.
- `model-0129/`: accepted current model, seeds 201..220 (201..210 for 10-run
  cases), no visitor actions. Three ordinary 72-hour room cycles: 48/50 coexist,
  two predator extinctions, no full collapse. Extreme runs can collapse.
- `model-24h/`, `model-72h/`: historical **0.12.3**, prior light model. They do
  not describe current 0.12.9 behaviour. Kept so older results remain traceable.
- `hardware-probe.json`: historical **0.12.3**, 128 occupied animal slots,
  instrumentation added 12 B of static SRAM. Max slice 4.324 ms, minimum observed
  stack gap 413 B, 180/180 responses in 90 seconds. Not a worst-case bound.
- [LOAD.md](LOAD.md): **0.11.15-hidden**, two external VPS, historical load
  results including failed requests and the manual-reset incident.

Current build headroom is not measured peak free RAM. Current smoke tests and
model experiments do not establish multi-client Internet reliability.


Placement fix 0.12.10: see [checks](placement-01210.json). Diagrams, day/night images and model/load measurements remain versioned 0.12.9 or earlier; their labels identify the measured revision. Current build sizes are in the root README and evidence/public-build.json.


0.12.11: [reliability checks](reliability-01211.json) and [nine host test groups](host-tests-01211.json). Current build sizes are in the root README.


0.12.12: [RX bound evidence](rx-bounds-01212.json) and [cause and fix](../docs/RX-BOUNDS.md).
