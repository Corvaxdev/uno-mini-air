# Room Terra РІР‚вЂќ 0.12.3

The UNO Mini owns the entire shared ecosystem. It advances without browsers,
the Internet, a Raspberry Pi or an application server. W5500 transports HTTP;
the browser only renders snapshots and submits bounded actions.

System light/dark theme follows live OS changes, with no manual preference.
The shared reserve is a 36px number in the left column (above the map on phones).
The deployed 0.12.6 reference uses 32,256 B Flash and 1,491 B static SRAM.
The public build removes deployment analytics and uses example networking;
see README for its independently measured resource use.

## Memory and scheduling

- 32 x 32 torus, four orthogonal neighbours; one step per 10 real seconds.
- Plants: 256 B, two bits per cell (biomass 0..3).
- Animals: 256 B, 128 fixed 16-bit records. Position: 10 bits; energy: 3;
  species: 1; acted-this-step: 1. Empty records are zero. No allocation.
- Complete AVR world state: 583 B, including environment, timing and counters.
- Snapshot: 480 B. Animals use 11 wire bits each (position + species);
  energies remain private MCU state. Counts determine the occupied wire records.
- The original HTTP workspace remains 512 B. The header and 480 B world body
  are separate sends. Snapshot generation waits for a completed world pass;
  it never advances the world or consumes random numbers.
- A loop iteration processes one plant cell or one animal slot, then returns
  to sensors, Ethernet and watchdog servicing. No full duplicate world buffer.
- Four sockets, the existing sensor drivers and EEPROM view journal remain.
  The ecosystem itself is not written to EEPROM. A reboot resets its world.

## Mathematical model

All calculations on AVR are integers. Probabilities use a 16-bit threshold
against the upper 16 bits of a xorshift32 result. Quantization makes these
equations approximate; `firmware/UnoMiniEthernet/Terra.h` is authoritative.

Let L = clamp((lux - 10)/490, 0, 1), Mtarget = clamp((RH - 20)/50, 0, 1).
Virtual soil approaches Mtarget with `M += (Mtarget-M)/180` per active step
(approximately a 30-minute time constant). Soil has eight fractional bits,
avoiding the large dead band of a whole-percent accumulator.

Effective light is saturated: photo=floor(383*L8/(L8+128)), L8=rounddown(255*L).
Temperature modifies plant growth by clamp(1 - |T-23|/25, 0, 1), and animal
metabolism by clamp(1 + (T-23)/40, 0.5, 1.5). Horizontal light/moisture gradients
and the wetter shore create local variation.

Plant growth probability is approximately
`photoLocal * temperatureFactor * CO2factor * (0.25 + 0.75*Mlocal) / 32` per step.
Empty cells receive 1/4 of this rate beside vegetation, otherwise 1/64.
Dry soil below 0.25 also permits decay, processed before growth.

Herbivores prefer safety, then food; feeding consumes one biomass unit and
adds three energy, capped at seven. Predators hunt unprotected adjacent prey
when hungry, gaining four energy. Energy expenditure probabilities are k/8
and k/16 respectively. Zero energy kills an animal; there is no automatic rescue.

Births split energy 7 into 3 + 3. They also require a free neighbouring cell,
and for herbivores sufficient local food and low local crowding. Two explicit
feedback terms damp boom/bust and avoid living against the storage ceiling:

1. Global food budget: biomass >= 8*herbivores; for predator births,
   herbivores >= 8*predators.
2. Density factor: `1 - (herbivores+predators)/128`. It multiplies the base
   birth probabilities 1/32 (herbivore) and 1/512 (predator).

This is a stochastic spatial ecosystem with energy accounting, not a calibrated
biological forecast or a closed physical conservation model. Rare extinctions
are permitted. Twenty successful seeds do not establish an extinction rate.
Power-up uses a reproducible default seed; browser reads do not perturb it.

## Pond, shelters and night

Four 4 x 4 refuges protect herbivores and exclude predators. Protection is used
in both attack and danger evaluation. Refuges supply no free food.

A central 24-cell pond is computed from coordinates: no terrain bitmap in RAM.
Water excludes plants and animals. Nearby land receives up to 64/255 extra
virtual moisture, clamped at one; the pond is a fixed reservoir, not a water
level simulation. The browser renders the identical geometry.

