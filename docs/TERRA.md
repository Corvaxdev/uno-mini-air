# Room Terra — 0.12.9

The UNO Mini owns the entire shared ecosystem. It advances without browsers,
the Internet, a Raspberry Pi or an application server. W5500 transports HTTP;
the browser only renders snapshots and submits bounded actions.

System light/dark theme follows live OS changes, with no manual preference.
The shared reserve is a 36px number in the left column (above the map on phones).
Deployed build (public edition measured separately in README): 32,250 / 32,256 B Flash, 1,492 B static SRAM, 11,385 B shared gzip.

## Memory and scheduling

- 32 x 32 torus, four orthogonal neighbours; one step per 10 real seconds.
- Plants: 256 B, two bits per cell (biomass 0..3).
- Animals: 256 B, 128 fixed 16-bit records. Position: 10 bits; energy: 3;
  species: 1; acted-this-step: 1. Empty records are zero. No allocation.
- Complete AVR world state: 584 B, including environment, timing and counters.
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

Let L = clamp((lux - 10)/310, 0, 1), Mtarget = clamp((RH - 20)/50, 0, 1).
Virtual soil approaches Mtarget with `M += (Mtarget-M)/180` per active step
(approximately a 30-minute time constant). Soil has eight fractional bits,
avoiding the large dead band of a whole-percent accumulator.

Effective light is saturated: photo=floor(383*L8/(L8+128)), L8=rounddown(255*L).
A one-byte light debt suppresses photosynthesis above128, with a factor(255-debt)/128. Every640s, daylight adds1..8 units based on raw lux; darkness removes3. Normal16h/8h cycles clear the debt; long/extreme illumination can exhaust food.
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
level simulation. The browser renders the identical geometry. Shelters have bright outlines; selecting one disables predator placement and shows SHELTER. Firmware independently rejects predator placement there.

Darkness is measured by the room light sensor, not wall-clock time. Eighteen
successive 10-second readings below the quantized night threshold (<=25 lx)
slow ecological passes to1/32 rate. Eighteen above the wake threshold (>=41 lx) restore full activity. The gap
provides hysteresis. Sensors, HTTP, credit refill and visitor actions continue
during night rest. There is no invented countdown to sunset.

Missing inputs retain their previous value for five minutes, then approach
marked fallback values (23 C, 50% RH, 227 lx). This affects only simulation
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

Current build sizes and example network configuration are in the repository README.
See [evidence](../evidence/README.md) for versioned measurements and failures.
The current host suite tests the model, arithmetic, all 2,048 species/cell
placements, HTTP routes and all 4,096 encoded actions under ASAN/UBSAN.
World state is not persisted; a reset starts it again.

The 0.12.9 light model completed 50 runs of 72 simulated hours under three room
cycles: 48 ended with both species, two lost predators, none lost both species.
Continuous daylight, darkness and excessive illumination can exhaust the food chain.
These are deterministic simulation experiments, not biological calibration.

Historical 0.12.3 instrumented hardware observed a 4.324 ms maximum work slice
and a 413 B minimum stack gap. The 180/180 external replies in 90 seconds were a
functional probe, not a capacity measurement. The earlier direct HTTP load test
includes a loss of connectivity requiring a manual reset; its cause is unresolved.
Current Terra capacity has not been established by those older tests.
