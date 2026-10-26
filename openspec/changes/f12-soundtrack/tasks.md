# Tasks

## 1. Línea base

- [x] 1.1 Desde `main` (antes de tocar código), correr `make GAME=ssf2t check FRAMES=40000 MOVIE=games/ssf2t/movies/ladder.txt ARGS="--features free_play,no_damage,one_hit"` guardando un `--hash-log`, y verificar que el archivo de hashes exista para compararlo después (guardarlo fuera de la repo, en el scratchpad).

## 2. `start` por canción (spec `soundtrack-song-start`)

- [x] 2.1 Agregar `start` a `File` y leerlo de `soundtrack.ini` en `port/soundtrack.cpp` con `Seconds()`; los valores inválidos o negativos quedan en 0. Verificar con `--soundtrack` y `CPS2RECOMP_SOUNDTRACK_LOG` que la carpeta abre sin errores con y sin `start`.
- [x] 2.2 Sumar `start` en `Place()`/`SeekTo()` y usar `start` como inicio del tramo que se repite cuando no hay `loop`; un `start` más allá del largo se toma como 0. Verificar sin ventana con `--soundtrack-dump` que `ryu_later` con `start = 60.43` empieza en el segundo 60.43 del MP3 (comparando las primeras muestras del volcado con el MP3 decodificado por ffmpeg desde 60.43) y que, al terminar el archivo, vuelve a `start` y no a 0.
- [x] 2.3 Documentar `start` en la cabecera de `port/soundtrack.h` y en la sección "A custom soundtrack" del README, con el ejemplo de una pista que trae dos canciones; verificar que el ejemplo del README use la misma sintaxis que acepta el código.
- [x] 2.4 *(Encontrada en la prueba del usuario: micropausa al entrar la versión acelerada.)* Dar a los MP3 una tabla de seek, calculada una vez por archivo y guardada (`port/decoders.cpp`), y compensar el cuadro de información Xing/LAME, que la tabla cuenta como audio. Verificado: seek al segundo 60.43 de 69 ms a 0.07 ms; con un programa de prueba, el seek con tabla coincide muestra por muestra con el decodificado completo en 0, 0.01, 0.05, 5, 60.43 y 100 s; en el juego, `guile_later` entra en 67.92 + 0.05 = 67.970 s.

## 3. Recordar la última música (spec `soundtrack-live-toggle`: save states)

- [x] 3.1 Agregar el bloque `last` (16 bytes de la petición, canción, cuadros) a `Soundtrack` en `machine/soundtrack.h`/`.cpp`: `Request` lo actualiza con toda petición de música (antes de interceptarla), `00`/`05` lo borran y `Frame()` cuenta sus cuadros. Verificar con `CPS2RECOMP_SOUNDTRACK_LOG` en una corrida sin ventana que la última canción registrada coincide con la del log.
- [x] 3.2 Serializar `last` solo para versión ≥ 8 (escritura siempre) y subir `kStateVersion` a 8 con su comentario. Verificar que un save state de versión 7, hecho con el exe de `main`, carga con el exe nuevo (`--state-load`) y que uno de versión 8 se guarda y se vuelve a cargar con la misma canción en `last`.
- [x] 3.3 Repetir la película de la tarea 1.1 con la rama y verificar que los hashes por cuadro son idénticos a la línea base, con el soundtrack apagado y encendido (`--features custom_soundtrack`).

## 4. Petición a la placa y la acción de alternar (spec `soundtrack-live-toggle`)

- [x] 4.1 Agregar a `Machine` el método que deja una petición de 16 bytes en `soundRam` solo si `soundRam[0x0F] == 0xFF`, escribiendo el byte 15 al final, y devuelve si pudo. Verificar sin ventana que una petición inyectada de la canción de Ryu suena en el volcado de audio (`--audio-dump`) con `--sound driver` y con `--sound z80`.
- [x] 4.2 Implementar `Port::ToggleSoundtrack()` según el diseño (decisión 4): el nuevo valor de la feature, `SoundtrackFollow()` inmediato, `FF 05` al encender o los bytes de `last` al apagar, y la cola de reintento de hasta 30 cuadros cuando la ranura está ocupada. Verificar con `--keys` que dos alternancias seguidas dejan la feature en su valor original (incluido el 2, aleatorio).
- [x] 4.3 Agregar la tecla de prueba `m` en `Port::Tick` y su línea en `--help`. Verificar con `--keys 900:m --audio-dump` (soundtrack encendido, a media pelea) que antes del tick suena el archivo y después la placa con el tema desde su principio, y lo contrario con el soundtrack apagado.
- [x] 4.4 Verificar con run-ahead encendido (`--features run_ahead` y `--keys ...:m`) que la placa recibe la petición una sola vez, contando las peticiones con `CPS2RECOMP_SOUNDTRACK_LOG`.
- [x] 4.5 Verificar los casos límite de la spec: encender con una música sin archivo (la placa sigue), apagar sin música sonando (la placa no recibe nada) y encender con la carpeta vacía (la placa sigue y el aviso dice "sin archivos").

