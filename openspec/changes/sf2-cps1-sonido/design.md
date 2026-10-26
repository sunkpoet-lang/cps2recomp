# Design

## Context

Ver `proposal.md`. Lo observado en el código (rama `sf2-cps1`, commit `838cbb9`):

- **`Machine::RunSound`** (`machine/machine.cpp`): avanza el Z80 en ciclos de un reloj de **8 MHz** (`kSoundClock`), con una interrupción periódica cada 32 000 ciclos (250 por segundo), y saca una muestra de QSound cada `kSoundClock / QSound::kSampleRate`. El audio sale en `machine.audio`, estéreo a **24 038 Hz** (`QSound::kSampleRate`), que el frontend (`port/frontend_sdl.cpp`) y los volcados (`--audio-dump`) dan por hecho.
- **El mapa del Z80** (`Machine::Read`/`Write`, uint16 address) es el de QSound: `C000`–`CFFF` memoria compartida, `D000`–`D007` registros del QSound, `F000`+ RAM.
- **`Z80::RaiseInterrupt`** (`sound/z80.h`) sostiene la interrupción hasta que la CPU la toma: es de flanco, no de nivel.
- **La placa clásica** (`Board.qsound == false`): el Z80 queda detenido (`m_soundRunning = false`) y las escrituras del 68000 a `0x800180`–`0x80018F` se aceptan sin efecto.
- **El programa de sonido de SF2** (`s92_09.11a`, 64 KB) está traducido por `recompz80` (1 448 instrucciones). Los samples del OKI (`s92_18`, `s92_19`, 256 KB) están en `RomSet::samples`, byte por byte.
- **Reglas del proyecto**: `CONTRIBUTING.md` regla 4 ("No emulator's code") y regla 5 (código de otros con licencia compatible y en las notas). `tools/audit.sh`:
  - rechaza subcarpetas en `third_party/` y carpetas con nombre de emuladores;
  - exige que cada archivo de `third_party/` esté en `THIRD_PARTY_NOTICES.md`.
- **El usuario eligió la opción B**: el sonido de SF2 vive en su fork, con ymfm.

## Goals / Non-Goals

**Goals:**
- SF2 CE y Hyper Fighting con su música FM, sus efectos y sus voces como en el arcade.
- Nada cambia para CPS-2 y para el CPS-1 con QSound.
- Save states y run-ahead correctos.

**Non-Goals:**
- Que pase la regla 4 del autor (es solo para el fork).
- Soundtrack propio y F12 en SF2.
- Un YM2151 propio.

## Decisions

### 1. ymfm, solo el OPM, plano en `third_party/`
Se descargan de `github.com/aaronsgiles/ymfm` (con el OK del usuario) los archivos que necesita el YM2151: `ymfm.h`, `ymfm_fm.h`, `ymfm_fm.ipp`, `ymfm_opm.h` y `ymfm_opm.cpp` (y `ymfm_misc`, si `opm` lo incluye). Van **planos** en `third_party/`, sin subcarpeta, porque la auditoría rechaza subcarpetas ahí, y sin cambios, como los decodificadores. Su licencia BSD-3 va en `THIRD_PARTY_NOTICES.md`, con la versión o el commit de origen.

- **Por qué ymfm y no Nuked-OPM:** precisión prácticamente igual en la práctica (es lo que usa MAME para estos juegos), varias veces más rápido, y trae guardado y carga de estado. Eso pesa con el run-ahead, que corre cuadros de más.

### 2. El OKI MSM6295, escrito para el proyecto
`sound/okim6295.h`/`.cpp`, al estilo de `qsound.h`:
- **Cuatro voces** ADPCM (el algoritmo de Dialogic/OKI de 4 bits) con la tabla de 128 entradas que el chip lee al inicio de las ROMs: dirección de inicio y de fin de cada sample.
- **Comandos:** un comando de dos bytes elige el sample y luego las voces y su atenuación; otro detiene voces.
- **Frecuencia:** reloj / 132 o / 165 según el pin 7.
- **Salida:** mono, una muestra cada vez que toca.
- **Estado:** `Serialize`.

Se escribe a partir de la hoja de datos del chip y de cómo lo describen otras implementaciones, sin copiar código (regla 4).

### 3. Reloj y tiempo del Z80 clásico
En la placa clásica, `RunSound` cuenta en ciclos de **3.579545 MHz** en lugar de 8 MHz: el reloj de la placa clásica para el Z80, el YM2151 y el OKI, que en la placa de SF2 va a 1 MHz desde otro oscilador. Lo que se agenda en ese tiempo:
- el próximo vencimiento de un timer del YM2151, en ciclos del chip convertidos;
- la próxima muestra del YM2151: reloj / 64, unos 55 930 Hz;
- la próxima muestra del OKI: 1 MHz / 132 o / 165;
- la próxima muestra de salida a 24 038 Hz.

El código de QSound no cambia: el camino clásico es una rama aparte dentro de `RunSound`.

### 4. Mapa de memoria del Z80 clásico
En `Machine::Read`/`Write`, si `!qsound`:

| Dirección | Lectura | Escritura |
|---|---|---|
| `0000`–`7FFF` | ROM | — |
| `8000`–`BFFF` | banco de ROM (`F004`) | — |
| `D000`–`D7FF` | RAM | RAM |
| `F000` / `F001` | estado del YM2151 | dirección / dato del YM2151 |
| `F002` | estado del OKI | comando del OKI |
| `F004` | — | banco de ROM |
| `F006` | — | pin 7 del OKI |
| `F008` | latch 1 (pedido del 68000) | — |
| `F00A` | latch 2 (volumen y fade del 68000) | — |

La RAM del Z80 clásico va en un arreglo propio (2 KB), no en `soundRam`/`soundRam2` de QSound.

### 5. Los latches del 68000
En `WriteCps1`, si `!qsound`:
- **`0x800180`/`0x800181`**: guarda el byte bajo en el latch 1;
- **`0x800188`/`0x800189`**: guarda el byte bajo en el latch 2.

El Z80 los lee en `F008` y `F00A` (decisión 4). Las demás escrituras del rango siguen como hoy.

### 6. Interrupción de nivel
`sound/z80.h` gana una línea de interrupción de nivel (`SetInterruptLine(bool)`): mientras esté en alto y las interrupciones estén habilitadas, la CPU la toma, sin que tomarla la baje. La placa clásica la conecta a la salida IRQ del YM2151, que ymfm avisa con su `ymfm_update_irq`. QSound sigue con `RaiseInterrupt` como hoy. El código traducido por `recompz80` revisa la interrupción donde ya lo hace; se verifica que tome la de nivel igual que el intérprete (corrida con `--sound check` o con `--cpu`, la que el Z80 admita).

### 7. Timers del YM2151
ymfm pide al anfitrión que programe sus timers (`ymfm_set_timer(tnum, duración en ciclos del chip)`) y que le avise cuando vencen (`engine_timer_expired`). El `Machine` los agenda en el tiempo del Z80 (decisión 3), como un evento más del ciclo de `RunSound`. El tempo de la música sale de ahí, y la prueba de tempo de la spec lo comprueba.

### 8. Mezcla y remuestreo
- **Fuentes:** el YM2151 da estéreo a unos 55.9 kHz y el OKI mono a unos 7.6 kHz.
- **Remuestreo:** se llevan a 24 038 Hz con interpolación lineal sobre las dos últimas muestras de cada fuente; es suficiente para estas frecuencias y no cambia el frontend.
- **Mezcla:** con ganancias fijas por chip, el OKI al centro.
- **Ajuste:** las ganancias se ajustan con la grabación de referencia para que la relación música/voces se parezca, y sin saturar (recorte en enteros de 16 bits).

### 9. Save states
`Machine::Serialize` guarda, solo para la placa clásica:
- el banco y los latches;
- la RAM del Z80;
- el estado de ymfm (`save_restore` con su `ymfm_saved_state` sobre un `std::vector<uint8_t>`);
- el estado del OKI;
- los tiempos agendados y la línea de interrupción.

`kStateVersion` pasa a 9, y lo nuevo solo se lee de estados versión 9 o mayor. Como el run-ahead se basa en guardar y cargar, la prueba "mismo audio con y sin run-ahead" valida todo esto junto.

### 10. Verificación con la referencia de FBNeo
El usuario graba en Fightcade (FBNeo) un WAV de SF2 CE: la intro, la selección, un round y el anunciador. Se compara con el volcado del port en tres cosas: la melodía y el tempo (por huella de audio, como con las pistas del CD), el nivel relativo música/voces y que las voces suenen a su tono. Sin grabación, se verifica de oído en la prueba del usuario y se anota.

## Risks / Trade-offs

- **[El recompz80 no revisa la interrupción de nivel en todos los puntos]** → Comparar el intérprete contra el traducido (mismo audio volcado). Si difieren, correr el Z80 de la placa clásica con el intérprete (es pequeño y lento: 3.58 MHz cuestan poco).
- **[Tempo distinto por un timer mal agendado]** → La prueba de tempo contra la referencia lo atrapa.
- **[Ganancias: música tapando voces o al revés]** → Se ajustan con la referencia; quedan como constantes comentadas.
- **[Rendimiento con run-ahead]** → Medir cuadros por segundo sin ventana con y sin run-ahead. ymfm es rápido; si el costo pasara del 20 %, anotarlo.
- **[La regla 4 del autor]** → Asumido por el usuario: esta rama no va en un PR tal cual. Se anota en el README de SF2 y en `THIRD_PARTY_NOTICES.md`.

## Migration Plan

1. Rama `sf2-sonido`; descarga de ymfm con OK; notas de licencia.
2. OKI, mapa, latches, interrupción, timers, mezcla y save states.
3. Pruebas sin ventana: arranque, efectos, voces, música, run-ahead y HSF2 sin cambios.
4. Comparación con la referencia de FBNeo (si el usuario la graba).
5. Instalación con respaldo y prueba del usuario.
6. **Si algo falla:** se devuelven los exes del respaldo; la rama `sf2-cps1` queda intacta.

## Open Questions

- Las ganancias finales de la mezcla se fijan con la referencia, sin cambiar el plan.
