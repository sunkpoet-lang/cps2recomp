# Tasks

## 1. Rama y referencias

- [x] 1.1 `git switch -c ghouls sf2-sonido`. Verificar con `git log --oneline -1` que parte de `19fc2f8`.
- [x] 1.2 Sacar hashes de referencia antes de tocar nada: HSF2 (`--frames 6000 --auto 1`, igual que `h_mus.txt`) y SF2 CE (`--frames 6000 --auto 5:900`, con audio), guardados en el scratchpad.
- [x] 1.3 Copiar `ghouls.zip` de `D:\CPS2\CPS2Recomp\ROM` a `rom/ghouls/` (ignorado por Git). Verificar con `git status` que no aparece.

## 2. Formas de carga de ROMs

- [x] 2.1 `RomFile` con partes y marca de byte alto primero; `ParseFiles` con `+` y `/be` (decisiones 2 y 4). Verificar que las listas de todos los `game.cfg` existentes dan los mismos archivos que antes (un volcado de los nombres y CRC antes y después).
- [x] 2.2 `LoadRomSet`: el programa con pares de bytes y con palabras de byte alto primero. Verificar que el programa de Ghouls cargado coincide con el de MAME: el vector inicial (las primeras 8 bytes, pila y PC) es razonable y el PC cae dentro del programa.
- [x] 2.3 `LoadRomSet`: los grupos de ocho archivos de a byte en los gráficos (decisión 3), y el error si un grupo de ocho corta uno de cuatro. Verificar que los gráficos de Ghouls miden 3 MB y que HSF2 y SF2 CE dan los mismos hashes que en 1.2.
- [x] 2.4 La comprobación de archivos del juego y del launcher recorre las partes. Verificar con el launcher compilado que encuentra los juegos de antes y que, con un archivo de Ghouls renombrado, nombra el que falta.
- [x] 2.5 Lista de samples vacía en la placa clásica (decisión 8): que `ParseRomList`, `LoadRomSet` y el launcher la acepten.

## 3. Reloj, DIP switches y mapper de objetos

- [x] 3.1 `cpu_clock` en `game.cfg` → `Board` → ciclos por línea de `Machine` (decisión 1), y su paso por `tools/gamecfg.py` y `port/main.cpp`. Verificar que HSF2 da los mismos hashes que en 1.2.
- [x] 3.2 `dip` en `game.cfg` → `Board` → `ReadCps1` (decisión 6); quitar el `0xDFFF` fijo. Verificar que SF2 CE con `dip = FF FF DF` da los mismos hashes que en 1.2, y luego dejarle `FF FC 9F` (y a `sf2hf` también).
- [x] 3.3 Los objetos por el mapper en `DrawObjectsCps1` (decisión 5). Verificar que SF2 CE (con el `dip` viejo) y HSF2 dan los mismos hashes que en 1.2.

## 4. El juego

- [x] 4.1 `games/ghouls/game.cfg` (decisión 7, con las listas y los CRC de `ROM_START( ghouls )`), `README.md` y su fila en la tabla del README principal. Verificar con `tools/gamecfg.py` y `tools/audit.sh`.
- [x] 4.2 `make GAME=ghouls` dos veces y `make GAME=ghouls explore`. Verificar que arranca sin ventana y anotar las entradas encontradas y el porcentaje en el intérprete.
- [x] 4.3 Corrida de 6000 cuadros sin ventana con `--auto` y capturas: intro, inicio de partida, cementerio del primer nivel con Arthur y enemigos, y una muerte. Verificar en las capturas que los fondos, los objetos y el marcador tienen tiles correctos. Si el intérprete pasa del 2 %, sembrar entradas con una corrida más larga y repetir.
- [x] 4.4 Audio de la misma corrida: música y efectos, ninguna muestra recortada; y el mismo audio byte por byte con y sin `run_ahead`.
- [x] 4.5 Probar los dos botones: una película corta de entradas con disparo y salto en el primer nivel, y capturas que muestren el arma y el salto.

## 5. Documentación e instalación

- [x] 5.1 README de `ghouls` (placa, 10 MHz, DM620, sin voces, solo en el fork por ymfm) y notas en los README de `sf2ce`/`sf2hf` sobre su `dip` de fábrica. Verificar que `tools/audit.sh` pasa.
- [x] 5.2 Respaldar en `D:\CPS2\CPS2Recomp\_respaldo_ghouls` los exes y el launcher que se reemplacen; instalar `ghouls.exe`, `sf2ce.exe`, `sf2hf.exe` y el launcher. Verificar que arrancan con un PATH mínimo.
- [x] 5.3 Prueba del usuario con ventana: Ghouls (imagen, controles, música) y SF2 (que la dificultad normal y el continuar se sientan bien). Anotar lo que reporte.
  - 2026-10-26: el usuario reporta que Ghouls y SF2 funcionan bien con ventana.

## Workflow follow-up

- Commit en la rama `ghouls` cuando el usuario lo apruebe; sin push sin su OK. La rama depende de ymfm, así que no va tal cual en un PR al autor.
