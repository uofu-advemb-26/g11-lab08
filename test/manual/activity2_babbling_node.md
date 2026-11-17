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
