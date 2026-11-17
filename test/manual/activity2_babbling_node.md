# Activity 2 - babbling node

- A (`can_babbler`): sends id 0x100 nonstop, then busy waits `delay` us. Delay
  sweeps 0 -> 400 us (or `+`/`-` in the terminal).
- B (`can_periodic`): sends id 0x101 every 100 ms (10/s).

Both count what they receive and print once a second.

## Results

| delay | babble/s | B's msgs received/s |
|---|---|---|
| 0 us | ~4050 | 0-2 |
| 10-220 us | ~4100 | 0-1 |
| 230 us | ~3980 | 10 (all) |
| 400 us | ~2390 | 10 |

- 0x100 always wins arbitration (lower id), so B basically never gets on the bus.
- Small delays do nothing. A's next frame is still ready before the bus frees up.
- Around 225 us (about one frame time) B suddenly gets all 10/s through.
- Babbler's frames are valid so CAN error handling never stops it. Fallacies:
  "the network is reliable", "bandwidth is infinite".

## Scope (`images/activity2/`)

CH1 CANH, CH2 A TXD, CH3 B TXD.

- Starved: frames back to back, 241 us apart, only ~24 us idle between them (`01`, `03`)
- B tries every frame and loses arbitration ~25 us in, but still ACKs A (`03`)
- Both start the frame at the same time and send the same bits until the last
  id bit (`04`, `05`)
- Above the cutoff the idle gap grows to ~48 us and B's msgs get through (`06`)
