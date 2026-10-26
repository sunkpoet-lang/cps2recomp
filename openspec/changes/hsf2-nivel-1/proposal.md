# Proposal

## Why

El usuario quiere jugar **Hyper Street Fighter II: The Anniversary Edition** (set `hsf2j`, Japón 040202) como port nativo, igual que Super Turbo. Ya tiene las ROMs (`hsf2j.zip` y su padre `hsf2.zip`). Este cambio es el **nivel 1**: que el juego corra traducido a código nativo, sin funciones propias todavía. También mide cuánto se parece su programa al de Super Turbo, para decidir si vale la pena el nivel 2 (heredar el training, el widescreen y la música de Super Turbo).

## What Changes

- **Rama nueva `hsf2`** desde el `main` actual del autor (que ya trae SFA3 y el tag team de Super Turbo), con el commit de F12 (`d8c342f`) aplicado encima.
- **Juego nuevo `games/hsf2`**: `game.cfg` con el set `hsf2j`, su padre `hsf2`, los archivos y CRC del programa, los gráficos, el programa de sonido, los samples y la llave; un README corto. Sin fuentes propias del juego (`src/` vacía), como el juego de ejemplo del toolkit.
- **Compilación y exploración**: `make GAME=hsf2` dos veces y `make GAME=hsf2 explore`, que encuentra el código jugando solo y lo traduce.
- **Prueba sin ventana**: que lea el set, que arranque y que corra en nativo, con capturas de la intro y de una partida.
- **Medición contra Super Turbo**: `tools/portmap.py` entre `work/ssf2t` y `work/hsf2`, con un resumen de cuánto coincide, para decidir el nivel 2.
- **Instalación personal**: `hsf2.exe` y un launcher que lo conozca en `D:\CPS2\CPS2Recomp`, respaldando antes lo que se reemplace.

Fuera de alcance (nivel 2 o después): free play, saltar la intro, la página Board, training, widescreen, nombres de la música, soundtrack propio, colores y modos alternativos.

## Capabilities

### New Capabilities
- `hsf2-port`: el port de Hyper Street Fighter II (`hsf2j`): qué set acepta, que corra en nativo y que aparezca en el launcher de la colección.

### Modified Capabilities
<!-- Ninguna: las specs existentes (soundtrack-live-toggle, soundtrack-song-start) no cambian. -->

## Impact

- **Repo (rama `hsf2`)**: `games/hsf2/game.cfg` y `games/hsf2/README.md` nuevos; la tabla de juegos del README principal. Nada del juego entra a la repo: `rom/` y `work/` siguen ignorados.
- **Tu F12 sobre el `main` nuevo**: el cherry-pick puede chocar con cambios del autor en `port/main.cpp`, `machine/` o el README; se resuelven a mano.
- **Instalación**: `D:\CPS2\CPS2Recomp` recibe `hsf2.exe`, un `cps2recomp-launcher.exe` nuevo (que también lista SFA3) y las dos ROMs copiadas a su carpeta `ROM`. Lo que se reemplace va antes a `_respaldo_hsf2`.
- **Nada se publica**: sin push ni PR en este cambio.
