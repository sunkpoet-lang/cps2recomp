# Design

## Context

Ver `proposal.md`. Lo que se observó y condiciona el diseño:

- **Las ROMs del usuario** (en `D:\CPS2\CPS2Recomp-SuperTurbo\CPS2Recomp\ROM`):
  - `hsf2j.zip`: `hs2j.03c` 6efe661f, `hs2j.04b` 93f2500a, `hs2j.05` dde34a35, `hs2j.06` f4e56dda, `hs2j.07` ee4420fc, `hs2j.08` c9441533, `hs2j.09` 3fc638a8, `hs2j.10` 20d0f9e4, `hsf2j.key` 19455a93.
  - `hsf2.zip` (padre, USA): `hs2.01` c1a13786, `hs2.02` 2d8794aa, `hs2.11m` 0e15c359, `hs2.13m` a6ecab17, `hs2.15m` 10a0ae4d, `hs2.17m` adfa7726, `hs2.19m` bb3ae322, más su programa USA (`hs2u.03`/`04`) y `hsf2.key`, que este port no usa.
  - `hs2j.05`–`10` tienen el mismo CRC que `hs2.05`–`10`: el clon sí trae todo su programa.
- **Tamaños**: programa 4 MB (8 de 512 KB), gráficos 32 MB (4 de 8 MB), samples 8 MB (1 archivo). Son los de SFA3, que ya está en `main`, así que la máquina los soporta.
- **Un juego sin fuentes**: el juego de ejemplo del toolkit (`example/games/example`) es solo un `game.cfg`, y CI lo compila. Por eso `games/hsf2` puede empezar sin `src/`.
- **El sonido**: `sound/driver.cpp` es el driver 2.01b; con otra versión se usa el Z80 traducido (`recompz80`), como pasó con Super Turbo.
- **`tools/portmap.py`**: `--from` y `--to` toman las carpetas `work/<id>` (`program.bin`, `entries.txt`), `--limit` es dónde termina el código y `--entries` escribe los puntos de entrada del primer juego movidos al segundo.
- **La rama del usuario** (`f12-soundtrack`, commit `d8c342f`) sale de `74cdb89`; `origin/main` va en `81cfa99`, con SFA3 y el tag team de Super Turbo. La versión de los save states en `main` sigue en 7.

## Goals / Non-Goals

**Goals:**
- Que HSF2 corra en nativo con la menor cantidad de configuración propia.
- Una medida concreta de cuánto se parece su programa al de Super Turbo.
- Una base al día con `main` y con F12, lista para el nivel 2 o para un PR.

**Non-Goals:**
- Cualquier función propia del juego (free play, Board, training, widescreen, música con nombre).
- Escribir `family.h` o compartir las fuentes de `ssf2t` (eso es el nivel 2).
- Publicar algo (push o PR).

## Decisions

### 1. La rama: `hsf2` desde `origin/main`, con F12 aplicado encima
`git switch -c hsf2 origin/main` y `git cherry-pick d8c342f`. Los choques se resuelven a mano conservando lo del autor y agregando lo de F12. Si `kStateVersion` sigue en 7 en `main`, F12 la deja en 8 como antes.

- **Por qué no partir de `f12-soundtrack`:** el nivel 2 y un posible PR necesitan el código actual del autor (por ejemplo, el tag team de Super Turbo toca las fuentes de `ssf2t` que HSF2 heredaría).
- **Alternativa descartada:** un merge de `main` en `f12-soundtrack`. Mezcla dos trabajos en una rama y ensucia el PR de F12.

### 2. `game.cfg` con el set `hsf2j` y los nombres del clon
`rom_set = hsf2j`, `rom_parent = hsf2`, `rom_name = Hyper Street Fighter II: The Anniversary Edition (Japan 040202)`.
- `program` = `hs2j.03c`…`hs2j.10` con sus CRC.
- `graphics` = `hs2.13m hs2.15m hs2.17m hs2.19m` (un grupo de cuatro).
- `sound_program` = `hs2.01 hs2.02`.
- `samples` = `hs2.11m`.
- `key` = `hsf2j.key`.

Los archivos se buscan por CRC dentro de los zip (`machine/romset.cpp`), así que da igual en qué zip esté cada uno.

- **Por qué `hsf2j`:** es el set que pidió el usuario y el de Japón, como el de Super Turbo.

