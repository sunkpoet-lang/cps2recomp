# Design

## Context

Ver `proposal.md`. Lo observado (rama `ghouls`, `4a44c8e`):

- **Opciones** (`runtime/game.h`, `Feature`): nombre, página (`category`), título, descripción, flags y una lista de valores; el primero significa "apagado" o, en las páginas Board, "As the board is set". `FeatureValue(name)` da el elegido.
  - `LoadFeatures` las lee de `settings.ini` y de `--features` (`NAME=N`).
  - El launcher las conoce corriendo el juego con `--list-features` (`port/main.cpp`), así que una opción registrada en el juego aparece en el launcher sin tocarlo.
  - La flag `kFeatureOwn` hace que el launcher de la colección no la cruce con la de otro juego del mismo nombre.
- **Páginas** (`Game::pages`): `port/menu.cpp` las pasa al menú.
- **Cadillacs and Dinosaurs** (`games/dino/src/board.cpp`) ya tiene una página Board con su código propio:
  - campos `{nombre, byte, shift, mask}`;
  - el valor elegido menos uno va en los bits;
  - lo aplica en `beforeFrame`;
  - las descripciones dicen el valor de una placa nueva.
- **Registro**: `RegisterGame` (las fuentes del juego; `runtime/no_game.cpp` si no tiene) y después `port::RegisterPortFeatures` (`port/main.cpp`), tanto al arrancar como con `--list-features`.
- **Ciclo**: el bucle principal llama a `TheGame().beforeFrame` y luego `machine->RunFrame()`. El run-ahead corre cuadros extra con las mismas entradas.
- **DIP switches**: `ReadCps1` devuelve `(m_roms.board.dip[n] << 8) | 0xFF`. `m_roms` es `const RomSet &`, así que hoy no se pueden cambiar mientras corre el juego.
- **`tools/gamecfg.py --header`** convierte cada clave de `game.cfg` en `GAME_<CLAVE>`. Las claves repetidas se pisan (es un diccionario).
- **Menú**: tiene **Reset** ("Starts the board again").

## Goals / Non-Goals

**Goals:**
- Que un juego con DIP llene su página Board solo con su `game.cfg`.
- Que sin elegir nada todo quede idéntico a hoy, y que el run-ahead y los save states no se vean afectados.

**Non-Goals:**
- Cambiar la página Board de Dino ni de los juegos CPS-2: siguen con su código propio.
- Pasar los DIP de la placa a una EEPROM simulada.

## Decisions

### 1. Una clave numerada por opción: `board_option_1` ... `board_option_12`
Cada opción va en su línea, con campos separados por `|`:

```
board_option_1 = board_difficulty | Difficulty | B:0-2 | How hard the game is. A new board: 2. Read as a game begins. | 1 (easiest)=4 | 2=5 | 3=6 | 4 (normal)=7 | 5=3 | 6=2 | 7=1 | 8 (hardest)=0
```

Los campos son: nombre de la opción en `settings.ini`, título, banco y bits (bit menor y mayor), descripción, y los valores, cada uno "nombre=valor" (el valor es el del campo, antes de correrlo a su bit).

- **Por qué numeradas:** `game.cfg` no admite claves repetidas, y una línea por opción se lee mejor que una sola línea con todas.
- **Hasta 12:** `port/main.cpp` las busca con `#ifdef GAME_BOARD_OPTION_1` ... `_12`, como las demás claves opcionales.
- **Alternativa descartada:** fuentes propias por juego, como Dino. Funciona, pero cada juego nuevo pediría código C++, y el usuario quiere que cada juego la traiga.

### 2. Registro en el puerto
`RegisterPortFeatures` (o una función `RegisterBoardOptions` llamada justo antes) lee las claves, valida y registra:
- la página `{"Board", "", "The board's own settings, its DIP switches: each as the board is set unless chosen here."}`, solo si hay al menos una opción;
- cada opción con la flag `kFeatureOwn`, sus valores precedidos por `"As the board is set"`.

