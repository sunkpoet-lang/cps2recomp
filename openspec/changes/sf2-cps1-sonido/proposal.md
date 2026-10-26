# Proposal

## Why

SF2 Champion Edition y Hyper Fighting ya se juegan en nativo, pero en silencio: la placa CPS-1 clásica del toolkit deja el Z80 detenido porque no tiene sus chips de sonido, el **YM2151** (FM) y el **OKI MSM6295** (voces). El usuario quiere oírlos como en el arcade y decidió hacerlo **solo en su fork** (opción B): el YM2151 sale de una biblioteca existente, **ymfm**, aunque la regla del proyecto ("no emulator's code") impide que esta parte entre al repo del autor tal cual. La placa clásica, el mapper y el multiplicador (change `sf2-cps1-paso-1`) no dependen de esto.

## What Changes

- **YM2151 con ymfm** (Aaron Giles, BSD-3-Clause): solo los archivos del OPM en `third_party/`, sin cambios, con su licencia en `THIRD_PARTY_NOTICES.md`. La descarga desde GitHub requiere el OK explícito del usuario al implementarlo.
- **OKI MSM6295 escrito para el proyecto** (`sound/okim6295.*`): cuatro voces ADPCM desde las ROMs `s92_18`/`s92_19`, con el modo de frecuencia del pin 7.
- **El Z80 de la placa clásica corriendo**:
  - su reloj (3.579545 MHz);
  - su mapa de memoria: ROM, banco, RAM, los dos chips, el banco y los dos latches del 68000;
  - su interrupción, que la da el YM2151 (de nivel, no periódica).
- **El 68000 manda los pedidos de sonido** por los latches `0x800181` y `0x800189`, que hoy se ignoran.
- **Mezcla**: YM2151 (estéreo) y OKI (mono), remuestreados a la salida que ya usa el frontend (estéreo, 24 038 Hz), sin cambiar el frontend.
- **Save states y run-ahead** guardan el estado de los dos chips, del Z80 y de los latches. La versión de los save states sube.
- **Verificación**: comparar contra una grabación WAV del mismo juego en FBNeo (Fightcade del usuario), además de pruebas sin ventana.

Fuera de alcance:
- El soundtrack propio y F12 en estos juegos: el usuario decidió no usar el CD ahí.
- Que esta parte vaya al repo del autor.
- Otros juegos de CPS-1 clásico, aunque quedan preparados.

## Capabilities

### New Capabilities
- `cps1-classic-sound`: el sonido de la placa CPS-1 clásica: el Z80 con su mapa y su interrupción, los pedidos del 68000, el YM2151, el OKI MSM6295, la mezcla y su estado en los save states.

### Modified Capabilities
<!-- `cps1-classic-board` vive en el change sf2-cps1-paso-1 (sin archivar): su requisito "Z80 detenido sin chips de sonido" queda reemplazado por esta capacidad. Se archiva primero aquel change y entonces se podrá escribir el delta MODIFIED; mientras, la nueva capacidad dice explícitamente que el Z80 de la placa clásica corre con sus chips. -->

## Impact

- **Rama `sf2-sonido`** desde `sf2-cps1`.
- **Nuevos**:
  - `third_party/ymfm*` (los archivos del OPM);
  - `sound/okim6295.h`/`.cpp`;
  - la integración en `machine/machine.h`/`.cpp`: reloj y mapa del Z80 clásico, latches, interrupción de nivel, `RunSound` con los dos chips, `Serialize`.
- **Posibles cambios**:
  - `sound/z80.h`: una línea de interrupción de nivel, usada solo por la placa clásica;
  - `CMakeLists.txt`: compilar los `.cpp` de ymfm y del OKI;
  - `THIRD_PARTY_NOTICES.md`;
  - `tools/audit.sh` NO se cambia: ymfm va plano en `third_party/` y listado en las notas.
- **QSound, CPS-2 y Dino**: nada cambia; se comprueba con hashes de HSF2 y la prueba de humo.
- **Save states**: `kStateVersion` sube; los de antes siguen cargando.
- **Instalación**: `sf2ce.exe` y `sf2hf.exe` nuevos, con respaldo.
