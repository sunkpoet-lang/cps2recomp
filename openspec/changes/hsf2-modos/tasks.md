# Tasks

## 1. Rama y referencias

- [x] 1.1 `git switch -c hsf2-modos ghouls` y verificar que parte de `e53dfa4`. Sacar referencias: Super Turbo (`--frames 6000 --auto 1`, hashes y audio, y una corrida con `free_play` y una sesión de entrenamiento por `--features`) y HSF2 (`h_mus.txt`).
- [x] 1.2 Revisar que `work/ssf2t` existe y está explorado (si no, `make GAME=ssf2t explore`), porque `places.py` lo necesita.

## 2. Super Turbo con At() y el toolkit

- [x] 2.1 `games/ssf2t/src/places.h` y `games/ssf2t/family.h` (lista vacía); cada dirección de `ssf2t.h` y de los `.cpp` con `At()`/`AtA5()`, y los ganchos con `HookAt(At(...))` (decisión 1). Verificar que Super Turbo da los mismos hashes y el mismo audio que en 1.1 (las tres corridas).
- [x] 2.2 `CMakeLists.txt`: el archivo propio reemplaza al compartido del mismo nombre, y `sources_leave_out` (decisión 2). Verificar que Vampire Savior 2 compila igual y que Super Turbo sigue igual.

## 3. HSF2 compila las fuentes de Super Turbo

- [x] 3.1 Medir con `--trace` la máquina de pasos de HSF2 en una partida (título, selección de versión, selección, versus, round, fin), pasar las constantes de pasos a `family.h` (decisión 5) y verificar que Super Turbo sigue igual.
- [x] 3.2 `places.py` para `games/hsf2/family.h`; anotar cuántas encontró y cuáles faltan.
- [x] 3.3 Buscar a mano las que falten (decisión 4), primero las de entrenamiento, luego las de modos, cheats y board, cada una con cómo se encontró (`// by hand`).
- [x] 3.4 `game.cfg` de HSF2 con `sources = ssf2t` y `sources_leave_out`; su `game.cpp` y `left_out.cpp` (decisión 3). Verificar que compila, y que con todo apagado da los mismos hashes que `h_mus.txt`.

## 4. Los modos en HSF2

- [x] 4.1 Modo de entrenamiento: una sesión sin ventana con hitboxes, frame meter, input display y readouts; golpes al dummy. Verificar con capturas y que la energía del dummy se rellena.
- [x] 4.2 Dummy: parado, agachado, salta, bloquea, graba y repite, y responde. Verificar cada uno con una película.
- [ ] 4.3 Versión de cada luchador: encontrar el byte, y la opción "Version" en los modos que arman una pelea (decisión 5). Verificar que una batalla infinita con Ryu de World Warrior y Ryu de Super Turbo los muestra distintos.
  - Pendiente (sin marcar): los modos ponen la versión en 0, Super X (la de Super Turbo), para todos. Una opción "Version" por luchador pide cambiar las listas de luchadores del motor común (fight/battle), más que una escritura (decisión 5): se anota y queda para otro change. El byte (+32A) ya está en family.h.
- [x] 4.4 Batalla infinita, survival y ring out: verificar sin ventana que cada uno arma su pelea y sigue como debe (batalla sin fin, survival con su récord, ring out).
- [x] 4.5 Modo demo (sus partes) y visor de escenarios: capturas de cada parte y de dos escenarios.
- [x] 4.6 Free play y "directo al título": verificar con capturas.
- [x] 4.7 Run-ahead: la misma película en una sesión de entrenamiento da los mismos hashes con y sin run-ahead.

## 5. Board, Cheats y documentación

- [x] 5.1 Página Board (decisión 6): verificar cada opción sin ventana (la dificultad o la velocidad llegan a la EEPROM o a la RAM que el juego lee).
- [x] 5.2 Página Cheats (decisión 6): verificar sin daño, un golpe gana y tiempo infinito con películas; usar `cheat_N` para los que no tengan gancho.
- [x] 5.3 README de HSF2 (los modos, qué viene de Super Turbo, qué falta) y nota en el de Super Turbo sobre las fuentes compartidas. Verificar que `tools/audit.sh` pasa.

## 6. Launcher e instalación

- [x] 6.1 Reconstruir `hsf2.exe`, `ssf2t.exe` y el launcher; respaldar en `D:\CPS2\CPS2Recomp\_respaldo_hsf2_modos` e instalar. Verificar que arrancan con un PATH mínimo y que el soundtrack con F12 de los dos sigue funcionando.
- [ ] 6.2 Prueba del usuario con ventana: entrenamiento, dummy, batalla infinita, survival, ring out, Board y Cheats en HSF2, y que Super Turbo sigue igual. Anotar lo que reporte.

## Workflow follow-up

- Commit en la rama `hsf2-modos` cuando el usuario lo apruebe; sin push sin su OK.
- Siguiente: el mismo trabajo para SF2 CE y Hyper Fighting (otro change).
