# Design

## Context

Ver `proposal.md`. Lo observado en el código (rama `hsf2`):

- **`board = cps1`** (`machine/romset.cpp`, `ParseRomList`) marca `Board.cps1` y lee `cps_b`: nueve bytes, con los valores del CPS-2 como predeterminados.
  - El cargador da por hecho un CPS-1 con QSound: tras leer el programa de sonido busca la clave Kabuki (`FindKabukiKey`) y, si no la encuentra, rechaza el set ("the sound program's cipher could not be worked out").
  - `machine/machine.cpp`: en CPS-1 el Z80 corre desde el inicio (`m_soundRunning = true` en `Reset`).
- **`ReadCps1` / `WriteCps1`**:
  - **Jugadores 1 y 2:** se leen en `0x800000`, el palo y tres botones (`p & 0x7F`); monedas y Start en `0x800018`.
  - **DIP switches** (`0x80001A`–`0x80001F`): devuelven `0xFFFF`, todos apagados.
  - **Registros del CPS-B** (`0x800140`–`0x80017F`): devuelven `0xFFFF`, así que `0x800176` (las patadas de SF2) hoy lee "nada presionado".
  - **Música:** el CPS-1 clásico la pide por un latch (`0x800180`/`0x800188`), que hoy no está mapeado.
- **Botones** (`machine/machine.h`): `kKick1..3` son los bits 7 a 9 del jugador.
- **`board` y `cps_b`** pasan de `game.cfg` a la compilación por `tools/gamecfg.py` y `build.mk` (`-DBOARD`, `-DCPS_B`).
- **Los sets del usuario** (`D:\CPS2\CPS2Recomp\ROM`, ambos merged):
  - `sf2ce`: `s92e_23b.8f` 0aaa1a3a, `s92_22b.7f` 2bbe15ed, `s92_21a.6f` 925a7877.
  - `sf2hf`: `s2te_23.8f` 2dd72514, `s2te_22.7f` aea6e035, `s2te_21.6f` fd200288.
  - **Comunes a los dos:**
    - gráficos, en el orden de la placa: `s92-1m.3a` 03b0d852, `s92-3m.5a` 840289ec, `s92-2m.4a` cdb5f027, `s92-4m.6a` e2799472, `s92-5m.7a` ba8a2761, `s92-7m.9a` e584bfb5, `s92-6m.8a` 21e3f87d, `s92-8m.10a` befc47df, `s92-10m.3c` 960687d5, `s92-12m.5c` 978ecd18, `s92-11m.4c` d6ec9a0a, `s92-13m.6c` ed2c67f6;
    - Z80: `s92_09.11a` 08f6b60e;
    - OKI: `s92_18.11c` 7f162009, `s92_19.12c` beade53f.
- **El usuario no tiene el set `dino`**, así que no hay cómo comparar hashes de Dino en esta PC.

## Goals / Non-Goals

**Goals:**
- Que SF2 CE y Hyper Fighting se jueguen en silencio con sus seis botones.
- No cambiar nada para los juegos con `board = cps1`.

**Non-Goals:**
- El sonido (YM2151, OKI), el latch de música y el soundtrack.
- DIP switches con nombre, free play y funciones de juego.
- Que Hyper Fighting comparta fuentes con Champion Edition.

## Decisions

### 1. `board = cps1-classic` y un indicador `qsound` en `Board`
`ParseRomList` acepta `cps1-classic`: marca `cps1 = true` y un campo nuevo, `qsound = false`. Para `cps1` (Dino) queda `qsound = true`, y CPS-2 no cambia. Todo lo nuevo pregunta por `cps1 && !qsound`, así que el camino de Dino no cambia en nada.

En la placa clásica el cargador:
- comprueba los CRC del programa de sonido y de los samples;
- copia el programa de sonido tal cual en `soundOpcodes` (sin Kabuki);
- no reordena los samples como QSound.

- **Alternativa descartada:** una clave nueva en `game.cfg` (`sound = ...`). Habría que pasarla por `gamecfg.py`, `build.mk` y CMake. `board` ya viaja por todo ese camino y describe justo esto: qué placa es.

### 2. Z80 detenido y escrituras de sonido aceptadas
En `Reset`, en la placa clásica el Z80 no arranca (`m_soundRunning = false`). La parte de sonido no hace nada, pero el tiempo de la máquina sigue pasando (`soundCpu.cycles` avanza, como ya pasa con un Z80 en reset). En `WriteCps1`, las escrituras a `0x800180`–`0x80018F` (el latch y su compañero) se aceptan sin efecto, sin `Unmapped`. El paso 2 conectará ahí el latch al Z80 y los chips.

- **Por qué detenido y no corriendo:** sin YM2151 ni OKI, el programa del Z80 leería estados de chips que no existen y podría quedarse esperando para siempre. Detenido es predecible y no cambia nada que vea el 68000.

### 3. Patadas en `0x800176`
En `ReadCps1`, antes de la regla de los registros del CPS-B y solo en la placa clásica, `0x800176` devuelve las patadas activas en bajo:
- **jugador 1:** `kKick1..3` en los bits 0 a 2;
- **jugador 2:** en los bits 4 a 6;
- los demás bits en 1.

Es como MAME describe el puerto `IN2` de SF2. El orden exacto de los bits (ligera, media, fuerte) se comprueba en una corrida con cada patada.

### 4. El CPS-B de SF2
Los dos juegos usan los valores del CPS-2, que también son los del CPS-B-21 de SF2 en la tabla de MAME:

```
cps_b = 26 28 2A 2C 2E 30 02 04 08
```

