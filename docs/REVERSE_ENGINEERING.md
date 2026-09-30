# Evidence-driven game research

No game-specific bindings are implemented yet. A visible actor, animated actor, combat actor, damageable actor and independently targetable second player are separate capabilities.

## Confidence

- UNKNOWN: no sufficient evidence.
- HYPOTHESIS: a plausible interpretation or external report.
- PARTIALLY VERIFIED: demonstrated only in limited conditions.
- VERIFIED: reproduced on an exact build with recorded experiments and limitations.

KNOWN labels source-backed facts, and ASSUMED labels design choices; neither substitutes for runtime verification of a native binding.

## Finding record

Record: finding ID; purpose; observed class/object name if any; exact executable and loader fingerprints; discovery method; actual signature/offset if measured; calling convention; arguments; thread and lifetime rules; confidence; experiment; repeated results; counterexamples; references; consumers; retest triggers.

Follow observation -> hypothesis -> controlled experiment -> restart/reload verification -> review -> adapter implementation. Research tasks produce documents. They do not fill unknown offsets with guesses.

## First research gates

1. Reproduce executable/loader identity and distinguish configured engine overrides from detected versions.
2. Locate the complete save write surface and prove isolation/recovery before game manipulation.
3. Pin a loader/SDK/toolchain/CRT combination and verify initialization/shutdown.
4. Prove the game-thread callback; identify world teardown and object invalidation.
5. Find world -> local participant -> controller -> character through evidence.
6. Verify transform units, axes, rotations, streaming origin and native conversion.
7. Test a minimal visual character's construction, movement, ownership and destruction.

Use local reflection/SDK dumps as research leads, not trusted layouts. A readable pointer or unique byte pattern does not prove the correct object or callable. Do not bypass protections or change entitlement behavior to obtain evidence.
