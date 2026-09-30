# Debugging the foundation

Build Debug and run `sbcoop_tests.exe` directly to see each test group's result. CTest prints failure output and returns nonzero on failure.

Run `sbcoop_harness.exe --impaired` to inspect accepted/rejected snapshot counts, rendered frames, maximum position error and dropped transforms. A rejected snapshot is expected when its duplicate or older sequence arrives late. A failed parser, invalid handle, leaked proxy or excessive error fails the harness.

For a wire-format failure, compare the 52-byte header with `NETWORK_PROTOCOL.md`; the fixed golden vector catches accidental layout changes. Keep decoding errors separate from future session/authentication failures.

For movement failures, record sequence, timestamp, discontinuity and epoch. Do not interpolate across epochs or teleports. Inspect buffer age before changing smoothing constants.

For lifetime failures, check both registry membership and adapter generation. Destroy/unbind are separate operations. Never treat a retained network ID as proof that a native object is alive.

There is no production structured logger, overlay, game crash handler or trace recorder yet. Do not create new graphics/native hooks simply to improve diagnostics before the loader and lifecycle gates pass.
