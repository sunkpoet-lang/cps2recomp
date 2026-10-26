# Design

## Context

Ver `proposal.md`. Lo observado:

- **La tabla de canciones de HSF2** (`hs2.01` + `hs2.02`): la palabra en `0x9000` es `0x280` y la tabla empieza en `0x9006`, cuatro bytes por canción, igual que el driver de Super Turbo (`0x1FF` entradas). Las piezas cuyo dato empieza en 0 son música: **68, con exactamente los mismos números** que en Super Turbo (comparado entrada por entrada).
- **Cómo se registra un juego**: con `games/<id>/src/*.cpp`, el toolkit compila esas fuentes y llama a su `RegisterGame()`; sin fuentes usa `runtime/no_game.cpp` (vacío). `games/ssf2t/src/music.cpp` llena `TheGame().songTable`, `songCount` y `songs` (número, nombre, descripción, si suena una vez).
- **El port** (`port/main.cpp`) solo crea la feature `custom_soundtrack` y llama a `FindMusic` cuando `songCount` no es 0. `musicBelow` hace falta solo para drivers donde el dato no distingue música de efectos: aquí sí distingue.
- **El nivel 1** está instalado y es la referencia para comparar hashes (`D:\CPS2\CPS2Recomp\hsf2.exe`, de `c5f78bf`).

## Goals / Non-Goals

**Goals:**
- Que HSF2 tenga custom soundtrack y F12 con el `soundtrack.ini` del CD que ya existe.
- Nombres de canciones iguales a los de Super Turbo donde la pieza es la misma.

**Non-Goals:**
- Cualquier otra función del nivel 2 (free play, Board, training, widescreen).
- Compartir las fuentes de `ssf2t` con `sources = ssf2t` (eso exige un `family.h`; para la música basta copiar la tabla).

## Decisions

### 1. Fuentes propias mínimas: `game.cpp` + `music.cpp`
`games/hsf2/src/game.cpp` define `RegisterGame()`, que solo llama a `RegisterMusic()`. `games/hsf2/src/music.cpp` copia la estructura del de Super Turbo, con `songTable = 0x9006`, `songCount = 0x280` y la lista de nombres.

- **Alternativa descartada:** `sources = ssf2t`. Traería todas las fuentes de Super Turbo, que nombran direcciones de su programa (`At(0x...)`) y exigen un `family.h` completo para compilar. Es el nivel 2 entero.

### 2. Los nombres: los de Super Turbo, confirmados en HSF2
Se parte de la lista de `games/ssf2t/src/music.cpp` con los mismos números. Cada uno se confirma con `CPS2RECOMP_SOUNDTRACK_LOG` en corridas sin ventana con juego inventado (`--auto`), anotando el cuadro de cada petición y mirando la captura de ese cuadro: un número que suena en otra pantalla se renombra o se queda sin nombre.

Las piezas que solo tienen nombre en el `soundtrack.ini` por número (continue `0x37`, game over `0x39`) se nombran aquí como `continue` y `game_over`. Así el `.ini` de HSF2 puede usar nombres. El `.ini` actual las tiene como `[37]` y `[39]`, que funcionan igual porque el número no cambia.

- **Por qué no confiar solo en los números:** el número coincide, pero HSF2 podría usar alguna pieza en otra pantalla (por ejemplo, por la elección de versión del personaje). La spec pide confirmar las pantallas probadas.

### 3. El soundtrack: carpeta propia, los mismos MP3 (decidido por el usuario)
HSF2 tiene su carpeta, `D:\CPS2\CPS2Recomp\AST\HSF2`, con su propio `soundtrack.ini` y ahí escribe su `songs.txt`. Los MP3 **no se copian**: el `.ini` de HSF2 nombra los de `SSF2Turbo` con rutas relativas (`file = ..\SSF2Turbo\06 Track 6.mp3`). El reproductor resuelve un `file` relativo respecto a la carpeta del `.ini` (`port/soundtrack.cpp`: `dir / named`). El `.ini` se genera a partir del de Super Turbo cambiando cada `file` a esa ruta; los cortes (`loop`, `start`) quedan igual.

- **Por qué:** así cada juego escribe su propio `songs.txt` y su `.ini` puede cambiar después sin tocar el de Super Turbo.
- **Alternativa descartada:** compartir la carpeta y el `.ini` de Super Turbo. Los dos `songs.txt` se pisarían.

### 4. Pruebas sin ventana
- **Hashes:** la misma corrida `--auto 1 --frames 6000` con el exe del nivel 1 y con el nuevo, soundtrack apagado, `--hash-log` en ambos; deben ser idénticos.
- **Pantallas:** con el soundtrack encendido y el log, se anota qué número pide cada pantalla.
- **F12:** `--keys <cuadro>:m` a media pelea con `--audio-dump`; la placa debe sonar fuerte tras la tecla, como en Super Turbo.

## Risks / Trade-offs

- **[`--auto` no llega a todas las pantallas (final, créditos, ranking)]** → Esas piezas se nombran como en Super Turbo y quedan marcadas en el README como sin confirmar.
- **[Una pieza suena en otra pantalla que en Super Turbo]** → Se renombra según la pantalla de HSF2 y se agrega al `.ini` si hace falta.
- **[Al tener `src/`, cambia la compilación]** → La prueba de hashes contra el nivel 1 lo cubre.

## Migration Plan

1. Fuentes, compilación y pruebas sin ventana.
2. Respaldo de `hsf2.exe` del nivel 1 en `_respaldo_hsf2\`; copia del nuevo.
3. El usuario elige la carpeta `AST\HSF2` en la página de HSF2, enciende la feature y prueba F12.
4. **Si algo falla:** se devuelve el `hsf2.exe` del respaldo.
