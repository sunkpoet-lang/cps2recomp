# Proposal

## Why

En Ghouls'n Ghosts, SF2 CE y Hyper Fighting los DIP switches están fijos en `game.cfg` con los valores de fábrica: para cambiar la dificultad, las vidas o el free play hay que editar un archivo y recompilar. El usuario necesita esos ajustes a mano y fáciles de usar, desde el menú del juego (F1/Esc) y desde el launcher, como ya los tiene Cadillacs and Dinosaurs. Además pidió que todo juego nuevo los traiga, así que tiene que ser una base que cualquier juego de la placa CPS-1 clásica llene solo con su `game.cfg`.

## What Changes

- **Opciones de placa declaradas en `game.cfg`**: cada juego CPS-1 puede declarar opciones con nombre (título, descripción, banco y bits de DIP, y cada valor con su nombre). El toolkit las convierte en opciones de una página **"Board"**, en el menú del juego y en el launcher, sin código propio del juego.
- **"As the board is set" por defecto**: cada opción empieza ahí, es decir, en el valor que trae la línea `dip` (los de fábrica). Lo que el jugador elige se guarda por juego en `settings.ini`, como las demás opciones.
- **Se aplica en vivo**: el valor elegido se pone en los DIP switches antes de cada cuadro. El juego lo lee cuando lo necesita: algunas opciones hacen efecto al instante y otras al empezar una partida o con **Reset**. La descripción de cada opción lo dice, comprobado juego por juego.
- **Las opciones de cada juego** (de MAME; quedan fuera el modo de prueba, voltear la pantalla, freeze, el tipo de gabinete y las monedas por crédito):

  | Juego | Opciones |
  |---|---|
  | Ghouls'n Ghosts | Dificultad (1-8), Vidas (3-6), Vida extra, Continuar, Demo sound |
  | SF2 CE y Hyper Fighting | Dificultad (0-7), Free play, Continuar, Demo sound, 2 monedas para empezar y 1 para continuar |

- **Corrección de texto**: los README y los `game.cfg` de SF2 dicen "dificultad 4 (normal)", pero en MAME el valor de fábrica (0x04) se llama **"3 (Normal)"**. El valor no cambia, solo el nombre.
- **Rama**: se trabaja en la rama `ghouls`, sobre `4a44c8e`.

Fuera de alcance:
- **Juegos CPS-2 y CPS-1 con QSound**: guardan sus ajustes en la EEPROM; los que tienen página Board la hacen con fuentes propias.
- **HSF2 y Super Turbo**: revisar si ya la tienen queda para otro change.
- **Coin A/B**: las monedas por crédito y el gabinete.

## Capabilities

### New Capabilities
- `board-settings-page`: la página Board que el toolkit arma desde `game.cfg` para una placa con DIP switches: cómo se declaran las opciones, cómo se muestran y se guardan, y cómo llegan al juego.

### Modified Capabilities
<!-- Ninguna en openspec/specs (no hay specs archivadas). El change ghouls (sin archivar) define la línea `dip`; aquí se usa como "As the board is set". -->

## Impact

- **Toolkit**:
  - `port/main.cpp`: leer las opciones del `game.cfg`, registrarlas como features de la página Board y aplicarlas antes de cada cuadro.
  - `machine/machine.h` y `machine/machine.cpp`: DIP switches que se pueden cambiar mientras corre el juego (hoy se leen de `RomSet::board`, que es constante).
  - `tools/gamecfg.py` solo si hace falta para las claves nuevas.
- **Juegos**: `games/ghouls`, `games/sf2ce` y `games/sf2hf` (`game.cfg` y README).
- **Launcher**: muestra las opciones con lo que ya pregunta al juego (`--list-features`); hay que reconstruirlo.
- **Sin cambios** con las opciones en "As the board is set": mismos hashes por cuadro que hoy.
- **Instalación** con respaldo; sin push ni PR sin OK.