### 3. Dónde termina el código: empezar con `code_end = 400000`
Todavía no se sabe dónde termina el código de HSF2 (en Super Turbo es `240000`). Se empieza con los 4 MB completos y se ajusta con lo que encuentre `explore`: si encuentra "código" en zonas que son datos (los "data a guess took for code" del resumen de `recomp`), se baja `code_end` hasta el último código real.
- **Alternativa descartada:** copiar el `240000` de Super Turbo. HSF2 tiene más personajes y versiones, así que su código puede pasar de ahí.

### 4. Sembrar los puntos de entrada con los de Super Turbo
Después del primer `make` (que deja `work/hsf2/program.bin`), `portmap.py --from work/ssf2t --to work/hsf2 --limit <code_end> --out work/hsf2/portmap.txt --entries work/hsf2/entries_desde_ssf2t.txt` mapea los programas y mueve los puntos de entrada de Super Turbo a sus lugares en HSF2. Si la plantilla de `entries.txt` del toolkit lo permite, se agregan a los de HSF2 antes del `explore`, para que encuentre más código de entrada. El mapa también es la medida que se pide.
- **Qué se informa:** cuántas instrucciones y rutinas de Super Turbo tienen pareja en HSF2 y qué partes no (de ahí sale la decisión del nivel 2).

### 5. `explore` sin funciones que ayuden
Super Turbo explora con `free_play,no_damage,one_hit`, pero esas funciones salen de sus fuentes. HSF2 nivel 1 no tiene fuentes, así que `explore` corre con jugadas inventadas que meten créditos (`--auto`). Llega menos lejos en la ladder, y por eso importa el sembrado de la decisión 4. Lo que quede sin traducir lo corre el intérprete: el juego funciona igual, solo más lento en esas partes.

### 6. Instalación en `D:\CPS2\CPS2Recomp`
- **Respaldo:** antes de reemplazar, `cps2recomp-launcher.exe` se copia a `_respaldo_hsf2\`.
- **Compilación:** `tools/collection.sh` hace el launcher nuevo, con `hsf2` y también `sfa3` y los demás.
- **Copia:** `hsf2.exe` va junto al launcher.
- **ROMs:** `hsf2j.zip` y `hsf2.zip` se **copian** (no se mueven) a `D:\CPS2\CPS2Recomp\ROM`.

El `ssf2t.exe` instalado (con F12) no se toca en este cambio. Se puede recompilar desde `hsf2` después, cuando se compruebe que el cherry-pick quedó bien.
- **Supuesto:** el usuario quiere HSF2 en su instalación principal, no solo en la copia del ZIP. Si no, se instala en la copia.

## Risks / Trade-offs

- **[El cherry-pick choca con cambios del autor]** → Resolver conservando lo del autor. Recompilar ssf2t y repetir la prueba de F12 con `--keys m` (como en el change `f12-soundtrack`) antes de seguir.
- **[HSF2 usa algo del hardware que Super Turbo y SFA3 no usan]** → La primera corrida sin ventana lo muestra (error de carga, imagen rota o el juego colgado). En ese caso se para, y se investiga en un change aparte.
- **[`explore` con jugadas al azar llega poco lejos]** → Sembrar entradas (decisión 4), y como último recurso dejar más trabajo al intérprete. El juego funciona igual.
- **[`code_end` mal puesto]** → Demasiado alto: se traducen datos como código (más trabajo, pero el juego funciona). Demasiado bajo: código real queda al intérprete. Se ajusta con el resumen de `recomp`.
- **[La clave del set]** → La llave descifra el programa. Si el set no corresponde a la llave, el programa descifrado sale como basura y nada arranca: se ve en la primera corrida.

## Migration Plan

1. Rama `hsf2` + cherry-pick de F12 y prueba de que F12 sigue bien.
2. `games/hsf2`, ROMs en `rom/hsf2/`, `make` ×2, `portmap.py` y `explore`.
3. Prueba sin ventana y medición.
4. Instalación con respaldo; prueba con ventana del usuario.
5. **Si algo falla:** devolver `_respaldo_hsf2\cps2recomp-launcher.exe` y borrar `hsf2.exe`. La rama `f12-soundtrack` queda intacta.

## Open Questions

- ¿Hasta dónde llega el código de HSF2 (`code_end` final)? Se responde con `explore`, sin cambiar el plan.
