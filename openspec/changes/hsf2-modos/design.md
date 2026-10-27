# Design

## Context

Ver `proposal.md`. Lo observado (rama `ghouls`, `e53dfa4`):

- **Fuentes compartidas** (`CMakeLists.txt`): un juego con `sources = <otro>` en su `game.cfg` compila sus `src/*.cpp` **más** todos los `src/*.cpp` del otro, y agrega la carpeta del otro a los includes. No hay reemplazo por nombre ni forma de dejar archivos fuera.
- **La familia de Vampire Savior** (`games/vampire_savior/src/places.h`) nombra cada dirección como `At(0x...)` o `AtA5(...)`, funciones `consteval` que buscan en `family::kPlaces` del juego que compila. Si una dirección no está, la compilación se detiene. Cada juego tiene su `family.h` en su carpeta (no en `src/`): `games/vsav2/family.h` tiene 652 direcciones, y `games/vampire_savior/family.h` ninguna, así que en él cada dirección es la misma. La lista la escribe `tools/places.py` (entre `// places: begin` y `// places: end`) desde el portmap. Una línea `// by hand` se respeta.
- **Super Turbo** (`games/ssf2t/src`, 16 archivos, ~2,500 líneas):
  - nombra sus direcciones directo, como `constexpr` en `ssf2t.h` (44 de RAM o ROM, más tablas) y algunas en los `.cpp`;
  - sus ganchos usan `HookAt(0x...)`;
  - `game.cpp` llama a 14 `Register*`;
  - hay cruces entre archivos, por ejemplo `cheats.cpp` llama a `TagKnockDown` de `livetag.cpp`.
- **HSF2** (`games/hsf2`): `src/game.cpp` (16 líneas) y `src/music.cpp` (su música para el soundtrack y F12). `work/hsf2/portmap.txt` ya existe (Super Turbo → HSF2, `--limit 400000`: el 40.8 % de las instrucciones recorridas, 602 anclas).
- **Memoria de HSF2** (volcados en una pelea, este change): ficha del jugador 1 en `FF833C` (Super Turbo `FF844E`), 0x400 entre fichas, energía en +2A con 0x90 lleno, reloj BCD en `FF8BFC` (Super Turbo `FF8DCE`). Los pasos en `FF8000`-`FF800C` tienen otros valores en pelea (4, 8, 2, 0), porque la máquina de pasos cambió.

## Goals / Non-Goals

**Goals:**
- Que HSF2 tenga los modos de Super Turbo reusando su código, sin copiarlo.
- Que Super Turbo no cambie en nada.

**Non-Goals:**
- Tag team, visor de modelos, pantalla ancha y colores propios en HSF2.
- Que `places.py` encuentre el 100 %: lo que falte se busca a mano.

## Decisions

### 1. Super Turbo con `At()`
`games/ssf2t/src/places.h` nuevo, igual al de Vampire Savior pero en el namespace `ssf2t`; y `games/ssf2t/family.h` con la lista vacía. Cada dirección de `ssf2t.h` y de los `.cpp` pasa a `At(0x...)`, y las relativas a A5 a `AtA5(...)`. Los ganchos usan `HookAt(At(0x...))`.
- **Comprobación:** Super Turbo con los mismos hashes y el mismo audio que antes, y `places.py` debe poder leer todas las direcciones de las fuentes.

### 2. El toolkit: reemplazo por nombre y `sources_leave_out`
En `CMakeLists.txt`, al agregar los `src/*.cpp` de otro juego:
- se salta el que tenga el mismo nombre que uno del juego;
- se salta el que esté en `sources_leave_out = a.cpp b.cpp` del `game.cfg` del juego.

Vampire Savior 2 y Vampire Hunter 2 no tienen archivos propios con el mismo nombre ni la clave nueva, así que no cambian.

### 3. Qué compila HSF2
Compila de Super Turbo:
- `modes.cpp`: modos alternativos, demo y visor;
- `training.cpp`: entrenamiento y dummy;
- `battle.cpp`: batalla infinita, survival y ring out;
- `cheats.cpp`, `coins.cpp` (free play), `title.cpp` (directo al título), `board.cpp` y `computer.cpp`.

