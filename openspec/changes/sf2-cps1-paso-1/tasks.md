# Tasks

## 1. Rama y placa CPS-1 clásica en el toolkit

- [x] 1.1 `git switch -c sf2-cps1 hsf2`. Verificar con `git log --oneline -1` que parte de `a35f09b` y con `git status` limpio (salvo logs y este change).
- [x] 1.2 `machine/romset.h`/`.cpp`: `Board.qsound` (true por defecto) y `board = cps1-classic` (`cps1 = true`, `qsound = false`). En esa placa: sin Kabuki (`soundOpcodes` = el programa tal cual) y sin el reordenado de samples de QSound. Comentarios al estilo del archivo. Verificar que compila (`make GAME=hsf2`) y que `games/dino/game.cfg` sigue diciendo `board = cps1`.
- [x] 1.3 `machine/machine.cpp`: en la placa clásica, el Z80 detenido en `Reset` y las escrituras a `0x800180`–`0x80018F` aceptadas sin efecto ni `Unmapped`; en `ReadCps1`, `0x800176` con las patadas (decisión 3 del diseño), antes de la regla del CPS-B. Verificar que compila y revisar con `git diff` que toda línea nueva está condicionada a `!qsound`.
- [x] 1.4 Comprobar que el camino de siempre no cambió: `make example` y `make smoke` (o `tools/ci_smoke.sh`) pasan, y la corrida de 6000 cuadros de HSF2 da los mismos hashes que antes (`h_mus.txt` del change hsf2-musica).

- [x] 1.5 Mapper de gráficos del CPS-1 (decisión 7 del diseño): `Board` con los tamaños de banco y los rangos, su lectura de `gfx_map`, el paso de `GAME_GFX_MAP` en `port/main.cpp` y la traducción en `DrawScroll` solo cuando hay tabla. Verificar que compila, que HSF2 da los mismos hashes que antes y que `git diff` no cambia nada del camino sin tabla.

## 2. Street Fighter II: Champion Edition

- [x] 2.1 `games/sf2ce/game.cfg` con `board = cps1-classic`, `cps_b = 26 28 2A 2C 2E 30 02 04 08`, el set `sf2ce` (padre `sf2ce`), los CRC del diseño, `code_end = 180000`, `players = 2`. Verificar con `tools/gamecfg.py` que se lee y copiar `sf2ce.zip` a `rom/sf2ce/` (ignorado por git).
- [x] 2.2 `make GAME=sf2ce` dos veces y una corrida corta sin ventana (`--headless --frames 600 --shot-every 150`). Verificar que carga (sin error de Kabuki), que las capturas muestran la intro y que no hay `Unmapped` repetido cada cuadro; si el juego se bloquea o lee un ID del CPS-B, resolverlo según el diseño (riesgos) o parar y anotar.
- [x] 2.3 `make GAME=sf2ce explore` en segundo plano con log. Verificar el resumen de `recomp` (instrucciones, lo que queda al intérprete) y ajustar `code_end` si los "data a guess took for code" se juntan arriba.
- [x] 2.4 Corrida sin ventana de 6000 cuadros con `--auto 1 --shot-every 500 --stats --audio-dump`. Verificar en las capturas la intro, la selección y una pelea, el porcentaje en el intérprete y que el audio es silencio.
- [x] 2.5 Probar las patadas: una corrida con `--movie` de una película corta que mantenga cada patada del jugador 1 en una pelea (o `--poke`/capturas si es más simple), y verificar en las capturas que el personaje patea con la ligera, la media y la fuerte. Corregir el orden de bits si no coincide.
- [x] 2.6 `games/sf2ce/README.md` (qué set, cómo compilarlo, que es paso 1 en silencio, lo que se vio) y su fila en la tabla del README principal. Verificar que `tools/audit.sh` pasa.

## 3. Street Fighter II′: Hyper Fighting

- [x] 3.1 `games/sf2hf/game.cfg` como el de CE con el set `sf2hf` y su programa (`s2te_23.8f`, `s2te_22.7f`, `s2te_21.6f`). Copiar `sf2hf.zip` a `rom/sf2hf/`, `make` dos veces y corrida corta. Verificar que carga y muestra la intro.
- [x] 3.2 Probar `portmap.py --from work/sf2ce --to work/sf2hf --limit 180000 --entries ...`; si ubica una parte grande, sembrar `work/sf2hf/entries.txt` y anotar la medida. Luego `make GAME=sf2hf explore` con log y verificar el resumen.
- [x] 3.3 Corrida sin ventana de 6000 cuadros como la 2.4 y prueba rápida de una patada. Verificar intro, selección, pelea y silencio.
- [x] 3.4 `games/sf2hf/README.md` y su fila en la tabla. Verificar que `tools/audit.sh` pasa.

## 4. Instalación y prueba del usuario

- [x] 4.1 Respaldar `D:\CPS2\CPS2Recomp\cps2recomp-launcher.exe` en `_respaldo_sf2\`; `tools/collection.sh`; copiar el launcher, `sf2ce.exe` y `sf2hf.exe`. Verificar con el modo de capturas del launcher que los dos juegos aparecen con Play, y que cada exe arranca con un PATH mínimo.
- [x] 4.2 Prueba del usuario con ventana: jugar CE y Hyper Fighting con los seis botones. Anotar lo que reporte.

## Workflow follow-up

- Commit en la rama `sf2-cps1` cuando el usuario lo apruebe.
- Paso 2 (sonido: YM2151 y OKI MSM6295, y el latch de música) como change aparte.
