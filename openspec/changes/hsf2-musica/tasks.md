# Tasks

## 1. Fuentes de la música

- [x] 1.1 Crear `games/hsf2/src/game.cpp` (`RegisterGame()` que llama a `RegisterMusic()`) y `games/hsf2/src/music.cpp` (`songTable = 0x9006`, `songCount = 0x280`, nombres de Super Turbo con los mismos números, más `continue` 0x37 y `game_over` 0x39), con licencia y comentarios al estilo del de Super Turbo. Verificar con `make GAME=hsf2` que compila y con `--headless --frames 300 --features custom_soundtrack --soundtrack <carpeta de prueba>` que escribe un `songs.txt` con 68 piezas.
- [x] 1.2 Comparar hashes: la misma corrida `--auto 1 --frames 6000 --hash-log` con el `hsf2.exe` del nivel 1 (el instalado) y con el nuevo, soundtrack apagado. Verificar que son idénticos.

## 2. Confirmar las piezas en HSF2

- [x] 2.1 Correr sin ventana con juego inventado (`--auto`, varias semillas, unos 20 000 cuadros) con `CPS2RECOMP_SOUNDTRACK_LOG=1`, el soundtrack encendido y capturas en los cuadros de cada petición de música. Verificar, pantalla por pantalla (título, selección, versus, escenarios y "later", ganador, continue, game over), que el número coincide con el nombre; renombrar o anotar lo que no.
- [x] 2.2 Anotar en `games/hsf2/README.md` qué piezas quedaron confirmadas y cuáles no se alcanzaron (por ejemplo, finales y créditos). Verificar que la lista coincide con el log.

## 3. Soundtrack del CD y F12

- [x] 3.1 Crear `D:\CPS2\CPS2Recomp\AST\HSF2\soundtrack.ini` a partir del de `SSF2Turbo`, con cada `file` cambiado a `..\SSF2Turbo\<archivo>` y los mismos `loop`/`start` (ajustando secciones si la 2.1 renombró piezas). Correr con `--soundtrack D:/CPS2/CPS2Recomp/AST/HSF2` y el log, y verificar que las piezas salen como "music (file)", que el juego escribió `songs.txt` en `AST\HSF2` y que `AST\SSF2Turbo` no cambió.
- [x] 3.2 Probar F12 sin ventana: `--keys <cuadro a media pelea>:m --audio-dump` con el soundtrack encendido; verificar que la placa vuelve a sonar tras la tecla (nivel de audio como con la placa sola) y lo contrario con el soundtrack apagado.

## 4. Instalación y prueba del usuario

- [x] 4.1 Respaldar `D:\CPS2\CPS2Recomp\hsf2.exe` (nivel 1) en `_respaldo_hsf2\` y copiar el nuevo. Verificar que el respaldo existe y que el nuevo arranca con un PATH mínimo.
- [x] 4.2 Prueba del usuario con ventana: en la página de HSF2 del launcher, elegir `AST\HSF2` y encender el soundtrack; jugar contra varios escenarios y alternar con F12. Anotar lo que reporte.

## Workflow follow-up

- Commit en la rama `hsf2` cuando el usuario lo apruebe.
- Archivar `hsf2-nivel-1` y `hsf2-musica` con `/opsx:archive` cuando corresponda.
