# Tasks

## 1. Base

- [x] 1.1 Confirmar que la copia de trabajo puede ir a la rama `killer-instinct` sin perder el trabajo sin commit de `hud-fightcade` (o trabajar con índice temporal como en los commits anteriores), y anotar los hashes de `cps2recomp-launcher.exe`, `kinst.exe` y `kinst2.exe` instalados como referencia. Verificar con `git log -1 killer-instinct` que apunta a `1d4f181`.
  - Hecho 2026-10-27: `killer-instinct` en `1d4f181`. Los archivos que toca el change son iguales en la copia de trabajo y en `killer-instinct`: se trabaja en la copia y el commit va con índice temporal. Referencia (SHA-256, 16 primeros), iguales en `CPS2Recomp` y `CPS-LAUNCHER`: launcher `144bd76fce6c59e9`, `kinst` `5d73b2798f5a7fe7`, `kinst2` `70bd0aedaad268af`.

## 2. El túnel (`tunel/`)

- [x] 2.1 Pasar la prueba de `D:\CPS2\cps-tunel` a `tunel/` (Cargo.toml con `iroh = "1.4"`, `src/main.rs`) y agregar `tools/tunel.sh`, que compila con `cargo build --release` y copia `cps-tunel.exe` a la carpeta que se le dé (decisión 2). Verificar que `sh tools/tunel.sh build/collection` deja el exe y que `cps-tunel.exe` sin argumentos imprime el uso.
- [x] 2.2 Código de sala nuevo: versión + EndpointId + URL del relay, en base32, tolerante a espacios, saltos de línea y minúsculas (decisión 4). Verificar que el código mide 120 caracteres o menos, que uno partido en dos líneas y en minúsculas conecta, y que uno cortado a la mitad imprime `ERROR` con "el código no es válido" y termina.
- [x] 2.3 Líneas de estado `CODE`, `LISTEN`, `CONNECTED`, `PATH direct|relay`, `PING <ms>`, `CLOSED` y `ERROR` (decisión 5), con el ping de `rtt` del camino elegido y el tipo de camino de `paths()`. Verificar en una conexión local que `PING` trae un número y `PATH` dice `direct`.
- [x] 2.4 Límites de tamaño y fin de conexión: contar e informar los datagramas que no caben; keep-alive de 1 s y desconexión a los 8 s; `--parent <pid>` termina el túnel cuando ese proceso desaparece (decisiones 3 y 7). Verificar que al matar al anfitrión cada invitado imprime `CLOSED` en 10 s o menos, y que al matar el proceso padre el túnel termina.
- [x] 2.5 Varios invitados por sala (decisión 9): un socket UDP local por invitado, eventos numerados (`CONNECTED <n>`, `PING <n> <ms>`, `CLOSED <n>`), hasta 8 y "sala llena" para el noveno; al irse uno, la sala sigue. Verificar con un eco UDP de prueba en el puerto del juego y tres túneles invitados: cada uno recibe solo sus respuestas, el eco ve tres puertos de origen distintos, al cerrar uno los otros siguen, y con KI y un invitado de más la partida del primero sigue sin desync.
- [x] 2.6 Prueba sin ventana por el túnel: dos UltraCombo con `--net-host 7845` / `--net-join 127.0.0.1:<LISTEN>` y el mismo `--script` durante 2,400 cuadros. Verificar que los dos `ki1.log` dan el mismo `state hash` y que el túnel informa 0 paquetes demasiado grandes.
- [x] 2.7 Buscar cómo forzar el relay (opción de Iroh o `--relay-only` de prueba) y repetir 2.6 por relay. Anotar en `tunel/README.md` el ping medido y cómo se forzó; si no se puede forzar en una PC, anotarlo y dejar la medición para la prueba con un amigo (6.3).

## 3. Arrancar y detener procesos

- [x] 3.1 Agregar `GameProcess::stop()` en `recomp-common/launcher/src/game_process.{h,cpp}` (TerminateProcess en Windows, `kill` en POSIX) (decisión 7). Verificar que `running()` pasa a falso tras `stop()` y que el launcher compila con `tools/collection.sh`.
- [x] 3.2 Que el puente agregue a `BRIDGE_ARGS` lo que venga después de `--` (decisión 6), en `launcher/src/outside_bridge.cpp`. Verificar que `kinst.exe --rom <UltraCombo.exe> -- --host 7845` abre UltraCombo con `--game ki1 --host 7845` (línea de comandos del proceso) y que sin `--` todo sigue como antes.

## 4. La tarjeta ONLINE

- [x] 4.1 Agregar a `launcher::Description` el campo opcional `online` (puerto y argumentos de anfitrión/invitado) y declararlo en `description.cpp` para `kinst` y `kinst2` con el puerto 7845 (decisión 6). Verificar que 3rd Strike y los juegos de Capcom no lo tienen.
- [x] 4.2 Dibujar la tarjeta ONLINE en la página del juego (decisión 8): solo si el juego tiene `online` y `cps-tunel.exe` está junto al launcher; "Create room", campo "Room code" con "Paste", "Join", estado y "Copy". Verificar con `--screenshot --page kinst` que aparece en KI, y con `--page ssf2t` que no; y que sin `cps-tunel.exe` tampoco aparece en KI.
  - Probado con capturas: aparece en KI (`launA`), no aparece en KI sin `cps-tunel.exe` (`launB`). La página de ST no existe en el launcher de prueba (sin ROMs); que no tenga la tarjeta se comprueba con el launcher instalado en 6.2.
