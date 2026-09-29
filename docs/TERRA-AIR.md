# Room Terra: air response and balance, 0.12.3

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

At23C,50%RH,353lx and101325Pa, actual compiled-model coefficients:

| CO2 ppm | Growth base | Relative growth | Relative energy demand |
|---:|---:|---:|---:|
|400|211|-4.5%|0%|
|600|221|reference|reference|
|1000|234|+5.9%|+3.1%|
|2000|251|+13.6%|+10.9%|
|5000|255|+15.4% (cap)|+18.4%|

## Balance corrections

The previous model loses both species in20/20 day-long runs at the measured
stand conditions28.7C,37.6%RH,246lx. Reducing growth makes this worse. Trials
of wider temperature response alone and smaller shelters alone also fail to
retain predators in this scenario. See all rejected variants in the experiment.

The selected combination:
- Photosynthesis saturates with light: L=clamp((lux-10)*255/490,0,255),
  photo=floor(383*L/(L+128)). This preserves darkness and the maximum, and
  improves use of moderate room light. Equivalent16-bit integer evaluation
  avoids an extra32-bit multiplication/division. Night thresholds are unchanged.
- Temperature factor: max(0,1-|T-23|/25), with integer quantization.
- Animal temperature factor: clamp(1+(T-23)/40,0.5,1.5).
- Four4x4 shelters replace four8x8 shelters:64 protected cells,6.4% of land.
  The canvas and firmware share this geometry; the pond remains24 cells.
- The display shows biomass/3000 instead of occupied plant cells/1000. A fully
  covered map can still have little stored food; biomass makes that visible.

Birth food budgets and population-density feedback stay active. There is no
rescue population, scheduled catastrophe or guaranteed coexistence. The dead
capacity-block birth counter was removed: birth chance is zero at full capacity,
so that failure branch cannot increment it. Snapshot bytes46..47 remain zero;
packet length480 and512B HTTP workspace remain compatible with previous clients.

## Validation

- 666,470 arithmetic and neutral-reference snapshot cases under ASAN/UBSAN.
  Full uint16 CO2 range, invalid pressure extremes, monotonicity, bounded filter,
  fallback, extreme temperature and unchanged neutral-air outcomes were checked.
- Pond/shelter, capacity, action cost, cooldown, night, snapshot bounds,
  reader independence and millis rollover invariants pass.
- Final model:13 scenarios x20 seeds x24h =260 simulated days. All240 ordinary
  runs retain both species;20/20 hot/dry/dim cases collapse. Cases include CO2
  400..5000ppm,95..105kPa, ventilation, spikes, missing CO2 and day/night.
- Held-out seeds21..40,72h each: normal20/20, current room19/20, changing room
  with day/night20/20 retain both species. One room run loses predators only;
  all herbivore populations survive. This is an observed sample, not an
  estimated universal extinction probability or a long-term guarantee.

The physical diagnostic measurements and final Flash budget are recorded in
[hardware-probe.json](../evidence/hardware-probe.json). Model CSVs are in
[evidence](../evidence); host test sources are in `tests/`.

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
