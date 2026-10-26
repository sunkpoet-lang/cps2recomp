# Design

## Context

Ver `proposal.md` (Why) y las specs `soundtrack-live-toggle` y `soundtrack-song-start`. Lo que el código hace hoy y condiciona el diseño:

- **La petición de música** (`machine/machine.cpp`): el 68000 escribe 16 bytes al inicio de la memoria compartida con la parte de sonido (`soundRam`). Escribir el byte `0x0F` con un valor distinto de `0xFF` marca que hay una petición, y en ese momento se llama a `Soundtrack::Request`.
- **La intercepción** (`machine/soundtrack.cpp`): si el jugador tiene archivo para esa música, `Request` reescribe la petición como `FF 05` ("para la música"). La placa nunca se entera de la canción y `state.song` pasa a ser esa canción. Si no hay archivo, `state.song = -1` y la placa la toca. `state` (canción, `serial`, `frames` y fades) se guarda en los save states con `ar.Pod(state)`.
- **El reproductor** (`port/soundtrack.cpp`): calcula la posición en el archivo a partir de `state.frames`, así que un `frames` grande hace que el archivo empiece a mitad, sin código extra.
- **Encender y apagar** (`Port::SoundtrackFollow`, una vez por cuadro): sigue la feature `custom_soundtrack`. Al apagar, vacía `Replace` y pone `state.song = -1`: la placa recupera su música solo desde la siguiente petición. Al encender, abre la carpeta y llena `Replace`: el archivo suena desde la siguiente petición.
- **El HUD** (`port/hud.cpp`): dibuja las líneas que fijan las fuentes del juego (`SetHud`) con la fuente de `recomp-common/text.h`, que es solo ASCII.
- **Save states** (`port/main.cpp`): `kStateVersion = 7`; al leer, `ar.version` dice qué versión se está leyendo.

## Goals / Non-Goals

**Goals:**
- Que el cambio se oiga en el mismo cuadro de la pulsación, en los dos sentidos.
- Que sin pulsar la tecla la memoria del juego y los hashes por cuadro sean idénticos a los de hoy.
- Que todo viva en el toolkit, para que cualquier juego con soundtrack lo tenga.

