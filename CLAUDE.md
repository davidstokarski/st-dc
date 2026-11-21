# ST DC — guide for agents

ST DC is a drum compressor in the ST family: pick the drum part a track is (bus, kick, snare, tom, cymbals, room,
verb) and it runs the engine built for it. Built on the shared module in `../st-shell`
(https://github.com/davidstokarski/st-shell). Read the shell docs first:

- `../st-shell/CLAUDE.md` — non-negotiables (design, real-time safety, native menus, verification)
- `../st-shell/docs/DESIGN.md`, `../st-shell/docs/ARCHITECTURE.md`, `../st-shell/docs/EXAMPLES.md`
- `../st-sat` is the reference plugin; `../st-eq` the second one.

Plugin-specific notes:

- North star: FabFilter Pro-C (level display with GR trace, knee display, threshold handle, style/character split),
  without FabFilter names or artwork. Look: near-black with a warm brown cast and soft brown fades, no textures
  (tokens in `Source/ui/Style.cpp::applyTheme`, backdrop / tray in `paintBackdrop` / `paintTray`), caramel accent,
  red gain reduction. Live meters / readouts read the `LevelHistory` (`LiveLevels`), never per-chunk atomics.
- Parameters: `Source/Params.*` (IDs are permanent — never rename). Drum parts and engines: `Source/Roles.*`
  (order is permanent: it is the parameter value). Each part has an engine, a "why" and a starting point;
  `StDcProcessor::selectRole` applies it as one undo step (UI only; automation of `role` only switches the engine).
- DSP: `Source/dsp/Compressor.*` — the six engines in one class (detector → gain computer → ballistics, dB domain).
  Feedback engines (FET, Opto, Vari-Mu) solve their output-detecting curve so the steady state equals
  `Compressor::staticGainDb` (what the knee display draws); keep that true (tests check it). `Source/dsp/Saturation.*`
  is the colour stage (engine colour + Drive), `Source/dsp/Engine.*` the whole path (input → side-chain HPF →
  gain → oversampled colour → make-up → latency-aligned mix → output → bypass) plus the `LevelHistory` the displays
  read. Oversampling changes re-prepare on the message thread (`applyQualityChange`).
- UI: `Source/ui/Display.*` (level history, spectrum with GR band and side-chain handle, knee, meters),
  `Source/ui/Panels.*` (drum-part selector, control bar, footer), `Source/ui/Strip.*` (ST DC's multiview module),
  `Source/PluginEditor.*`.
- Drum Multiview: the shell's `st::MultiView` with `Options::requiredKind = "ST DC"` (only tracks with an ST DC, but
  every ST plug-in on them), `leftmostKind = "ST DC"`, `orderFor` = drum part (kit order), title "Drum Multiview".
  The strip may be rendered off screen by other ST plug-ins: knobs and self-handled clicks only.
- Verify: `StDcPluginTests` (engine curves / timing, bypass + mix nulls per oversampling, stress over parts / engines
  / block sizes, side-chain, state, presets, multiview with the installed ST Sat / ST EQ AUs),
  `StDcPluginTests --snapshot <dir>` (look at the PNGs), `auval -v aufx Stdc StAu`.
- Release: `scripts/release.sh`.
- Anything reusable belongs in st-shell, not here.