Como `Feature` guarda `const char *`, los textos leídos se guardan en almacenamiento estático que dura toda la ejecución. Un error de formato (banco que no es A, B o C; bits fuera de 0-7 o al revés; un valor que no cabe en los bits; ningún valor) se informa al arrancar con el nombre de la opción y el juego no corre.
- **Por qué en el puerto y no en `runtime/game.cpp`:** ahí están los `GAME_*` del juego y las otras opciones del puerto. Así sale lo mismo al arrancar y con `--list-features`.

### 3. DIP switches que el puerto puede cambiar
`Machine` gana `uint8_t dip[3]`, públicos como `testSwitch`, que se copian de `board.dip` al construirse. `ReadCps1` los lee de ahí.
- **Antes de cada cuadro** (junto a `beforeFrame`, en el bucle principal), el puerto parte de `board.dip`, pone los bits de cada opción elegida (valor elegido > 0: el del valor `elegido - 1`) y deja el resultado en `machine->dip`.
- **No se guardan en los save states:** se recalculan desde las opciones antes de cada cuadro. Así un save state no fija los DIP y el run-ahead los ve iguales.
- **Por qué no `beforeFrame`:** es el gancho de las fuentes del juego (Dino lo usa). Uno del puerto no choca con él.

### 4. Cuándo hace efecto
Se aplica antes de cada cuadro y el juego lee los DIP cuando quiere. Para cada opción se mira en una corrida sin ventana (con un log de lecturas a 0x80001A-1F, o probando) si la lee al instante, al empezar partida o solo al arrancar, y la descripción lo dice ("Read as a game begins", "Takes effect after Reset"...).

### 5. Las opciones de cada juego (de MAME)
**Ghouls** (`dip = FF FD FF`):
- Difficulty B:0-2: 1 (easiest)=4, 2=5, 3=6, 4 (normal)=7, 5=3, 6=2, 7=1, 8 (hardest)=0.
- Lives C:0-1: 3=3, 4=2, 5=1, 6=0.
- Bonus life B:4-5: 10K, 30K and every 30K=2; 20K, 50K and every 70K=1; 30K, 60K and every 70K=3; 40K, 70K and every 80K=0.
- Continue C:6: No=0, Yes=1.
- Demo sound C:5: Off=0, On=1.

**SF2 CE y Hyper Fighting** (`dip = FF FC 9F`):
- Difficulty B:0-2: 0 (easiest)=7, 1=6, 2=5, 3 (normal)=4, 4=3, 5=2, 6=1, 7 (hardest)=0.
- Free play C:2: Off=1, On=0.
- Continue C:6: No=1, Yes=0.
- Demo sound C:5: Off=1, On=0.
- 2 coins to start, 1 to continue A:6: Off=1, On=0.

Los nombres de las opciones en `settings.ini` son los de Dino (`board_difficulty`, `board_lives`, `board_extend`, `board_continue`, `board_demo_sound`) más `board_free_play` y `board_two_coins`.

### 6. Texto de SF2
En los README y en los comentarios de `game.cfg` de SF2, "difficulty 4 (normal)" pasa a "difficulty 3 (normal)", como lo llama MAME. El valor no cambia.

## Risks / Trade-offs

- **[Un juego lee un DIP solo al encender]** → La descripción dice "after Reset" y el menú tiene Reset.
- **[Cambiar un DIP a media partida confunde al juego]** → Es lo que haría la placa real con el mismo switch. Las opciones que se lean en vivo de forma rara (si aparecen) se marcan en su descripción.
- **[Nombres repetidos entre juegos en el launcher]** → `kFeatureOwn`.
- **[Validación débil del formato]** → Errores al arrancar, con el nombre de la opción; se prueba con una opción rota a propósito en una compilación de prueba.

## Migration Plan

1. Sobre la rama `ghouls`.
2. DIP cambiables en `Machine` y las opciones en el puerto; verificar con hashes que todo queda igual sin elegir nada.
3. Las opciones de los tres juegos; pruebas sin ventana con `--features`.
4. Launcher, instalación con respaldo y prueba del usuario.
5. **Si algo falla:** el respaldo de los exes y del launcher, y la rama en `4a44c8e`.
