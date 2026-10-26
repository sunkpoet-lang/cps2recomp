# Proposal

## Why

El usuario quiere ports nativos de **Street Fighter II: Champion Edition** y **Street Fighter II′: Hyper Fighting**, y ya tiene sus ROMs (`sf2ce.zip`, `sf2hf.zip`). Son juegos de **CPS-1 clásico**: el toolkit solo conoce el CPS-1 con QSound de Cadillacs and Dinosaurs (el "CPS-1.5"), y su cargador falla con un Z80 sin cifrar. Este **paso 1** los hace correr y jugar **en silencio**. El sonido de su hardware (YM2151 y OKI MSM6295) es el paso 2, y estos juegos no usarán el soundtrack del CD de Super Turbo.

## What Changes

- **Placa "CPS-1 clásica" en el toolkit**: un valor nuevo de `board` en `game.cfg` para un CPS-1 sin QSound. Con él, el cargador no intenta descifrar el programa del Z80 con Kabuki, no arranca el QSound y deja el Z80 detenido: sin sus chips de sonido no tendría con qué sonar, y el 68000 no espera respuestas de él. Dino sigue igual.
- **Los seis botones del CPS-1 clásico**: las patadas de los dos jugadores se leen donde las pone la placa de SF2 (`0x800176`).
- **Dos juegos nuevos**:
  - `games/sf2ce`: set `sf2ce`, World 920513.
  - `games/sf2hf`: set `sf2hf`, World 921209.

  Cada uno lleva su `game.cfg` con los CRC observados (los mismos 12 archivos de gráficos, el Z80 y las ROMs del OKI, más su propio programa), un README y su fila en la tabla de juegos.
- **Compilar, explorar y probar sin ventana** cada juego: intro, selección y una pelea con patadas, con capturas.
- **Instalación personal** en `D:\CPS2\CPS2Recomp`: `sf2ce.exe`, `sf2hf.exe` y el launcher, respaldando lo que se reemplace.
- **Rama nueva `sf2-cps1`** desde `hsf2`, para que la colección tenga todos los juegos del usuario.

Fuera de alcance:
- **Sonido:** YM2151 y OKI quedan para el paso 2.
- **Soundtrack y F12:** no aplican a estos juegos.
- **Funciones propias:** free play, DIP switches con nombre, training y widescreen.
- **La relación entre los dos juegos:** compartir fuentes (`sources =`) no hace falta mientras no haya fuentes.

## Capabilities

### New Capabilities
- `cps1-classic-board`: la placa CPS-1 clásica sin QSound en el toolkit: cómo se declara, qué hace con el Z80 y los seis botones.
- `sf2-ports`: los ports de Champion Edition y Hyper Fighting: qué sets aceptan, que corran en nativo, en silencio, y que estén en el launcher.

### Modified Capabilities
<!-- Ninguna en openspec/specs. El comportamiento de Dino (board = cps1) no cambia y se comprueba con hashes. -->

## Impact

- **Toolkit (rama `sf2-cps1`)**:
  - `machine/romset.h` y `machine/romset.cpp`: el valor nuevo de `board` y saltarse el Kabuki.
  - `machine/machine.cpp`: el Z80 detenido en esa placa y la lectura de `0x800176`.
  - Posiblemente `tools/gamecfg.py` o `CMakeLists.txt`, si el valor de `board` se valida en otro lado.
- **Juegos nuevos**: `games/sf2ce/`, `games/sf2hf/` y la tabla del README principal.
- **Dino no debe cambiar**: se comprueba con hashes por cuadro antes y después. Este cambio requiere el set `dino`; si el usuario no lo tiene, se comprueba con el juego de ejemplo y se anota.
- **Instalación**: dos exes nuevos y el launcher (respaldo en `_respaldo_sf2`); las ROMs ya están en `D:\CPS2\CPS2Recomp\ROM`.
- **Nada se publica**: sin push ni PR.
