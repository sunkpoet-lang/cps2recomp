# Proposal

## Why

El usuario quiere un port nativo de **Ghouls'n Ghosts** (Capcom, 1988) y ya tiene su ROM (`ghouls.zip`, set World `ghouls`). Corre en la misma placa **CPS-1 clásica** que SF2 CE y Hyper Fighting, así que el sonido ya está hecho: el Z80, el YM2151 y los latches de la rama `sf2-sonido`. Lo que falta es lo que hace distinta a una placa de 1988: el 68000 a 10 MHz, ROMs de programa y de gráficos cargadas de otra forma, otro mapper de gráficos y otros DIP switches. Hacerlo bien sirve también para los siguientes juegos CPS-1 (Final Fight, Strider, 1941...), que usan las mismas formas.

## What Changes

- **El reloj del 68000 por juego**: un valor nuevo en `game.cfg` para el reloj de la CPU en la placa CPS-1. Ghouls va a 10 MHz y SF2 a 12 MHz, como hoy; Dino no cambia.
- **Más formas de cargar el programa del 68000**: pares de archivos de a byte (uno con los bytes pares y otro con los impares), y archivos de palabras en el orden del 68000 (sin invertir los bytes), además de la forma de hoy.
- **Más formas de cargar los gráficos**: grupos de ocho archivos de a byte, uno por cada byte de la palabra de 64 bits, además de los grupos de cuatro archivos de dos bytes de hoy.
- **Los DIP switches por juego** en `game.cfg`, con los valores de fábrica de cada juego. Reemplaza el valor fijo de la placa clásica que se agregó para SF2.
  - **Cambio visible en SF2 CE y Hyper Fighting:** pasan a los valores de fábrica de MAME. La dificultad pasa de "0 (la más fácil)" a "4 (normal)" y se permite continuar. Hoy, por leer todos los switches apagados, juegan en la dificultad más fácil y sin continuar.
- **El juego nuevo `games/ghouls`**: set `ghouls` (World), CPS-B-01, el mapper DM620, dos botones (Fire, Jump), sin ROMs de voces, y su sonido FM con el YM2151 que ya existe. Lleva su `game.cfg` con los CRC, su README y su fila en la tabla de juegos.
- **Compilar, explorar y probar sin ventana**: intro, inicio de partida, el primer nivel y una muerte, con música y efectos; y prueba del usuario con ventana.
- **Instalación personal** en `D:\CPS2\CPS2Recomp`: `ghouls.exe` y el launcher, con respaldo. Las ROMs ya están ahí.
- **Rama nueva `ghouls`** desde `sf2-sonido`, solo en el fork, porque el sonido usa ymfm.

Fuera de alcance:
- **Las otras versiones:** `ghoulsu` (USA), `daimakai` y `daimakair` (Japón).
- **Funciones propias:** trucos, selección de nivel, free play con nombre y menú de DIP switches.
- **Las capas de estrellas:** según MAME solo las usan Forgotten Worlds y Strider.
- **El soundtrack propio y F12.**

## Capabilities

### New Capabilities
- `cps1-board-config`: lo que `game.cfg` puede decir de una placa CPS-1: el reloj del 68000, las formas de cargar su programa y sus gráficos, y los valores de sus DIP switches.
- `ghouls-port`: el port de Ghouls'n Ghosts: qué set acepta, que corra en nativo con imagen, controles y sonido, y que esté en el launcher.

### Modified Capabilities
<!-- Ninguna en openspec/specs (todavía no hay specs archivadas). La placa clásica y su sonido viven en los changes sf2-cps1-paso-1 y sf2-cps1-sonido, sin archivar; aquí se agrega a su lado. -->

## Impact

- **Toolkit (rama `ghouls`)**:
  - `machine/romset.h` y `machine/romset.cpp`: las nuevas formas de programa y gráficos, el reloj y los DIP en `Board` o en la lista de ROMs.
  - `machine/machine.cpp`: los ciclos por línea según el reloj y la lectura de los DIP desde la configuración.
  - `port/main.cpp` y `tools/gamecfg.py`: las claves nuevas de `game.cfg`.
  - El launcher, si arma las listas de ROMs él mismo: tiene que entender las formas nuevas para encontrar los archivos.
- **Juegos**: `games/ghouls/` nuevo; `games/sf2ce` y `games/sf2hf` ganan su línea de DIP; la tabla del README principal.
- **Sin cambios para CPS-2 ni para Dino**: se comprueba con los hashes por cuadro de HSF2 y con Dino si el usuario tiene su set.
- **Instalación**: `ghouls.exe` nuevo y el launcher (respaldo en `_respaldo_ghouls`).
- **Nada se publica**: sin push ni PR sin el OK del usuario.
