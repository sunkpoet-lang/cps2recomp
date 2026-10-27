# Proposal

## Why

El usuario quiere en Hyper Street Fighter II: The Anniversary Edition (`hsf2j`) los modos que ya tiene Super Turbo: el modo de entrenamiento y los modos alternativos. Hoy HSF2 es un port básico, sin funciones propias. Su memoria guarda a cada jugador con la misma ficha que Super Turbo, solo en otro lugar:

| | Super Turbo | HSF2 |
|---|---|---|
| Ficha del jugador 1 | `FF844E` | `FF833C` |
| Separación entre fichas | 0x400 | 0x400 |
| Energía | ficha + 2A | ficha + 2A |
| Reloj | `FF8DCE` | `FF8BFC` |

Además, los dos programas comparten el 40.8 % de su código. Por eso las fuentes de Super Turbo (~2,500 líneas) se pueden reusar con la traducción de direcciones que ya usa la familia de Vampire Savior. Por la regla de la skill, HSF2 también tiene que traer sus páginas Board y Cheats.

## What Changes

- **Las fuentes de Super Turbo pasan a nombrar sus direcciones con `At()` y `AtA5()`**, como las de Vampire Savior, con un `family.h` vacío para Super Turbo. Super Turbo no cambia: mismos hashes.
- **HSF2 compila las fuentes de Super Turbo** (`sources = ssf2t` en su `game.cfg`) y su `family.h` dice dónde está cada dirección en HSF2. La lista la genera `tools/places.py` con el portmap ya hecho, y lo que la herramienta no encuentre se busca a mano y queda marcado `// by hand`.
- **Las fuentes que HSF2 no usa quedan fuera:**
  - el tag team (`livetag.cpp`);
  - el visor de modelos (`model.cpp`);
  - la pantalla ancha (`widescreen.cpp`);
  - los colores propios (`colours.cpp`);
  - los personajes ocultos y viejos de Super Turbo (`choosing.cpp`), porque HSF2 ya trae todas las versiones;
  - la música (`music.cpp`), porque HSF2 tiene la suya.

  Para eso, el toolkit aprende dos cosas:
  - un archivo propio del juego con el mismo nombre reemplaza al compartido;
  - una clave nueva, `sources_leave_out`, deja fuera archivos compartidos.
- **En HSF2, desde el menú F1/Esc y el launcher:**
  - **Modo de entrenamiento:** energía y barra que se rellenan, readouts de combo y stun, input display, hitboxes, frame data y frame meter, y el escenario y el punto de reinicio.
  - **Dummy:** parado, agachado, salta, graba y repite (con slots), bloquea y responde.
  - **Modos alternativos:** batalla infinita, survival, ring out, modo demo y visor de escenarios.
  - **Juego:** free play y "directo al título".
  - **Página Board** (dificultad, velocidad, continuar, demo sound) y **página Cheats** (sin daño P1/P2, un golpe gana, tiempo infinito).
- **Lo propio de HSF2** (que Super Turbo no tiene):
  - **La selección de versión** (World Warrior, CE, Hyper Fighting, Super, Super Turbo): los modos que arman una pelea le ponen a cada luchador una versión. Por defecto, Super Turbo.
  - **Los pasos del juego:** el paso de la selección de versión cambia los valores de la máquina de pasos (`FF8000`... en HSF2 no son los de Super Turbo).
- **Rama `hsf2-modos`** desde `ghouls` (`e53dfa4`), en el fork.

Fuera de alcance:
- **Lo que queda fuera de las fuentes:** tag team, visor de modelos, pantalla ancha y colores propios (grupo 3, otro change).
- **Los personajes ocultos de HSF2.**
- **SF2 CE y Hyper Fighting:** van en otro change.

## Capabilities

### New Capabilities
- `shared-game-sources`: cómo un juego compila las fuentes de otro hecho del mismo programa (direcciones traducidas, archivos propios que reemplazan a los compartidos, archivos dejados fuera), extendido a la familia de Super Turbo.
- `hsf2-modes`: los modos de HSF2: entrenamiento, dummy, modos alternativos, Board y Cheats, y cómo eligen versión.

### Modified Capabilities
<!-- Ninguna en openspec/specs (no hay specs archivadas). -->

## Impact

- **Toolkit:** `CMakeLists.txt` (reemplazo por nombre y `sources_leave_out`).
- **Super Turbo:** `games/ssf2t/src/*` (las direcciones con `At()`), `games/ssf2t/src/places.h` y `games/ssf2t/family.h` nuevos. Sin cambio de comportamiento.
- **HSF2:** `games/hsf2/game.cfg`, `games/hsf2/family.h` nuevo, `games/hsf2/src/` (su `game.cpp` llama a los registros compartidos, más los reemplazos que haga falta), README.
- **Launcher:** se reconstruye; las opciones salen con `--list-features`.
- **Instalación** de `hsf2.exe`, `ssf2t.exe` y el launcher, con respaldo; sin push ni PR sin OK.
