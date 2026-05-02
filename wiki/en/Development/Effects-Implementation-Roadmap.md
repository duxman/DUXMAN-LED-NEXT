# Effects Implementation Roadmap

This document translates the conceptual effects roadmap into a progressive implementation strategy for the current `duxman-led-next` architecture.

It does not define the low-level math for each effect. That work belongs in the conceptual roadmap; this file focuses on how to introduce those effects without destabilizing the current firmware base.

## Goal

Implement the new family of dynamic effects in phases while preserving:

- compatibility with the current Home UI
- persistence of existing state
- render stability on ESP32
- the option to evolve later toward startup sequences and an effect scheduler

## Current Firmware State

The current architecture has these characteristics:

- `CoreState` exposes `power`, `brightness`, `effectId`, `sectionCount`, `effectSpeed`, `effectLevel`, `primaryColors[3]`, and `backgroundColor`
- `EffectEngine` currently exposes only `begin()`, `supports()`, and `renderFrame()`
- `EffectManager` resolves the active effect through fixed compile-time instances
- the main `loop()` renders on a fixed 16 ms cadence
- there is no common float math, robust blending, or gamma-correction layer yet
- there is no formal effect activation/deactivation lifecycle yet

Conclusions:

- current static effects provide a solid base, but they are not enough for a large dynamic family
- before implementing stateful effects, the engine should be reinforced

## Agreed Decisions

These decisions are the baseline for the next iterations:

1. Do not implement all 11 effects at once.
2. Start with engine infrastructure first.
3. Keep the current UI initially and reuse existing controls.
4. Use the following initial semantics for global parameters:
   - `brightness`: final global brightness
   - `effectSpeed`: temporal speed of the effect
   - `effectLevel`: structural intensity of the effect
   - `sectionCount`: number of blocks, repetitions, or density depending on the effect
5. Do not move render to a dedicated core-specific task yet; validate performance in the current loop first.
6. Introduce effect lifecycle hooks before implementing effects that require persistent state.
7. Centralize blend, gradient, smoothstep, normalized coordinates, and gamma helpers in the engine base, not inside each effect.

## Required Infrastructure Changes

### Phase 1. Engine Base

Goal: prepare a shared layer for time-based and float-based effects.

Planned changes:

- extend `EffectEngine` with shared helpers
- introduce normalized time and per-pixel coordinate helpers
- introduce common RGB blending and saturating additive blending
- introduce `smoothstep`
- introduce float-based color scaling
- introduce a shared gamma table

Expected helpers:

- `normalizedX(pixelIndex, pixelCount)`
- `normalizedTimeSec()`
- `clamp01(value)`
- `smoothstep(edge0, edge1, x)`
- `lerpColor(colorA, colorB, t)`
- `addColor(colorA, colorB)`
- `scaleColorFloat(color, gain)`
- `applyGamma(color)`

### Phase 2. Effect Lifecycle

Goal: support effects with stable internal state.

Planned changes:

- add `onActivate()`
- add `onDeactivate()`
- optionally add `onStateChanged()`
- make `EffectManager` detect real `effectId` changes
- reset or reseed internal state when the active effect changes

This is required before:

- `Bouncing Physics`
- `Stellar Twinkle`
- `Random Color Pop`
- cualquier efecto con arrays o entidades persistentes

### Phase 3. Catalog Scalability

Goal: let the effects catalog grow without touching too many points in the codebase.

Planned changes:

- reduce manual index dependencies in `EffectManager`
- keep `EffectRegistry` as the functional catalog of IDs, labels, and metadata
- make it possible to add new effects with minimal cross-cutting changes

## Recommended Implementation Order

### Group A. Effects Without Persistent State

These should come first because they validate the mathematical foundation without introducing persistent buffers:

1. `Triple Fixed Breathe`
2. `Global Gradient Breathe`
3. `Triple Chase`
4. `Gradient Meteor`
5. `Scanning Pulse`
6. `Trig-Ribbon`
7. `Lava Flow`
8. `Polar Ice`

### Group B. Effects With Light or Persistent State

These should come after lifecycle support is in place:

9. `Stellar Twinkle`
10. `Random Color Pop`
11. `Bouncing Physics`

## Acceptance for Each Effect

Each new effect should be considered acceptable only if it:

- compiles correctly
- does not break existing effects
- respects `brightness`
- uses `effectSpeed` correctly
- applies a clear semantic meaning to `effectLevel`
- behaves stably when switching to and from other effects
- maintains smooth-enough visual response on real hardware