- [x] 4.3 Crear sala: arrancar el túnel con su log en `logs/tunel.log` y `--parent`, el juego con `-- --host <puerto>`, leer las líneas nuevas del log y mostrar el código con "Copy" (`SDL_SetClipboardText`) (decisiones 5, 7 y 8). Verificar con `--press` que tras "Create room" la tarjeta muestra un código, que "Copy" lo deja en el portapapeles y que UltraCombo corre con `--host 7845`.
- [x] 4.4 Unirse: validar el código, arrancar el túnel invitado y, con `LISTEN`, el juego con `-- --join 127.0.0.1:<puerto>`; un código inválido se muestra como error sin abrir el juego. Verificar con `--press` (pegar un código medio, luego uno bueno de un túnel anfitrión de prueba) el error y luego la línea de comandos de UltraCombo.
- [x] 4.5 Estado y vida de la sala: esperando, conectado directo o por relay con ping, error; mientras hay sala, Play dice "Playing online…" y queda desactivado, igual que la otra opción; "Close room" mientras el anfitrión espera y "Leave" ya conectados, que detienen juego y túnel con `stop()`; "Copy" cambia a "Copied"; al terminar el juego, `stop()` del túnel y la tarjeta vuelve al inicio; una sala a la vez en todo el launcher. Verificar con capturas en cada estado (sin sala, esperando, conectados, código inválido, como la maqueta de la decisión 8) que Play dice "Playing online…" y no responde al clic; que "Close room" y "Leave" (con `--press`) dejan sin procesos `UltraCombo.exe` ni `cps-tunel.exe` y la tarjeta vuelve a "Create room"/"Join"; que tras el "Leave" del invitado la tarjeta del anfitrión dice que el otro jugador se fue; y que al cerrar UltraCombo a mano también desaparece `cps-tunel.exe`.

## 5. Juego completo en la misma PC

- [x] 5.1 Con dos launchers de prueba (copias en el scratchpad, el invitado con otro puerto en su `ki1.ini`), crear sala en uno y unirse con el código en el otro, sin tocar el teclado mientras se manda `--press`. Verificar que los dos KI muestran "P1/P2 ping" y ningún desync en 60 s, con capturas de las dos ventanas.
- [x] 5.2 Repetir con KI2. Verificar igual que en 5.1.
  - Hecho 2026-10-27 con dos launchers de prueba (`launA`/`launB`): KI y KI2 conectan directo (1 ms en la tarjeta, 22–23 ms en el juego), juegan 60 s sin desync, "Copied", "Another game has a room open", "The other player left" y "Leave"/"Close room" sin procesos sueltos; cerrar KI con su X cierra el túnel. Los clics los hizo Claude con la ventana comprobada al frente, con el usuario grabando la pantalla.

## 6. Instalación, LEEME y paquete

- [x] 6.1 Agregar `tools/tunel.sh` a `tools/collection.sh` (si hay `cargo`; sin él, avisar y seguir) y documentar en `docs/BUILDING.md` cómo instalar Rust con rustup (MSVC) para el túnel. Verificar que `tools/collection.sh` deja `cps-tunel.exe` en `build/collection`.
- [x] 6.2 Instalar launcher, puentes y `cps-tunel.exe` en `D:\CPS2\CPS2Recomp` y `D:\CPS2\CPS-LAUNCHER` con respaldo en `_respaldo_online`, y en `CPS-LAUNCHER-compartir`. Agregar al LEEME la sección "JUGAR EN LÍNEA" (crear sala, copiar el código por WhatsApp, pegarlo y unirse; "Permitir" en el aviso del firewall; los dos con esta misma carpeta) y quitar de la sección de KI en línea lo de abrir el puerto 7845 como único camino. Verificar que el LEEME queda en UTF-8 con BOM y CRLF y que las capturas del launcher instalado muestran la tarjeta en KI.
- [ ] 6.3 Prueba con un amigo en otra red (la hace el usuario): crear sala, mandar el código por WhatsApp y jugar unos rounds. Verificar con el usuario que conecta sin abrir puertos y anotar si fue directo o relay y el ping.
- [x] 6.4 Rehacer el zip con la copia limpia (lista de archivos igual a la anterior más `cps-tunel.exe`, sin logs, saves ni rutas en `launcher.ini`), extraerlo en C: y probar 5.1 desde ahí. Verificar el tamaño del zip y el hash de `cps-tunel.exe` dentro.
  - Hecho 2026-10-27: `D:\CPS2\CPS-LAUNCHER.zip` (729 MB). Antes, `tools/tunel.sh` pasó a compilar con `--remap-path-prefix`: el exe guardaba 1,952 rutas con el usuario de Windows y `D:\CPS2`; ahora 0. Desde el zip extraído en C:, KI en línea por el túnel dio el mismo hash en los dos lados.

## Workflow follow-up

- Commit en `killer-instinct` cuando el usuario lo pida (Conventional Commits en español, con `-F`).
- Archivar el change con `/opsx:archive` cuando el usuario confirme la prueba con su amigo.