Deja fuera (`sources_leave_out`):
- `livetag.cpp`, `model.cpp`, `widescreen.cpp` y `colours.cpp`, del grupo 3;
- `choosing.cpp`, porque HSF2 ya tiene todos los personajes y versiones.

Reemplaza con archivos propios:
- `game.cpp`: su `RegisterGame` llama a los `Register*` de los archivos que compila;
- `music.cpp`: el suyo;
- un `left_out.cpp` con versiones vacías de lo que los archivos compilados llaman de los dejados fuera (por ejemplo `TagKnockDown`, `TeamHoldsKnockOut`). Lo que se necesite se ve al compilar.

### 4. `games/hsf2/family.h`
1. `tools/places.py --sources games/ssf2t/src --from work/ssf2t --to work/hsf2 --map work/hsf2/portmap.txt --family games/hsf2/family.h`.
2. Lo que no encuentre se busca a mano y se marca `// by hand`:
   - **RAM:** volcados de una pelea, `--trace`, y comparar con lo que se sabe de Super Turbo (la ficha corrida 0x112, el reloj 0x1D2; cada una se comprueba);
   - **ROM** (ganchos y tablas): buscar los mismos bytes cerca de lo que el portmap ubicó y confirmarlo con `--watch` o con el efecto del gancho.

`family.h` también lleva lo que en HSF2 es distinto y no es una dirección: los valores de los pasos, la lista de personajes y escenarios, y la versión por defecto.
- **Por qué `places.py`:** es la herramienta del proyecto para esto, y deja escrito cómo se encontró cada dirección.

### 5. Lo propio de HSF2: pasos y versiones
- **Los pasos:** las fuentes de Super Turbo comparan con constantes (`kModeGame`, `kGameMatch`, `kMatchFought`...). Esas constantes pasan a `family.h` (Super Turbo con las suyas), y HSF2 con las que se midan con `--trace` en una partida de un jugador (título, selección de versión, selección de personaje, versus, round).
- **Las versiones:** HSF2 guarda la versión de cada jugador en su ficha; el byte se busca con volcados eligiendo distintas versiones. Los modos que arman una pelea (`modes.cpp`, `battle.cpp`) la ponen junto al personaje. Una opción por luchador en esos modos ("Version": World Warrior, Champion Edition, Hyper Fighting, Super, Super Turbo), con Super Turbo por defecto. Si poner la versión pide más que una escritura, se anota y por ahora queda Super Turbo para todos.

### 6. Board y Cheats
- **Board:** `board.cpp` de Super Turbo, con sus direcciones de HSF2. Los ajustes viven en la EEPROM, y las opciones de la página son las de su menú de test.
- **Cheats:** `cheats.cpp` de Super Turbo (sin daño P1/P2, un golpe gana, tiempo infinito), con ganchos traducidos. Si algún gancho no se encuentra, el truco se hace con `cheat_N` en `game.cfg` (energía y reloj ya conocidos).

## Risks / Trade-offs

- **[Muchas direcciones a mano: el portmap solo cubre el 40.8 %]** → Se hace por archivo, en el orden de las tareas (entrenamiento primero), compilando seguido: la compilación dice cuál falta.
- **[Un gancho en un lugar parecido pero no igual hace otra cosa]** → Cada gancho se comprueba con su efecto (por ejemplo, el daño de un golpe con "No damage") y con `--watch`.
- **[La máquina de pasos de HSF2 tiene pasos de más]** → Las constantes en `family.h` y las funciones que deciden "en pelea" o "eligiendo" se revisan con `--trace` en cada modo.
- **[Romper Super Turbo al pasarlo a `At()`]** → Hashes antes y después, y `ssf2t` no se reinstala si cambia algo.

## Migration Plan

1. Rama `hsf2-modos` desde `ghouls`.
2. Super Turbo con `At()` y el toolkit con reemplazo y `sources_leave_out`; Super Turbo con hashes idénticos.
3. HSF2: `family.h` (herramienta y a mano), su `game.cpp` y `left_out.cpp`; compilar y probar modo por modo.
4. Board y Cheats, versiones, README.
5. Launcher, instalación con respaldo y prueba del usuario.
6. **Si algo falla:** los respaldos, y la rama `ghouls` intacta.
