# Activity 1 - send/receive

A (pico w, `can_node_a`) sends `PING <seq>` on id 0x100 every 1s. B (pico,
`can_node_b`) logs it and replies `PONG <seq>` on 0x101. 500 kbit/s, GP4 rx /
GP5 tx, MCP2562 on each board, 120 ohm on each end.

## Result

Ran ~25 min: 1508 pings, 1508 pongs, nothing lost, rtt ~430-490 us.

## Scope (`images/activity1/`)

- Idle: CANH = CANL = ~2.5 V. Dominant: CANH ~3.5 V, CANL ~1.5 V (`01`, `02`)
- 2 us bits = 500 kbit/s (`02`)
- Stuff bit after 5 same bits (`03`)
- ACK: the other node pulls the ack slot low (`04`)
- TXD/RXD are 0-3.3 V, RXD echoes TXD while sending (`07`, `08`)
- Frame ~165 us, B answers ~35 us after the ping ends (`01`, `10`)
