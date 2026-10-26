# Tasks

## 1. Rama al día con F12 encima

- [x] 1.1 `git fetch origin`, `git switch -c hsf2 origin/main` y `git cherry-pick d8c342f`, resolviendo choques a favor de lo del autor más lo de F12. Verificar con `git log --oneline -3` que el commit de F12 está sobre `81cfa99` (o el `main` del momento) y con `git status` limpio.
- [x] 1.2 Recompilar ssf2t en la rama (`make GAME=ssf2t`, más `explore` si el `main` nuevo lo pide) y repetir la prueba de F12 sin ventana (`--keys 2300:m` con `--audio-dump`, como en el change `f12-soundtrack`). Verificar que la placa vuelve a sonar tras la tecla y que la película de la ladder sin `m` da, en cada modo (soundtrack apagado y encendido), los mismos hashes que la línea base de `main` de ese modo. (Entre modos difieren aun en `main`: la intercepción cambia la memoria compartida, que entra en el hash.)

## 2. El juego en el toolkit

- [x] 2.1 Crear `games/hsf2/game.cfg` según la decisión 2 del diseño (`hsf2j`, padre `hsf2`, CRC observados, `code_end = 400000`, sin `explore_features`). Verificar con `python3 tools/gamecfg.py name hsf2` (y `program`, `graphics`, `key`) que el toolkit lo lee.
- [x] 2.2 Copiar `hsf2j.zip` y `hsf2.zip` de `D:\CPS2\CPS2Recomp-SuperTurbo\CPS2Recomp\ROM` a `rom/hsf2/`. Verificar que `git status` no los muestra (carpeta ignorada).
- [x] 2.3 `make GAME=hsf2` dos veces. Verificar que deja `work/hsf2/program.bin` y `build/hsf2/hsf2.exe`, y que una corrida corta sin ventana (`--headless --frames 300`) no da error de ROM.
- [x] 2.4 Correr `tools/portmap.py --from work/ssf2t --to work/hsf2 --limit 400000 --out work/hsf2/portmap.txt --entries work/hsf2/entries_desde_ssf2t.txt`, agregar las entradas movidas a `work/hsf2/entries.txt` (mismo formato, sin duplicados) y anotar el resumen del mapa: cuántas instrucciones y rutinas coinciden. Verificar que `entries.txt` creció y que `make GAME=hsf2` vuelve a compilar.
- [x] 2.5 `make GAME=hsf2 explore` en segundo plano con log. Verificar en el resumen de `recomp` las instrucciones traducidas, lo que queda al intérprete y los "data a guess took for code"; si estos últimos se concentran por encima de una dirección, bajar `code_end` a ese punto y repetir `make`.
- [x] 2.6 Escribir `games/hsf2/README.md` (qué set, cómo compilarlo, que es nivel 1 sin funciones propias) y agregar la fila del juego a la tabla del README principal. Verificar que `make audit` (o `tools/audit.sh`) pasa: nada del juego, sin llaves ni rutas personales.

## 3. Prueba sin ventana y medición

- [x] 3.1 Correr `build/hsf2/hsf2.exe --rom rom/hsf2 --headless --frames 3000 --auto 1 --shot-every 300:<scratchpad>/hsf2_ --stats`. Verificar en las capturas la intro y una pelea, y en `--stats` cuánto pasó por el intérprete.
- [x] 3.2 Comprobar el rechazo de sets: correr con solo `hsf2j.zip` (sin el padre) y verificar que el mensaje nombra el archivo que falta.
- [x] 3.3 Escribir en `games/hsf2/README.md` (sección de progreso) la medición de `portmap.py` y la recomendación sobre el nivel 2. Verificar que los números coinciden con `work/hsf2/portmap.txt`.

## 4. Instalación con respaldo

- [x] 4.1 Respaldar `D:\CPS2\CPS2Recomp\cps2recomp-launcher.exe` en `D:\CPS2\CPS2Recomp\_respaldo_hsf2\`. Verificar que el respaldo existe y pesa lo mismo que el original.
- [x] 4.2 `tools/collection.sh` y copiar a `D:\CPS2\CPS2Recomp` el `cps2recomp-launcher.exe` nuevo y `hsf2.exe`; copiar `hsf2j.zip` y `hsf2.zip` a `D:\CPS2\CPS2Recomp\ROM`. Verificar con el modo de capturas del launcher (`--screenshot`) que Hyper Street Fighter II aparece entre los juegos encontrados, y con `hsf2.exe --rom ROM --headless --frames 600` corriendo con un PATH mínimo, que no le faltan DLLs.
- [x] 4.3 Prueba del usuario con ventana: abrir HSF2 desde el launcher, llegar a una pelea y confirmar que se juega bien. Anotar lo que reporte.

## Workflow follow-up

- Hacer el commit en la rama `hsf2` cuando el usuario lo apruebe (Conventional Commits en español).
- Decidir con la medición si se abre el change del nivel 2 (`sources = ssf2t` + `family.h`).
- Archivar este change con `/opsx:archive` cuando la prueba del usuario esté hecha.
