# Proposal

## Why

Para probar el juego hace falta aislar cosas rápido: energía infinita, tiempo detenido, un personaje o un arma en particular. Se descubre algo (un bug, un combo, un truco de la placa) con trucos y luego se valida jugando sin ellos. Hoy solo Super Turbo tiene trucos, programados a mano en C++ (`games/ssf2t/src/cheats.cpp`). SF2 CE, Hyper Fighting y Ghouls no tienen ninguno, y la única alternativa es `--poke` desde la línea de comandos. El usuario quiere una página de trucos en todos los juegos, fácil de usar desde el menú y el launcher, y que todo juego nuevo la traiga.

## What Changes

- **Trucos declarados en `game.cfg`**: un juego declara sus trucos, uno por línea (`cheat_1` a `cheat_16`). Cada uno tiene nombre, título, descripción y lo que escribe en la RAM del 68000: dirección, tamaño (byte, palabra o long) y valor. Puede ser de encendido/apagado o tener varias opciones (por ejemplo, un personaje o un arma de una lista), y puede llevar una condición simple: escribir solo mientras otra dirección valga algo (por ejemplo, solo durante la selección de personaje).
- **Página "Cheats"** en el menú del juego (F1/Esc) y en el launcher. Todos los trucos empiezan apagados, se guardan por juego y se pueden prender y apagar en vivo: al apagarlos, el juego sigue normal desde ese cuadro.
- **Se aplican antes de cada cuadro**, también en los cuadros del run-ahead, así lo que se ve coincide con lo que pasa.
- **Trucos de cada juego**, con las direcciones encontradas aquí (las bases de trucos de MAME o FBNeo solo como pista, y cada dirección se comprueba):

  | Juego | Trucos |
  |---|---|
  | SF2 CE y Hyper Fighting | Energía infinita P1, energía infinita P2, tiempo infinito, P2 sin energía (un golpe gana), jugar con los jefes (Balrog, Vega, Sagat, M. Bison) |
  | Ghouls'n Ghosts | Vidas infinitas, tiempo infinito, armadura (normal o dorada), elegir arma, invencible (si la placa guarda un contador de invencibilidad) |

  Un truco que no se pueda hacer con escrituras a RAM queda anotado y fuera.
- **Regla para juegos nuevos**: la skill `cps2recomp` pasa a exigir la página Cheats, igual que la página Board.
- **Rama**: `ghouls`, después del commit de `dip-menu`.

Fuera de alcance:
- **Trucos que cambian el código del juego** (como los de Super Turbo con ganchos): siguen siendo cosa de las fuentes propias.
- **Buscador de RAM dentro del juego** (el que tienen MAME o FBNeo para encontrar trucos nuevos): las direcciones se buscan con `--trace`, `--dump-ram` y `--poke`.
- **Importar archivos de trucos** de otros proyectos.
- **El modo de entrenamiento**: va en otro change, aunque usará direcciones de esta búsqueda.

## Capabilities

### New Capabilities
- `cheats-page`: la página Cheats que el toolkit arma desde `game.cfg`: cómo se declaran los trucos, cómo se muestran, se guardan y se aplican a la RAM, y cómo conviven con el run-ahead y los save states.

### Modified Capabilities
<!-- Ninguna en openspec/specs (no hay specs archivadas). Sigue el mismo patrón que board-settings-page (change dip-menu, sin archivar). -->

## Impact

- **Toolkit**: `port/main.cpp` (leer los trucos, registrar la página y aplicarlos antes de cada cuadro y en el run-ahead), con el mismo patrón que las opciones de placa. `--poke` sigue igual.
- **Juegos**: `games/sf2ce`, `games/sf2hf` y `games/ghouls` (`game.cfg` y README).
- **Launcher**: lo muestra con `--list-features`; se reconstruye.
- **Sin cambios con todo apagado**: mismos hashes por cuadro.
- **Skill** `cps2recomp`: la regla de la página Cheats.
- **Instalación** con respaldo; sin push ni PR sin OK.