Si la primera corrida muestra capas en mal orden o un bloqueo porque el juego lee un ID del chip, se corrige en ese momento:
- **capas en mal orden:** los valores de `cps_b`, en `game.cfg`, sin cambiar el plan;
- **lectura de un ID:** un valor que devolver en `ReadCps1` solo para la placa clásica.

### 4b. El multiplicador del CPS-B-21 (encontrado en la primera pelea)
Al empezar una pelea, SF2 CE escribe dos factores en `0x800140` y `0x800142`, y lee el producto en `0x800144` para elegir la entrada de una tabla de saltos (`0x003B4C`–`0x003B6A`). El CPS-1 del toolkit responde `0xFFFF` a ese rango, así que el juego saltaba a `0x4ED40004` y se reiniciaba, también con `--cpu interp`. En la placa clásica, `0x800144` y `0x800146` devuelven la parte baja y alta del producto de `cpsB[0] * cpsB[1]`, como ya hace el CPS-2. Dino no cambia.

### 5. Los dos juegos sin fuentes, con su propia exploración
`games/sf2ce` y `games/sf2hf` son solo un `game.cfg` y un README cada uno (sin `src/`, como el nivel 1 de HSF2), con `code_end` a 180000 (los tres archivos de programa, 1.5 MB). Como no tienen fuentes, no hay `free_play`: `explore` usa `--auto`, que mete monedas. Antes del `explore` de Hyper Fighting se prueba `portmap.py --from work/sf2ce --to work/sf2hf` para sembrar sus entradas, como se hizo con HSF2. Solo si ayuda.

### 7. El mapper de gráficos del CPS-1 (agregado tras la primera corrida, con OK del usuario)
La primera corrida de `sf2ce` mostró los sprites bien y los tres fondos revueltos. `video/video.cpp` da por hecho que en CPS-1 los números de pieza de los fondos son su lugar en las ROMs ("the games here have them in order"), y eso vale para Dino. SF2 usa el PAL **S9263B**, que reparte cada tipo de gráfico en bancos.

- **La tabla:** cada juego la declara en su `game.cfg` como `gfx_map`, con los tamaños de banco y los rangos (tipo, inicio, fin, banco), en unidades de 64 bytes, como la tabla de MAME para ese PAL. Para S9263B:
  - bancos de `0x8000`, `0x8000` y `0x8000`;
  - sprites `0`–`7FFF` en el banco 0, `8000`–`FFFF` en el 1 y `10000`–`11FFF` en el 2;
  - scroll3 `2000`–`3FFF`, scroll1 `4000`–`4FFF` y scroll2 `5000`–`7FFF`, todos en el banco 2.
- **Cómo llega al juego:** `tools/gamecfg.py --header` ya escribe cada clave como `GAME_<CLAVE>`, así que `GAME_GFX_MAP` llega sin tocar el build. `port/main.cpp` la pasa a `Board` (`machine/romset.h`).
- **Cómo se aplica:** en `DrawScroll`, solo si hay tabla, el número de pieza pasa a unidades (`<< 0` para scroll1, `<< 1` para scroll2, `<< 3` para scroll3), se busca su rango y se calcula el lugar: la suma de los bancos anteriores más la unidad dentro de su banco. Una pieza fuera de todo rango no se dibuja, como en la placa.
- **Lo que no cambia:** sin tabla todo sigue como hoy (Dino, CPS-2). Los objetos no pasan por la tabla: en S9263B sus rangos dejan cada pieza en su lugar.

- **Verificación:** el texto del aviso inicial y los fondos de la intro y de las peleas se leen bien en las capturas.

### 6. Rama e instalación
- **Rama:** `sf2-cps1`, desde `hsf2`, para que la colección y el ZIP puedan tener todos los juegos.
- **Respaldo:** el launcher instalado va a `_respaldo_sf2\` antes de copiar el nuevo.
- **Instalación:** `sf2ce.exe` y `sf2hf.exe` van junto al launcher. Las ROMs ya están en `D:\CPS2\CPS2Recomp\ROM`.

## Risks / Trade-offs

- **[Dino cambia sin que lo notemos (no hay ROM para comprobarlo)]** → Todo el código nuevo va condicionado a `!qsound`. Se compila y corre el juego de ejemplo (`make example`, `make smoke`) y se revisa el diff para confirmar que nada del camino `qsound` cambió.
- **[El juego revisa el ID del CPS-B o algo de la placa clásica que falta]** → La primera corrida corta sin ventana lo muestra (bloqueo, pantalla negra o `Unmapped` repetido). El ajuste se hace dentro de este change si es acotado; si no, se para y se anota.
- **[Efectos de video de SF2 que la placa CPS-1 del toolkit no tiene]** (scroll por líneas, estrellas) → Se ven en las capturas de la pelea. Si faltan, quedan anotados como trabajo aparte; el juego se puede jugar igual.
- **[DIP switches "todos apagados"]** → Si dejan el juego en un modo raro (por ejemplo, de prueba), se devuelve en la placa clásica el valor de fábrica de los DIP de SF2 y se anota.
- **[El orden de las patadas]** → Se comprueba con una corrida por patada.

## Migration Plan

1. Rama, cambios del toolkit y comprobación con el juego de ejemplo.
2. `sf2ce`: `game.cfg`, ROMs en `rom/sf2ce/`, `make` ×2, `explore`, prueba sin ventana.
3. `sf2hf`: lo mismo (más el sembrado opcional).
4. Instalación con respaldo y prueba del usuario.
5. **Si algo falla:** se devuelve el launcher del respaldo y se borran los dos exes.

## Open Questions

- El `code_end` final de cada juego se ajusta con lo que encuentre `explore`, sin cambiar el plan.
