# Design

## Context

Ver `proposal.md`. Lo observado (rama `ghouls` con `dip-menu` implementado, sin commit todavía):

- **Opciones de placa** (`dip-menu`): `port/main.cpp` lee `board_option_N` del `game.cfg` con `ReadBoardOptions`, las registra en `RegisterPortFeatures` y las aplica con `ApplyBoardOptions` justo antes de `beforeFrame` en el bucle principal. Los trucos siguen el mismo patrón.
- **La RAM de trabajo** (FF0000-FFFFFF) se escribe con `SetRam8`, `SetRam16` y `SetRam32` (`runtime/game.h`). Así lo hacen las fuentes de los juegos y `--poke`.
- **Herramientas para buscar direcciones**:
  - `--poke ADDR=BYTE,...` escribe después de cada cuadro;
  - `--trace LOW-HIGH:FILE` guarda esos bytes cuadro a cuadro;
  - `--dump-ram FRAME:file` vuelca la RAM;
  - `--watch` dice quién escribe una dirección;
  - `--movie` y `--auto` reproducen entradas.
- **Run-ahead** (`Port::RunAhead`): corre cuadros extra con `beforeFrame` y luego devuelve la máquina a como estaba con un save state. Lo que se escriba ahí se deshace al cargar ese estado.
- **Super Turbo** ya tiene trucos propios en su página "Cheats" (`no_damage`, `infinite_time`...), con ganchos al código.

## Goals / Non-Goals

**Goals:**
- Que cualquier juego llene su página Cheats solo con `game.cfg`.
- Que con todo apagado no cambie nada, y que el run-ahead dé el mismo resultado con trucos.

**Non-Goals:**
- Reemplazar los trucos con ganchos de Super Turbo.
- Condiciones complejas (y/o, mayor/menor) o escrituras de una sola vez.

## Decisions

### 1. `cheat_1` ... `cheat_16`, uno por línea
```
cheat_1 = cheat_energy_p1 | Infinite energy, player 1 | Player 1's health is refilled every frame. | On: FF83E8.w=0090
cheat_5 = cheat_boss | Play as a boss | Player 1 is a boss, while the players choose. | when FF8004.w=0000 | Balrog: FF864F.b=0A | Vega: FF864F.b=0B | ...
```
Los campos son:
- nombre en `settings.ini`, título y descripción;
- una condición opcional `when ADDR.s=VAL` (o `when ADDR.s!=VAL`);
- las opciones, cada una "nombre: escrituras". Las escrituras van separadas por espacios, `ADDR.s=VAL`, en hex, con `s` = `b`, `w` o `l`.

Un truco de encendido y apagado tiene una sola opción (`On`). El menú muestra "Off" y luego las opciones.
- **Por qué hasta 16:** a diferencia de los DIP, un juego puede querer muchos trucos. Se buscan con `#ifdef GAME_CHEAT_N`, como `board_option_N`.

### 2. Validación al leer
Al arrancar (y con `--list-features`) se comprueba el formato:
- las direcciones dentro de FF0000-FFFFFF;
- el tamaño `b`, `w` o `l`, y la dirección par para `w` y `l`;
- que el valor quepa en el tamaño;
- al menos una opción.

Un error impide arrancar, con el nombre del truco, como `ReadBoardOptions`.

### 3. Registro
Página `{"Cheats", "", "Changes to the game's memory, for trying things out: each off unless chosen, and they can be switched any time."}`, solo si hay trucos. Cada truco es una opción con `kFeatureOwn` y valores `"Off"` más los nombres de sus opciones.
- **Super Turbo ya tiene una página "Cheats"** con sus trucos propios. Si algún día un juego tiene las dos, comparten la página (la misma `category`) y la página no se registra dos veces.

### 4. Cuándo se aplican
`ApplyCheats(machine)` se llama junto a `ApplyBoardOptions`, antes de cada cuadro del bucle principal **y** antes de cada cuadro del run-ahead (`Port::RunAhead`, junto a su `beforeFrame`). Por cada truco con opción elegida:
- se evalúa la condición (si la hay) con `Ram8`, `Ram16` o `Ram32`;
- si se cumple, se hacen sus escrituras en orden.

- **Por qué antes del cuadro y no después** (como `--poke`): el juego corre todo el cuadro con el valor puesto, así el marcador y la lógica lo ven desde el principio. En el run-ahead, los cuadros extra ven lo mismo que verán los reales.
- **Save states:** no guardan nada aparte. Los trucos son opciones, y la RAM ya va en el estado.

### 5. Cómo se buscan las direcciones
Cada dirección se busca y se comprueba en la ROM del juego, de esta forma:
- `--trace` sobre la RAM a lo largo de una corrida con jugadas conocidas (`--movie`), buscando el byte o la palabra que cambia como el dato (la energía baja con cada golpe; el reloj, cada segundo);
- se confirma con `--poke` o con el truco declarado y capturas.

Las bases de trucos de MAME o FBNeo sirven de pista para dónde buscar, nunca como fuente: cada dirección se comprueba aquí y el comentario de `game.cfg` dice cómo se encontró.
- **Hyper Fighting** comparte casi todo el programa con CE, así que se prueba cada dirección de CE en HF. Las que cambien llevan la suya.

### 6. Trucos que pueden no salir
- **"Invencible" en Ghouls**: depende de que el juego tenga un contador de invencibilidad (el parpadeo después de un golpe). Si no lo hay, queda fuera y se anota.
- **"Jugar con los jefes" en SF2**: depende de que el byte de personaje elegido se pueda forzar durante la selección sin colgar el juego. Si no se puede con escrituras, queda fuera y se anota.

## Risks / Trade-offs

- **[Un truco con un valor fuera de rango cuelga el juego]** → Es parte del uso de trucos. Cada truco declarado se prueba sin ventana, y sus opciones son solo valores válidos.
- **[Escribir antes del cuadro choca con el juego, que escribe el mismo dato a mitad del cuadro]** → Para energía o tiempo se vuelve a poner cada cuadro; puede verse un cuadro con la barra baja. Si molesta, se escribe también después del cuadro.
- **[Muchos `#ifdef`]** → Igual que `board_option_N`: código simple y sin generador nuevo.

## Migration Plan

1. Commit de `dip-menu` (con el OK del usuario) y luego este change en la rama `ghouls`.
2. El mecanismo y hashes iguales con todo apagado.
3. Buscar y declarar los trucos de cada juego, cada uno probado sin ventana.
4. Launcher, instalación con respaldo, prueba del usuario y la regla en la skill.
5. **Si algo falla:** los respaldos y el commit anterior.
