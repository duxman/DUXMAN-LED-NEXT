# Effects Guide

Functional catalog of effects currently implemented in DUXMAN-LED-NEXT.

## Conventions

- ID: numeric identifier in the effect catalog.
- key: technical key used by the API/UI.
- usesSpeed: whether the speed control affects the effect.
- usesAudio: whether the effect automatically reacts to the microphone.

## Visual Effects

| ID | key | Name | usesSpeed | usesAudio | Description |
|---|---|---|---:|---:|---|
| 0 | fixed | Fixed Color | no | no | Alternates the 3 primary colors in fixed sections. |
| 1 | gradient | Static Gradient | no | no | Builds a static gradient using the 3 primary colors inside each section. |
| 2 | diagnostic | Diagnostic | no | no | Enables only the first output in red to isolate the main line. |
| 3 | blink_fixed | Fixed Blink | yes | no | Blinks the fixed section pattern using the configured speed. |
| 4 | blink_gradient | Gradient Blink | yes | no | Blinks the gradient section pattern using the configured speed. |
| 5 | breath_fixed | Fixed Breath | yes | no | Fixed-color sections pulse with a sinusoidal envelope. |
| 6 | breath_gradient | Gradient Breath | yes | no | The full gradient breathes with a sinusoidal envelope. |
| 7 | triple_chase | Triple Chase | yes | no | Moving color train with gaps and repetitions across sections. |
| 8 | gradient_meteor | Gradient Meteor | yes | no | Moving head-and-tail effect with a gradient along the strip. |
| 9 | scanning_pulse | Scanning Pulse | yes | no | Pulse that scans from one end of the strip to the other and bounces back. |
| 11 | lava_flow | Lava Flow | yes | no | Organic flow with wave distortion and warm color blending. |
| 12 | polar_ice | Polar Ice | yes | no | Cold interference effect with slow waves and high contrast. |
| 14 | random_color_pop | Random Color Pop | yes | no | Random color appearances with a fade envelope. |

## Audio-Reactive Effects

| ID | key | Name | usesSpeed | usesAudio | Description |
|---|---|---|---:|---:|---|
| 10 | trig_ribbon | AUDIO · Trig Ribbon | yes | yes | Wavy pattern combining sinusoids and a dynamic gradient driven by microphone input. |
| 13 | stellar_twinkle | AUDIO · Stellar Twinkle | yes | yes | Pseudo-random star flashes reinforced by beats and live audio level. |
| 15 | bouncing_physics | AUDIO · Bouncing Physics | yes | yes | Light balls that bounce with variable energy and react to audio input. |
| 16 | audio_pulse | AUDIO · Audio Pulse | no | yes | Symmetric reactive VU with beat flash, peak hold, and dynamic color. |
| 17 | audio_spectrum | AUDIO · Spectrum VU | no | yes | 3-band VU meter with per-band primary colors and configurable segmentation. |
| 18 | audio_neon_eq | AUDIO · Neon EQ | yes | yes | 3-band neon equalizer with alternating bars, bright heads, and beat flash. |
| 19 | audio_rainbow_wave | AUDIO · Rainbow Wave | yes | yes | Rainbow wave that shifts color with audio, from red to white. |
| 20 | audio_spectrum_chase | AUDIO · Spectrum Chase | yes | yes | Knight Rider-style chase with color based on perceived energy. |
| 21 | audio_section_strobe | AUDIO · Section Strobe | no | yes | LED sections flashing in turn on each beat. |

## Recommended Usage

- Use visual effects for stable or decorative scenes.
- Use audio-reactive effects when the microphone is well calibrated and you want live response.
- If an effect feels too fast or too aggressive, adjust speed and level before changing brightness.
- If an audio effect feels delayed or overreactive, tune `gainPercent`, `noiseFloorPercent`, `noiseGateKnee`, and `agcResponsePercent` in microphone configuration.