Darkness is measured by the room light sensor, not wall-clock time. Eighteen
successive 10-second readings below the quantized night threshold (<=25 lx)
pause biology. Eighteen above the wake threshold (>=41 lx) resume it. The gap
provides hysteresis. Sensors, HTTP, credit refill and visitor actions continue
during dormancy. There is no invented countdown to sunset.

Missing inputs retain their previous value for five minutes, then approach
marked fallback values (23 C, 50% RH, 353 lx). This affects only simulation
inputs; the sensor API still reports missing measurements as null.

## Shared actions

Cell selection is local browser state. Click/tap or arrow keys only move the
selection; the three action buttons submit the corresponding command. No cell
is selected initially. Buttons require a snapshot and selected cell, sufficient
reserve and no pending request/action or shared cooldown. Selection remains
possible while actions are blocked; Enter/Space on the map never submits.

`PUT /life/XYZ` is a bodyless hexadecimal command. Low 10 bits are the cell;
the next two bits select seed / herbivore / predator (codes 0..2).
The parser rejects commands >=0xc00. A 202 response means queued, not applied.

Seed 3 x 3 empty land cells: 1 credit; herbivore at energy 3: 3 credits;
predator at energy 3: 8 credits. The common reserve is 12, with one credit
refilled every 10 real minutes. Successful actions start a global 60-second
cooldown. Only one pending command is stored; it is revalidated at the next
step boundary and charged only when it changes the world. Public commands
have no per-person identity or fairness guarantee.

## Validation and limits

Current0.12.3 formulas, CO2/pressure response, all final/held-out tests and MCU
measurements: [TERRA-AIR.md](TERRA-AIR.md), [hardware probe](../evidence/hardware-probe.json).
The following measurements are the historical0.12.0 baseline.


Current reproducible model tests: `tests/terra.cpp` and `tests/air.cpp`.

- The supplied desktop reference reproduced `normal_24h.csv` exactly.
- Final model: 20 seeds x 8,640 steps at 23 C, 50% RH, 353 lx: both species
  survived 20/20. Final herbivores 57..117; predators 3..10. No capacity blocks.
- Stress: 5 x 8,640 steps at 30 C, 27.5% RH, 84 lx: both species extinct 5/5.
- Bounds/invariants, pond, shelters, night hysteresis, fallback, failed-action
  charging, cooldown, refill, snapshot guards, reader independence and millis
  rollover passed. Address/undefined-behaviour sanitizers passed host tests.
- HTTP: seven GET routes and all 4,096 three-digit command encodings checked.
- Browser: 320, 390, 768, 1100 and 1440 px; routes and keyboard queueing;
  headless browser explicitly closed. The local preview identifies host
  computation and simulated room inputs.

Physical profiling used a separate diagnostic build with 128 animal slots
occupied and ten extra diagnostic SRAM bytes. Over 65 seconds from one VPS:
65/65 API+world pairs succeeded (130 HTTP replies); pair median 138 ms,
p95 197 ms. Max observed work slice 4,344 us; max accumulated world work per
step 488,480 us (~4.9% of its ten-second interval). Sensor/network work is
additional; these are observed timings, not worst-case bounds.

Canary profiling observed at least 412 B between static data and deepest
observed stack use. This is not proof for every interrupt/fault path. Diagnostic
instrumentation was removed; the production build contains no serial logging.

After deployment, an external observer verified all public page hashes, the
retired /guide route, working sensors and advancing world steps. At 8 lx the
real world entered dormancy: plant/animal snapshot bytes stayed identical
over an eleven-second interval while generations and sensor readings remained
available. Public dark-theme browser checks passed at 320/390/768/1440 px,
including /terra, /lab, /devlog and the live display.

This is not a new many-user capacity benchmark. The previous direct HTTP
10-client repeat lost connectivity and required a manual reset; its cause
remains unconfirmed. Raw historical failures remain in
`verification/direct-http-20260928`. No stability claim is inferred from the
short profiling run.

## Differences from the supplied reference

Sparse 128-animal capacity; integer arithmetic; cooperative updates; random
slot order instead of a full animal-cell permutation; real night debounce;
food/density feedback; plant growth coefficient 1/32 instead of 1/16;
refuge-aware danger; pond/shore; bounded actions and input fallback. These
changes mean the final simulation is not bit-identical to the desktop model.

`/terra` replaces Shared World and the public model section. Legacy #world and
#stories links open Terra; `/guide` is retired. Devlog contains no ESP32/model
entry. Historical source/releases and backups remain available for recovery.
