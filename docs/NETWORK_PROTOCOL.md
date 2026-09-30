# Foundation wire format 0.1

This is an internal, unstable foundation protocol with **exact version matching**. It implements only spawn/despawn/transform payloads. It is not a complete network/session protocol and must not be exposed as a public game service.

All integers are unsigned, fixed-width and little endian. Floats are 32-bit IEEE-754. Fields are written individually; native struct alignment, pointers and padding never appear on the wire.

## Envelope (52 bytes)

| Offset | Bytes | Field |
| --- | --- | --- |
| 0 | 4 | ASCII `SBMP` |
| 4 | 2 | Major version = 0 |
| 6 | 2 | Minor version = 1 |
| 8 | 2 | Message type |
| 10 | 2 | Flags; must be zero |
| 12 | 4 | Payload byte length |
| 16 | 16 | Session ID; all-zero invalid |
| 32 | 4 | World epoch; zero invalid |
| 36 | 8 | Message sequence; zero invalid |
| 44 | 8 | Monotonic timestamp in microseconds |

Maximum complete encoded message: 1,024 bytes. Declared payload size and complete message size must match exactly; trailing bytes are rejected. Unsupported versions, flags and types fail decoding.

## Payloads

| Type | Value | Layout | Payload / total bytes |
| --- | --- | --- | --- |
| Spawn | 1 | Entity ID u64 | 8 / 60 |
| Despawn | 2 | Entity ID u64 | 8 / 60 |
| Transform | 3 | Entity ID u64, discontinuity u32, position xyz f32, rotation xyzw f32 | 40 / 92 |

Entity IDs 0 and UINT64_MAX are invalid. Transform coordinates must be finite and within +/-1,000,000 metres per axis. Quaternion values must be finite, with squared length within 0.001 of 1. These are project bounds, not measured game limits. Positions are not quantized.

`encode` throws `invalid_argument` on invalid locally constructed messages. `decode` returns a typed error for malformed input. A successful decode does not establish sender ownership, session admission, freshness, correct direction, or authorization.

## Ordering and compatibility

The harness assigns increasing transform sequences; the snapshot buffer rejects equal/lower sequence numbers and non-increasing timestamps per entity. There is no wraparound policy: identifiers and counters must not wrap in a live session.

Future lifecycle messages use a reliable ordered lane, with increasing spawn IDs and explicit activation barriers. Transforms use unreliable delivery with application stale-drop. GNS reliability does not remove the need for application ownership checks or cross-lane readiness.

Future handshake work must establish exact mod/game/adapter/profile compatibility, participant identities, secret handling and area readiness before actor creation. Do not add unauthenticated public sockets around this codec and call it multiplayer.

Changing layouts or validation semantics requires a version change and updated golden vectors. Add later animation, gameplay, baseline and progression messages only when their owning milestone begins.
