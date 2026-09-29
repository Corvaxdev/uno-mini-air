# Changes

## 0.12.12-terra

- Fix an RX buffer overflow: sample the W5500 receive count once before limiting the copy. Arduino min() previously read a changing count twice.
- Add 81,962 bounded-RX regression cases, including packet arrival between counter reads.

## 0.12.11-terra

- LIVE stops polling while hidden and resumes a single loop on return.
- Animal actions check occupied cells and full capacity; selection hints survive refresh.
- World restart clears the displayed event log.
- Failed EEPROM writes advance through the ring while preserving the latest verified checkpoint. Retries remain15 minutes apart.
- Automatic system theme uses CSS media queries.

## 0.12.10-terra

- All visitor actions require a dry selected cell; water-centred seeding no longer affects nearby land.
- Water selection disables all action buttons and displays WATER; costs restore on land.
- MCU rejects water commands without spending reserve or starting cooldown.
- 6,144 placement cases cover every cell, action and normal/empty reserve.

## 0.12.9-terra

- Room daylight normalized to 320 lx; sensor-driven terrain tint with bright
  animals and shelter outlines, independent of the OS theme.
- One-byte illumination debt; slow ongoing night ecology (one pass / 32 steps).
- Clear shelter selection feedback; predator action disabled in shelters.
- Shorter report and metadata to fit the model. Current model and placement tests.
- Current build evidence, versioned Internet load results and day/night previews.

## 0.12.6-terra

- Initial public source release; LIVE / TERRA / DETAILS / DEVLOG navigation.
