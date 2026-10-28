# Proposal

## Why

El juego en línea de KOF '97 con GekkoNet (neogeo-recomp, change `neogeo-online`) funcionó muy bien en la primera reta
con un amigo: ping de 51 a 69 ms por wifi, rollbacks de un cuadro y sin desync. El usuario lo quiere ya para
**Super Turbo** y **SF2 Champion Edition** en su colección, con la misma tarjeta ONLINE del launcher y el mismo túnel. El
netplay de Tommy (#70) acaba de entrar a su `main` (2026-10-28), pero nuestra colección es del fork; por ahora usamos lo
que ya probamos, y más adelante se decide si se pasa al de upstream.

## What Changes

- **El port de CPS (`port/main.cpp`, `frontend_sdl.cpp`) juega en línea con GekkoNet**, igual que el de Neo Geo:
  - `--host [PUERTO]`, `--join HOST:PUERTO`, `--delay N` y `--stress`;
  - el adaptador UDP propio: el anfitrión aprende al otro del primer saludo, así sirve con `cps-tunel`;
  - el saludo compara el juego, sus ROMs, la versión y los ajustes que cambian la partida.
- **El estado que guarda la sesión** es el del save state: la máquina y las fuentes del juego (`TheGame().state`).
- **En línea no hay**:
  - trucos, save states, pausa, cámara lenta, run-ahead, modos ni el interruptor de prueba;
  - el soundtrack del CD (F12): cambia la memoria que comparten el juego y el sonido. Se oye la música de la placa.
- **El launcher**: la tarjeta ONLINE para Super Turbo y SF2 CE (y cualquier juego de CPS que pase la prueba de
  estrés).

Fuera de alcance:
- los demás juegos de Capcom (cada uno pasa su prueba de estrés antes);
- el soundtrack en línea;
- espectadores y repeticiones;
- el netplay y el lobby de Tommy.

## Capabilities

### New Capabilities
- `cps-netplay`: dos jugadores en línea en los juegos de CPS con rollback: anfitrión e invitado, qué se apaga, la
  revisión entre los dos y la prueba de estrés.

### Modified Capabilities
<!-- ninguna -->

## Impact

- `CMakeLists.txt`: GekkoNet (la misma versión fijada que neogeo-recomp, sin asio) y ws2_32 en Windows.
- Nuevo `port/online.{h,cpp}`, adaptado de `port/neogeo_online.cpp` de neogeo-recomp.
- `port/main.cpp`: opciones, el cuadro de la sesión y lo que se apaga en línea.
- `port/frontend_sdl.cpp`: la ventana en línea.
- `runtime/game.{h,cpp}`: los trucos apagados en línea.
- `launcher/src/description.cpp`: la tarjeta ONLINE de ST y CE.
- `THIRD_PARTY_NOTICES.md`.
