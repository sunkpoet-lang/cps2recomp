# Proposal

## Why

Hyper Street Fighter II ya corre en nativo (change `hsf2-nivel-1`), pero no tiene la opción "Custom soundtrack": el toolkit solo la ofrece a los juegos que dicen dónde guarda su programa de sonido la tabla de canciones. El usuario quiere la misma experiencia que en Super Turbo: el soundtrack del CD de PC con F12 para alternar al instante con la música de la placa. El programa de sonido de HSF2 tiene la tabla en el mismo lugar que Super Turbo (`0x9006`) y **las mismas 68 piezas de música con la misma numeración**, así que su música y el `soundtrack.ini` ya hecho se pueden reutilizar.

## What Changes

- **Las primeras fuentes propias de HSF2** (`games/hsf2/src/`): un `game.cpp` con `RegisterGame()`, que solo registra la música, y un `music.cpp` con la tabla de canciones (`0x9006`, cuenta `0x280`) y los nombres de las piezas confirmados en este juego.
- **Con eso, HSF2 gana la feature `custom_soundtrack`**: la opción en el launcher, el `songs.txt` en su carpeta, el soundtrack del CD por nombre de canción y F12 (que ya está en el toolkit).
- **Se confirma con el log del soundtrack** qué número pide el juego en cada pantalla (escenarios, versus, ganador, continue, game over, título, selección), con capturas de esos cuadros.
- **Carpeta de música propia** (`D:\CPS2\CPS2Recomp\AST\HSF2`, decidido por el usuario): un `soundtrack.ini` hecho a partir del de Super Turbo que apunta a los mismos MP3 de `AST\SSF2Turbo` con rutas relativas, sin copiarlos; ahí escribe HSF2 su `songs.txt`.
- **Instalación**: `hsf2.exe` nuevo en `D:\CPS2\CPS2Recomp`, respaldando antes el del nivel 1.

Fuera de alcance: free play, Board, training, widescreen y el resto del nivel 2; nombres de canciones que el juego nunca pida en las pruebas quedan con nombre genérico.

## Capabilities

### New Capabilities
- `hsf2-musica`: la música de Hyper Street Fighter II para el custom soundtrack y F12.

### Modified Capabilities
<!-- Ninguna: la spec `hsf2-port` todavía vive en el change hsf2-nivel-1 (sin archivar), y la música se separa como capacidad propia. -->


## Impact

- **Repo (rama `hsf2`)**: `games/hsf2/src/game.cpp` y `games/hsf2/src/music.cpp` nuevos; README de `hsf2` (la música en "Where it stands"). Al tener `src/`, HSF2 deja de usar `runtime/no_game.cpp`.
- **Toolkit**: sin cambios; usa `songTable`/`songCount` (`runtime/game.h`) y el soundtrack existente.
- **Instalación**: `hsf2.exe` reemplazado (respaldo del de nivel 1 en `_respaldo_hsf2`); el usuario elige la carpeta `AST\HSF2` en la página de HSF2 del launcher.
- **Nada se publica**: sin push ni PR.
