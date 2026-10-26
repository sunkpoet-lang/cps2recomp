# Tasks

## 1. Rama y ymfm

- [x] 1.1 `git switch -c sf2-sonido sf2-cps1`. Verificar con `git log --oneline -1` que parte de `838cbb9`.
- [x] 1.2 Con OK explícito del usuario, descargar de `github.com/aaronsgiles/ymfm` (en una carpeta aparte) los archivos del OPM y copiarlos sin cambios y planos a `third_party/`. Agregar su sección BSD-3 en `THIRD_PARTY_NOTICES.md`, con el commit de origen, y agregarlos a `CMakeLists.txt`. Verificar que `make GAME=sf2ce` compila y que `tools/audit.sh` pasa.

## 2. OKI MSM6295

- [x] 2.1 `sound/okim6295.h`/`.cpp` (decisión 2): tabla de samples, cuatro voces ADPCM, comandos, atenuación, pin 7 y `Serialize`, con licencia y comentarios al estilo de `qsound.h`. Verificar con un programa de prueba chico (o un modo de prueba del juego) que decodificar el primer sample de `s92_18` da una forma de onda con sentido (sin ruido blanco ni silencio) y que dura lo que dice su tabla.

## 3. Z80 clásico, latches e interrupción

- [x] 3.1 Mapa del Z80 clásico (decisión 4), su RAM propia, banco y latches del 68000 (decisión 5); el Z80 corre en la placa clásica. Verificar sin ventana con `CPS2RECOMP_SOUND_COMMANDS` (o un log equivalente) que el Z80 lee los pedidos del 68000 en `F008`.
- [x] 3.2 Interrupción de nivel en `sound/z80.h` (decisión 6), usada solo por la placa clásica. Verificar que HSF2 da los mismos hashes que antes (`h_mus.txt`) y que el juego de ejemplo pasa la prueba de humo.
- [x] 3.3 Reloj de 3.579545 MHz y agenda de eventos de la placa clásica en `RunSound` (decisiones 3 y 7): timers del YM2151 con ymfm, IRQ, muestras de los dos chips y de salida. Verificar que el Z80 no se cuelga en 6000 cuadros y que la interrupción llega a un ritmo estable (contarla en un log).
  - ~250 interrupciones por segundo, estables (4.19 por cuadro). Además: el DIP C6 (sonido de las demostraciones) encendido, para que suene la intro.

## 4. Mezcla y save states

- [x] 4.1 Mezcla y remuestreo a `QSound::kSampleRate` (decisión 8). Verificar con `--audio-dump` en una corrida de 6000 cuadros que hay música en la selección, efectos en los golpes y la voz del anunciador al empezar un round (mirando el espectrograma y escuchando los fragmentos), y que hay menos del 1 % de muestras recortadas.
- [x] 4.2 Save states (decisión 9), `kStateVersion` a 9. Verificar que un save state de versión 8 carga, que guardar y cargar uno de la 9 a media pelea deja el audio igual, y que la misma película con y sin `run_ahead` da el mismo `--audio-dump` byte por byte.
  - v8 carga y el lado de sonido arranca de cero (se oye al siguiente pedido). v9 a media pelea sigue con la música. Run-ahead 1 y 2: audio idéntico en 6000 cuadros (CE y HF).
- [x] 4.3 Comparar el código traducido contra el intérprete del Z80 (mismo `--audio-dump`) y medir los cuadros por segundo con y sin run-ahead. Verificar que coinciden y anotar el costo.
  - El intérprete y el traducido suenan igual (correlación 0.95-0.999, mismo volumen) pero no byte por byte: difieren por unos ciclos, igual que HSF2 con `--sound z80` en el código de antes. 580 cuadros/s con sonido contra 685 sin él (~15 %); con run-ahead 2, 191.

## 5. Referencia, documentación e instalación

- [x] 5.1 Pedirle al usuario un WAV de FBNeo (Fightcade) de SF2 CE con la intro, la selección, un round y el anunciador; compararlo con el volcado del port (tempo por huella, nivel música/voces, tono de las voces) y ajustar las ganancias. Sin grabación, anotar que la comparación queda para su prueba de oído.
  - Sin grabación de referencia todavía: la comparación queda para la prueba de oído del usuario (5.4). Si graba el WAV de FBNeo, se ajustan `kYmShare`/`kOkiShare` en `machine/cps1_sound.cpp`.
- [x] 5.2 Actualizar los README de `sf2ce` y `sf2hf` (ya con sonido; ymfm y que esta parte es solo del fork) y la tabla del README principal. Verificar que `tools/audit.sh` pasa.
- [x] 5.3 Respaldar `sf2ce.exe` y `sf2hf.exe` instalados en `_respaldo_sf2\`, compilar y copiar los nuevos. Verificar que arrancan con un PATH mínimo.
- [ ] 5.4 Prueba del usuario con ventana: música, efectos y voces en CE y Hyper Fighting. Anotar lo que reporte.

## Workflow follow-up

- Commit en la rama `sf2-sonido` cuando el usuario lo apruebe; esta rama no va a un PR al autor tal cual (regla 4).