## 5. F12 y el aviso en pantalla (spec `soundtrack-live-toggle`)

- [x] 5.1 Agregar el aviso con vencimiento en ticks de pantalla y dibujarlo en `Port::Shown` (arriba, centrado, con el estilo del letrero "STOPPED"), sin fijarlo en corridas sin ventana. Verificar con una captura con ventana (`--keys ...:m --shot-every`) que el letrero se ve y que `--shot-every` sin ventana no lo muestra.
- [x] 5.2 Atender F12 en `port/frontend_sdl.cpp` junto a F11, después de `KeyBound`, llamando a `Port::ToggleSoundtrack()`. Verificar en la ventana: F12 a media pelea cambia la música al instante en los dos sentidos y muestra "Musica: Arreglada" / "Musica: Arcade" unos 2 s; con F12 asignado a un botón, solo se aprieta el botón. *(Confirmado por el usuario en ventana: F12 en los dos sentidos con letrero. Con F12 asignado a un botón: por código, F12 se atiende solo tras `KeyBound`.)*
- [x] 5.3 Documentar F12 en la sección "A custom soundtrack" del README y en la lista de teclas del juego, si la hay (`grep -n F11 README.md games/ssf2t/README.md`); verificar que la lista quede igual a lo que hace `frontend_sdl.cpp`.

## 6. Instalación personal y prueba con la música

- [x] 6.1 Escribir `soundtrack.ini` en `D:\CPS2\CPS2Recomp\AST\SSF2Turbo` con la tabla identificada: 03–18 con `loop = 0 - <corte>` y sus `_later` con `start = <corte>`, `final_fight`/`final_fight_later` con la pista de Bison, `title` = 01, `player_select` = 02, `versus` = 40 y `ryu_ending` = 33. Verificar con `--soundtrack` y `CPS2RECOMP_SOUNDTRACK_LOG` que reconoce todas las canciones del `.ini`.
- [x] 6.2 Compilar ssf2t (`make GAME=ssf2t`), respaldar `D:\CPS2\CPS2Recomp\ssf2t.exe` en `D:\CPS2\CPS2Recomp\_respaldo_ssf2t\` y copiar el nuevo. Verificar que el respaldo existe y que el exe nuevo arranca con `Jugar Super Turbo.bat` y desde el launcher. *(El `.bat` ahora llama a `cps2recomp-launcher.exe --play ssf2t`, para que use los ajustes del launcher y la carpeta del soundtrack; verificado por la línea de comandos con que abre `ssf2t.exe`.)*
- [x] 6.3 Prueba del usuario en la ventana: elegir la carpeta en el launcher, encender el soundtrack, jugar contra varios escenarios, alternar con F12 y confirmar de oído los cortes dudosos (Blanka, Guile, Vega, Cammy, Sagat, Zangief) y `versus`; ajustar el `.ini` si hace falta. *(Hecha: carpeta, tema arreglado y F12 con letrero OK; sin corte ni micropausa al entrar la versión acelerada; `.bat` con música; versus = 40, ganador = 38, continue (0x37) = 37 y game over (0x39) = 42 confirmados de oído; la intro suena con "Demo sound" en On. Los cortes siguen ajustables en el `.ini` sin recompilar.)*

## Workflow follow-up

- Hacer commit en la rama `f12-soundtrack` cuando el usuario lo apruebe (Conventional Commits en español, según su memoria).
- Antes de proponer el PR al autor: rebasar sobre `main`, quitar `openspec/` del PR y escribir la descripción en inglés.
- Archivar el change con `/opsx:archive` cuando la prueba del usuario esté hecha.