**Non-Goals:**
- Que la placa retome su música a mitad de canción: su secuenciador no se puede adelantar, así que empieza desde el principio.
- Cambiar al instante cuando el cambio se hace desde el menú: el menú conserva su comportamiento actual (el cambio se oye desde la siguiente canción).
- Mezclar las dos músicas o hacer un fundido entre ellas.
- Netplay: cuando exista (issue #70), la tecla se apaga durante una partida en línea.

## Decisions

### 1. Recordar la última música en el estado del soundtrack

`Soundtrack` guarda, aparte de `state`, un bloque `last` con:
- los 16 bytes de la última petición de música que hizo el juego, tal como la escribió (antes de la intercepción);
- su número de canción (`-1`: ninguna);
- los cuadros que lleva desde entonces.

`Request` lo actualiza con cada petición de música, tenga o no archivo. Los comandos `00` (para todo) y `05` (para la música) lo borran. `Frame()` cuenta sus cuadros.

- **Por qué los 16 bytes y no solo el número:** al reenviarla, la placa recibe la petición exacta que pidió el juego, sin suponer qué significan los otros bytes.
- **Por qué contar sus propios cuadros:** cuando la placa toca la música (sin archivo), `state.frames` no avanza. Al encender hace falta saber cuánto lleva sonando para arrancar el archivo en ese punto.
- **Alternativa descartada:** leer la canción de la memoria del Z80. Depende de cada versión del driver de sonido y no escala a otros juegos.

### 2. Save states: versión 8 con lectura compatible

`Soundtrack::Serialize` escribe `state` como antes y, cuando `ar.version` es 0 (escritura) o 8 o mayor (lectura), también `last`. Al leer una versión 5–7, `last` queda vacío (`-1`). `kStateVersion` pasa a 8 y su comentario lo explica.

- **Alternativa descartada:** meter los campos dentro de `State`. Cambiaría el tamaño de `ar.Pod(state)` y rompería los save states de la versión 7.

### 3. Mandarle una petición a la placa desde el port

`Machine` recibe un método para dejar una petición de 16 bytes en `soundRam` como lo haría el juego: escribe los bytes 0–14 y al final el 15. No pasa por el bus del 68000, así que no llama a `Soundtrack::Request` ni se intercepta. Solo escribe si la ranura está libre (`soundRam[0x0F] == 0xFF`). Si no lo está, devuelve `false` y el port reintenta al inicio de cada cuadro siguiente, hasta 30 cuadros; pasado ese tiempo la descarta.

- **Por qué esperar la ranura libre:** si el juego acaba de dejar una petición que la parte de sonido aún no leyó, escribir encima la borraría.
- **Alternativa descartada:** llamar directamente al driver de sonido (`--sound driver`) o a `SoundBoard::Request`. Funcionaría con un lado de sonido y no con el otro (`z80`). La memoria compartida es la entrada común de los dos.

### 4. La tecla como acción del port, aplicada una sola vez

`Port::ToggleSoundtrack()` hace todo en orden, entre cuadros (nunca dentro de `RunFrame` ni de una máquina de reserva):
1. calcula el nuevo valor: 0 si estaba encendido; si no, el último valor encendido recordado (1 por defecto);
2. llama a `SetFeatureValue("custom_soundtrack", ...)` y a `SoundtrackFollow()`, para que `Replace` quede al día en ese momento;
3. **si encendió** y `last` es una música con archivo: deja una petición `FF 05` para la placa, y pone `state.song = last.song`, `serial++`, `frames = last.frames` y el nivel completo;
4. **si apagó** y `state.song` era una música con archivo: pone `state.song = -1` y deja los 16 bytes de `last` como petición para la placa;
5. muestra el aviso (decisión 6).

Lo llaman F12 en `port/frontend_sdl.cpp` (junto a F11, después de `KeyBound`, que respeta las teclas asignadas al juego) y la tecla de prueba `m` en `Port::Tick` (`--keys TICK:m`).

- **Run-ahead:** `RunAhead` guarda el estado de la máquina principal, corre adelante y lo restaura. Como la petición se escribe en la máquina principal antes de eso, queda dentro del estado guardado y la placa la recibe una sola vez. Las máquinas de reserva (`runtime/spare.h`) no pasan por aquí.
- **Alternativa descartada:** hacer el cambio instantáneo dentro de `SoundtrackFollow`, que corre cada cuadro. El menú también cambiaría al instante (fuera de alcance), y cualquier cambio de la feature, incluidas las películas de prueba con `fNAME=N`, escribiría en la placa. Eso rompería la regla de que la partida no cambia sin la tecla.

### 5. `start` por canción

`File` gana `double start = 0`, leído de `soundtrack.ini` con el mismo `Seconds()` que usa `loop`. El reproductor cuenta las muestras desde `start`: `Place()` y `SeekTo()` suman `start * rate` a la posición. Sin `loop`, el tramo que se repite va de `start` al final del archivo, no de 0. `loop` sigue mandando cuando está escrito, aunque quede antes de `start`. Un `start` inválido, negativo o más allá del largo del archivo se toma como 0.

- **Alternativa descartada:** cortar cada pista en dos archivos con ffmpeg. Duplica 16 MP3 en la carpeta del usuario, y ajustar un corte implicaría volver a cortar el archivo en vez de cambiar un número.

### 6. El aviso en pantalla

El port guarda un texto de aviso y el tick de pantalla en que vence (120 ticks, unos 2 s de pantalla y no de cuadros del juego, para que también venza con el juego en pausa). `Port::Shown`, que compone la imagen de la ventana y de las capturas, lo dibuja arriba y centrado con el mismo estilo que el letrero "STOPPED" que ya pone abajo. Así no se tocan las líneas del HUD que fijan las fuentes del juego. Las corridas sin ventana no muestran el aviso: allí no se fija (la tecla de prueba `m` lo fija solo con ventana).

*(Ajuste durante la implementación: el plan era agregarlo como línea de `HudPanels`; `Shown` ya tenía el patrón del letrero de pausa y evita mezclarlo con las líneas del juego.)*

Los textos son ASCII: "Musica: Arreglada", "Musica: Arcade" y "Musica: sin archivos".

- **Alternativa descartada:** `Menu::Note`. Solo se ve con el menú abierto.
- **Alternativa descartada:** agregar la `ú` a la fuente de `recomp-common`. Es una copia sincronizada con otra repo (`tools/sync-common.sh`) y no vale la pena por un acento.

### 7. La configuración personal, fuera de la repo

El `soundtrack.ini` con las 44 pistas va en la carpeta de la música del usuario (`D:\CPS2\CPS2Recomp\AST\SSF2Turbo`, elegida en el launcher). Las 16 canciones normales usan `loop = 0 - <corte>` y las 16 "later" usan `start = <corte>`. La repo no recibe nada derivado del juego ni de la música (la misma regla que ya sigue el proyecto).

## Risks / Trade-offs

- **[La parte de sonido no lee la petición inyectada en el momento esperado]** → Verificarlo sin ventana con `--sound driver` y con `--sound z80`, volcando el audio (`--audio-dump`) alrededor de una tecla `m`. Si no la toma, esperar a que se libere la ranura también al apagar.
- **[Al encender, `FF 05` corta también un fundido que el juego había empezado]** → Copiar el nivel y el paso de fundido de la placa no es posible. Se acepta: la música arreglada arranca con el nivel completo.
- **[Hashes con la parte de sonido incluida (`--hash-log` sin `--hash-game`) cambian tras pulsar la tecla]** → Es lo esperado: la placa recibió otra petición. La regla de "no cambia" aplica sin pulsar la tecla, y se comprueba con una película sin `m`.
- **[Cortes de la versión acelerada medidos a ±0.5 s; seis inciertos]** → Ajustar los números en `soundtrack.ini` de oído, sin recompilar.
- **[`git pull` de main choca con la rama]** → La rama `f12-soundtrack` se rebasa sobre main. El cambio toca pocos archivos y queda listo para un PR al autor (quitando `openspec/`).

## Migration Plan

1. Implementar en la rama `f12-soundtrack`, compilar ssf2t y correr las pruebas sin ventana: la película de la ladder sin `m` con los mismos hashes que main, más las pruebas con `m`.
2. Respaldar `D:\CPS2\CPS2Recomp\ssf2t.exe` en `D:\CPS2\CPS2Recomp\_respaldo_ssf2t\` y copiar el nuevo.
3. Escribir `soundtrack.ini` en la carpeta de la música y elegirla en el launcher.
4. **Si algo falla:** volver a copiar el exe respaldado. Los save states nuevos (versión 8) no los lee el exe viejo; los de versión 7 siguen sirviendo en los dos.

## Open Questions

- ¿La pista 40 ("Next combat") suena bien como `versus`? Se decide de oído, cambiando solo el `.ini`.
- ¿Conviene usar 37 ("Continue") o los jingles de game over para alguna canción del port? Hoy el port no tiene esas canciones con nombre; se puede ver con `CPS2RECOMP_SOUNDTRACK_LOG` más adelante, sin cambiar este diseño.
