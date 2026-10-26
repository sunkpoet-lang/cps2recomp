# Tasks

## 1. Base

- [x] 1.1 Confirmar que `dip-menu` está en un commit de la rama `ghouls` (con el OK del usuario) y sacar hashes de referencia de SF2 CE, Ghouls y HSF2 (las corridas de `dip-menu`).

## 2. El mecanismo

- [x] 2.1 Leer y validar `cheat_1`...`cheat_16` (decisiones 1 y 2), con su condición opcional. Verificar con `--list-features` en un juego de prueba que salen los trucos con sus opciones, y que un truco roto (dirección fuera de la RAM, palabra en dirección impar) impide arrancar con un mensaje que lo nombra.
- [x] 2.2 Registrar la página Cheats y sus trucos con `kFeatureOwn` (decisión 3), sin duplicar la página si el juego ya tiene una.
- [x] 2.3 Aplicar los trucos antes de cada cuadro y en el run-ahead (decisión 4). Verificar que, con todo apagado, los hashes de SF2 CE, Ghouls y HSF2 son los de 1.1 y que HSF2 no gana la página.

## 3. Trucos de SF2 CE y Hyper Fighting

- [x] 3.1 Encontrar en CE (decisión 5) la energía de cada jugador, el reloj del round, el personaje de cada jugador y el paso del juego que dice "selección". Anotar cómo se encontró cada una.
- [x] 3.2 Declarar en `games/sf2ce/game.cfg`: energía infinita P1 y P2, tiempo infinito, P2 sin energía (un golpe gana) y jugar con los jefes (condición: solo en la selección). Verificar sin ventana cada uno con `--features` y capturas; y la misma película con un truco encendido da el mismo resultado con y sin run-ahead.
- [x] 3.3 Probar las mismas direcciones en Hyper Fighting y declarar sus trucos (con las suyas donde cambien). Verificar igual que en 3.2.

## 4. Trucos de Ghouls'n Ghosts

- [x] 4.1 Encontrar las vidas, el reloj, la armadura, el arma y, si existe, el contador de invencibilidad.
- [x] 4.2 Declarar en `games/ghouls/game.cfg`: vidas infinitas, tiempo infinito, armadura (normal o dorada), elegir arma e invencible (si salió en 4.1). Verificar cada uno sin ventana con capturas; anotar los que no se pudieron.

## 5. Documentación, launcher e instalación

- [x] 5.1 README de los tres juegos (la página Cheats y sus trucos) y comentario en `game.cfg` de cómo se encontró cada dirección. Verificar que `tools/audit.sh` pasa.
- [x] 5.2 Reconstruir los tres exes y el launcher; respaldar en `D:\CPS2\CPS2Recomp\_respaldo_cheats` lo que se reemplace e instalar. Verificar que arrancan con un PATH mínimo y que `--list-features` muestra los trucos.
- [x] 5.3 Agregar a la skill `cps2recomp` la regla: todo juego nuevo trae su página Cheats (cómo se declaran los trucos y cómo se buscan las direcciones).
- [x] 5.4 Prueba del usuario con ventana: prender y apagar trucos en vivo desde F1 y desde el launcher en los tres juegos. Anotar lo que reporte.
  - 2026-10-26: el usuario reporta que los trucos funcionan bien en los tres juegos.

## Workflow follow-up

- Commit en la rama `ghouls` cuando el usuario lo apruebe; sin push sin su OK.
