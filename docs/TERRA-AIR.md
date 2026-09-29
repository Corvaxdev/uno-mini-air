# Room Terra: air response and light model

The new model uses CO2 and absolute barometer pressure. Its response coefficients
are game-design parameters, not a measured biological model or an air-quality
health index. Raw sensor readings and calibration are unchanged.

## Coupling

Let C be measured CO2 in ppm and P absolute pressure in Pa. The internal CO2
partial-pressure equivalent at standard pressure is E = C * P / 101325.
This is proportional to CO2 partial pressure, not an altitude/sea-level correction.
Pressure does not independently label air as clean or dirty. Typical weather
changes produce small effects; no invented pressure danger threshold is used.

AVR stores round(E/25) in one byte, bounded by validated inputs C=400..5000 and
P=30000..110000. Values are 5..217. Every10s the state advances by delta/8,
with a minimum one-unit step near the target. The initial large-change time
constant is about75s; the last small difference closes in25ppm steps. No queue,
history, floating-point library or EEPROM writes are added.

Invalid CO2 smoothly returns the model to600ppm equivalent. Missing pressure
uses101325Pa. The UI marks missing inputs. Temperature/humidity/light retain their
existing hold-then-fallback logic. CO2 warm-up does not stop the world.

## Response curves

Using the filtered E, before integer quantization:

- Plant multiplier: 1 + (E-600) / (4*(E+600)).
- Animal energy-use multiplier: 1 + max(0,E-800) / (4*(E+800)).

These are bounded saturating curves. The800ppm knee is a gameplay parameter;
it is not a safety standard. There is no direct CO2 death or forced respawn.
Higher CO2 can supply plants while increasing animal demand. Predators still
need real prey; herbivores still consume real biomass. All gains are evaluated
once per world boundary, not per cell or animal.

Implementation uses integer gains:
G=64-floor(3072/(A+24)); load=max(0,64-floor(4096/(A+32))), where A=E/25.
Plant precomputed gain remains capped at255, so the effective bonus also depends
on light and temperature. Metabolism adds ((rate>>4)*load)>>4; its intermediate
fits16 bits over the whole validated range.

Historical 0.12.3 coefficients at23C,50%RH,353lx and101325Pa (before 320lx normalization):

| CO2 ppm | Growth base | Relative growth | Relative energy demand |
|---:|---:|---:|---:|
|400|211|-4.5%|0%|
|600|221|reference|reference|
|1000|234|+5.9%|+3.1%|
|2000|251|+13.6%|+10.9%|
|5000|255|+15.4% (cap)|+18.4%|

## Current validation

The current suite checks 656,470 CO2/pressure arithmetic cases separately from
light-debt accumulation. See `tests/air.cpp` and `tests/light.cpp`.
Older population results in evidence/model-24h and model-72h belong to 0.12.3;
current light-cycle experiments are in evidence/model-0129.

## Sources and interpretation

