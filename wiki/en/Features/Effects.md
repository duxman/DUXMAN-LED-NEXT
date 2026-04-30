# Effects Guide

Catalogo funcional de efectos implementados en DUXMAN-LED-NEXT.

## Convenciones

- ID: identificador numerico del catalogo.
- key: clave tecnica usada por API/UI.
- usesSpeed: indica si el control de rapidez tiene efecto.
- usesAudio: indica si el efecto reacciona automaticamente al microfono.

## Efectos visuales

| ID | key | Nombre | usesSpeed | usesAudio | Descripcion |
|---|---|---|---:|---:|---|
| 0 | fixed | Color fijo | no | no | Alterna los 3 colores por secciones fijas. |
| 1 | gradient | Degradado fijo | no | no | Genera un degradado estatico con los 3 colores dentro de cada seccion. |
| 2 | diagnostic | Diagnostico | no | no | Activa solo la primera salida en rojo para aislar la linea principal. |
| 3 | blink_fixed | Parpadeo fijo | si | no | Parpadea el patron fijo por secciones usando la velocidad configurada. |
| 4 | blink_gradient | Parpadeo degradado | si | no | Parpadea el degradado por secciones usando la velocidad configurada. |
| 5 | breath_fixed | Respiracion fija | si | no | Las secciones de color fijo respiran con una envolvente senoidal. |
| 6 | breath_gradient | Respiracion degradado | si | no | El degradado completo respira con una envolvente senoidal. |
| 7 | triple_chase | Triple chase | si | no | Tren de color en movimiento con huecos y repeticiones por secciones. |
| 8 | gradient_meteor | Meteorito degradado | si | no | Cabeza y cola en movimiento con color degradado a lo largo de la tira. |
| 9 | scanning_pulse | Pulso barrido | si | no | Pulso que recorre la tira de extremo a extremo con rebote. |
| 11 | lava_flow | Lava flow | si | no | Flujo organico con deformacion de ondas y mezcla calida. |
| 12 | polar_ice | Polar ice | si | no | Interferencia fria con ondas lentas y contraste polar. |
| 14 | random_color_pop | Random color pop | si | no | Apariciones de color aleatorias con envolvente de atenuacion. |

## Efectos audio-reactivos

| ID | key | Nombre | usesSpeed | usesAudio | Descripcion |
|---|---|---|---:|---:|---|
| 10 | trig_ribbon | AUDIO · Cinta trigonometrica | si | si | Patron ondulante con mezcla de sinusoides y gradiente dinamico guiado por el micro. |
| 13 | stellar_twinkle | AUDIO · Stellar twinkle | si | si | Destellos estelares pseudoaleatorios reforzados por beats y nivel del micro. |
| 15 | bouncing_physics | AUDIO · Bouncing physics | si | si | Bolas luminosas que rebotan con energia variable y responden al micro. |
| 16 | audio_pulse | AUDIO · Audio Pulse | no | si | VU simetrico reactivo con beat flash, peak hold y color dinamico. |
| 17 | audio_spectrum | AUDIO · Spectrum VU | no | si | VU meter de 3 bandas con colores primarios por banda y segmentos configurables. |
| 18 | audio_neon_eq | AUDIO · Neon EQ | si | si | Ecualizador neon de 3 bandas con barras alternadas, cabezal brillante y beat flash. |
| 19 | audio_rainbow_wave | AUDIO · Rainbow Wave | si | si | Onda de arcoiris que cambia de color con el audio, de rojo a blanco. |
| 20 | audio_spectrum_chase | AUDIO · Spectrum Chase | si | si | Chase tipo Knight Rider con color segun energia percibida. |
| 21 | audio_section_strobe | AUDIO · Section Strobe | no | si | Secciones de LEDs que flashean por turno en cada beat. |

## Recomendaciones de uso

- Usa efectos visuales para escenas estables o decorativas.
- Usa efectos audio-reactivos cuando el microfono este bien calibrado y quieras respuesta en vivo.
- Si un efecto se siente demasiado rapido o demasiado agresivo, ajusta primero rapidez y nivel antes de tocar brillo.
- Si un efecto de audio parece retrasado o sobrerreacciona, ajusta gainPercent, noiseFloorPercent, noiseGateKnee y agcResponsePercent en configuracion de microfono.
