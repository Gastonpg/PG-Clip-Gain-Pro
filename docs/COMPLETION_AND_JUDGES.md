# PG-Clip-Gain-Pro — Completion and Judge Validation

This document is the engineering contract for completing the first usable release. It does not claim that a judge is trained merely because its interface or evaluator exists.

## Product invariants

- The plug-in remains a manual intelligent editing tool: static Clip Gain and Clip Peak proposals only.
- No compressor, limiter, saturation, automatic dynamic leveling, or destructive rendering is introduced.
- The audio process callback remains bounded and real-time safe. Capture assembly, analysis, judge evaluation, and UI preparation stay off the audio callback.
- Proposals are reviewable and editable. The user remains the final decision-maker.
- Uncertain evidence produces a conservative abstention, not a forced correction.
- All work stays on `pre-judges/core-completion-roadmap`; `main` is not modified.

## Completion gates

1. **Core analysis safety**
   - Add explicit within-phrase protection diagnostics before allowing phrase-wide gain to affect uncertain, transient-like, or bridged low-level windows.
   - Keep protection conservative: RMS/energy descriptors cannot identify phonemes, consonants, breaths, or semantic phrase boundaries.
   - Add tests for quiet internal windows, short bridged pauses, phrase starts/ends, high-crest transients, invalid windows, and smooth gain transitions.
   - Do not describe this stage as phonetic protection.

2. **Reviewable VST3 workflow**
   - Expose capture state, captured duration, analysis readiness, issues, Clip Gain proposals, Clip Peak events, and the combined static curve to the controller/editor through a thread-safe component/controller message path.
   - Keep the process callback free of allocation, locks, waits, UI calls, and analysis.
   - Provide explicit review/accept/reject/manual-adjust semantics. Do not silently apply proposals during capture.
   - Keep the audio pass-through until a separately reviewed static curve is explicitly committed by the user.

3. **Host and package validation**
   - Keep Windows x64 Release build, CTest, VST3 bundle packaging, and Steinberg Validator in CI.
   - Add automated tests for controller messaging and state serialization where feasible.
   - Run the built VST3 in FL Studio and verify load/unload, mono/stereo routing, 32/64-bit processing, capture start/stop, transport discontinuity handling, and no audio dropouts.
   - CI success is not a substitute for a real FL Studio host test.

4. **Five-judge decision layer**
   - Implement each judge as an independent, deterministic, inspectable evaluator first; no judge may write audio or mutate another judge's evidence.
   - Each judge returns evidence, score/confidence, rationale codes, and one of: recommend, keep, or abstain.
   - A separate arbitrator resolves conflicts and may abstain. It must never override hard safety constraints.

## Judge responsibilities

1. **Musical interpretation** — phrase-level consistency and musical context from available approved features; must abstain when context is unavailable.
2. **Adaptive vocal leveling** — evaluates Clip Gain balance proposals against the adaptive reference and within-phrase variation; cannot use a fixed target loudness.
3. **Transient and peak protection** — evaluates Clip Peak events, crest factor, local peak contrast, and protected regions; cannot equate every peak with a problem.
4. **Global coherence** — checks whether proposed edits remain consistent across the full capture and do not create abrupt or contradictory gain changes.
5. **Main arbitrator** — combines judge evidence, enforces user limits and safety rules, records disagreement, and abstains when evidence is insufficient.

## Training and evaluation requirements

A trained model requires a documented, licensed or explicitly consented dataset and human-reviewed labels. The corpus must be partitioned by speaker/session into train, validation, and held-out test sets to prevent leakage. Evaluation must report per-judge precision/recall where labels support it, false-positive and false-negative rates, calibration, abstention coverage, disagreement rates, and listening-test results. Thresholds must be frozen before held-out evaluation.

Until those requirements are met, the judges must be labelled **rule-based prototypes**, not trained AI models. No user recordings may be uploaded or reused for training without explicit consent.

## Release definition

The first usable release is complete only when the core analysis is tested, the editor can review and modify proposals, the VST3 passes CI validation, and the FL Studio host test is recorded. The judge layer is a separate milestone; trained-judge claims require the dataset and evaluation gates above.