[USDA ARS](https://www.ars.usda.gov/oc/dof/growing-plants-in-a-hotter-world/)
describes interacting CO2, water, temperature and nutrient effects. This motivates
bounded plant response; it does not supply our numerical coefficients.
[NASA gas formulation](https://www.grc.nasa.gov/WWW/wind/TFAWS2007/Formulation.pdf)
describes species partial pressures and Dalton's law.
[ASHRAE CO2 position document](https://www.ashrae.org/file%20library/about/position%20documents/pd_indoorcarbondioxide_2022.pdf)
distinguishes ventilation-related CO2 information from a complete indoor-air
quality assessment. We therefore expose an ecosystem response, not an AQI.

## Readouts (0.12.5)

Terra shows raw CO2 in ppm and pressure in mmHg. X-Air carries fresh raw
CO2ppm,pressurePa in the existing /life HTTP header;0 independently marks each
unavailable reading. The browser reuses its Pa-to-mmHg conversion. These are
current readings at header generation; the model still filters its partial-
pressure equivalent every10s. Snapshot480B, buffer512B; no extra API polling.


## Room-light calibration (0.12.8)

The room's measured maximum is approximately320lx. This supersedes the0.12.3
light normalization above: L=clamp((lux-10)*255/310,0,255), with explicit endpoint
clamps before unsigned multiplication. Raw lux readings are unchanged.
Sleep at<=25lx and wake at>=41lx still require18 consecutive10s samples.
The26..40lx hysteresis band retains the current mode. Missing-data fallback is
227lx (L=178), preserving the former353lx neutral normalized light.

Rendering uses existing snapshot bytes15/17: DAY overlay alpha=(255-L)/1020;
NIGHT alpha=0.5, RGB16/32/68. Terrain dims before animals and selection are drawn.
There are no new polls, packet bytes, model fields or animation loops.

The current tests include the normalization boundaries.

## Ongoing night ecology and light stress (0.12.9)

This supersedes the frozen-night behavior. Night mode still qualifies at<=25lx
for3minutes; wake qualifies at>=41lx for3minutes. Environment/snapshot boundaries
remain10s, but ecological passes run once per32 boundaries at night (320s).
Plants have negligible/no production in dim/zero light; animals still consume
food and energy during these passes. Ordinary nights permit rest; long darkness
can exhaust stores. Temperature/humidity/CO2/pressure interactions remain active.

One new uint8_t lightDebt captures cumulative absence of rest and excess light.
Every64 generations (640s): day adds clamp(floor(lux/320),1,8), saturating255;
night subtracts3, saturating0. At debt<=128 growth is unchanged. Above128:
growthBase=floor(growthBase*(255-lightDebt)/128). No forced death, rescue or random
catastrophe is added. Food scarcity, predation and existing birth rules produce
the population outcome. These are game-design coefficients, not plant-care or
health thresholds. User-stated320lx is full normal light, not a stress trigger.

A16h/8h cycle peaks at90..91 debt and clears overnight. Continuous320lx starts
reducing growth after approximately23h; continued exposure can suppress it fully
after approximately45h. At2560lx the debt accumulates8x faster. Darkness can
clear maximum debt in approximately15h; already extinct species do not respawn.
Exact edge time is quantized to640s and includes the3-minute mode qualification.

Evidence: [model-0129](../evidence/model-0129) and current host tests. 519,494 ASAN/UBSAN checks cover full uint16
lux range at7 debt values, uint32 lux extremes, saturating decay,8-bit bounds,
independent growth arithmetic,7-day ordinary dose cycle and exact night cadence.
Existing population, geometry, snapshot, actions, reader independence and timer
wrap invariants pass. A narrowed AVR expression was measured and rejected because
its compiled image grew38B. Final code uses the smaller measured expression.

Deterministic seeds201..220 (first10 for10-run cases), no visitor actions:

| Conditions | Runs | Duration each | Both species survive | Complete collapse |
|---|---:|---:|---:|---:|
|23C,50%RH,320lx,16h/8h cycle|20|72h|19|0|
|29C,36.3%RH,238lx,16h/8h cycle|20|72h|19|0|
|23C,45%RH,80lx,16h/8h cycle|10|72h|10|0|
|23C,50%RH,320lx continuous|10|168h|0|10|
|23C,50%RH,0lx continuous|10|168h|0|6|
|23C,50%RH,2560lx,16h/8h cycle|10|72h|0|10|

The other4 prolonged-dark runs retain1..3 herbivores and no predators. Ordinary
cycles lose predators in2/50 runs, never both species. Counts describe these
seeds and durations, not universal probabilities. The initially tested1/8 night
rate was rejected:14/20 ordinary measured-room runs fully collapsed within72h.
The rejected rate is documented here; the published CSVs contain the accepted 1/32 model.

Flash32250/32256B; static SRAM1492/2048B (+1B); world584B. Gzip11385B,480B world
snapshot,512B workspace and23 HTML body sends. The report was shortened to fund
the model; buffering/poll frequency were not reduced. No current load-capacity
claim is made; retained physical timing results explicitly refer to0.12.3.


### Shelter visibility and action validation (0.12.9)

The four4x4 shelters now use a lighter green fill and thin pale-green outlines,
drawn after light tint but before animals/selection. The legend identifies them.
Selecting a shelter shows "Shelter: no predators." The predator button remains
disabled and its cost caption becomes "SHELTER" until selection leaves refuge.
Herbivores/seeds remain available subject to normal reserve/cooldown rules.
The existing MCU validation remains authoritative even for direct commands.

Browser checks cover all64 shelter cells and adjacent outside cells: no command
on selection, predator disabled, herbivore enabled with adequate reserve, normal
predator cost/enabled state restored outside. 2,048 ASAN/UBSAN direct action cases
cover both species across all1,024 cells; rejected placements spend no reserve.
No MCU model/packet fields are added for this UI improvement. The old detailed
performance records remain archived; visible history/meta descriptions were
shortened to fit. Final image:32250B Flash,1492B static SRAM,11385B gzip.
