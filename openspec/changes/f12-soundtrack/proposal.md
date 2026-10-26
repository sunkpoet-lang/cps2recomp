# Proposal

## Why

El port de Super Turbo ya puede tocar un soundtrack propio en lugar de la música de la placa, pero cambiar entre uno y otro solo se puede desde el menú, y el cambio no se oye hasta la siguiente canción que pida el juego. Si se apaga a media pelea, hay silencio hasta el siguiente round. El usuario tiene el soundtrack arreglado del CD de PC de Gametek (44 pistas ya identificadas por huella de audio) y quiere alternar al instante entre las dos músicas con una sola tecla (F12), con un aviso en pantalla, para compararlas y mostrarlo.

## What Changes

- **F12 alterna la música en vivo:** cambia la feature `custom_soundtrack` entre apagada y su último valor encendido, igual que el menú, pero el cambio se oye en ese mismo instante:
  - al encender, la placa calla su música y el archivo arranca en el punto de la canción donde va el juego;
  - al apagar, la placa vuelve a tocar desde el principio la última canción que pidió el juego.
- **Aviso en pantalla:** al presionar F12 aparece un letrero breve (unos 2 s) con la música activa: "Musica: Arreglada" o "Musica: Arcade" (sin acento: la fuente del juego es solo ASCII).
- **El juego no cambia mientras no se toque F12:** el reenvío de canciones a la placa solo ocurre al presionar la tecla, nunca en el cuadro a cuadro normal. Sin F12, las corridas con el mismo input dan los mismos hashes por cuadro que hoy.
- **Inicio por canción en `soundtrack.ini`:** una clave nueva, `start`, hace que un archivo empiece en un punto distinto de 0. Así una sola pista del CD sirve para dos canciones: la versión normal (`ryu`) y la acelerada que el juego toca con poca vida (`ryu_later`).
- **Tecla de prueba:** `--keys TICK:m` alterna la música como F12, para probarlo sin ventana.
- **Configuración personal (fuera de la repo):** un `soundtrack.ini` en la carpeta del soundtrack del usuario con la tabla de las 44 pistas y los cortes medidos. No se incluye en la repo ni se comparte música.

## Capabilities

### New Capabilities
- `soundtrack-live-toggle`: alternar en vivo entre el soundtrack propio y la música de la placa (tecla, cambio inmediato, aviso en pantalla, sin cambiar la partida cuando no se usa).
- `soundtrack-song-start`: el punto de inicio de un archivo por canción en `soundtrack.ini`, para usar partes de una misma pista en canciones distintas.

### Modified Capabilities
<!-- La repo no tenía specs de OpenSpec hasta ahora: no hay capacidades existentes que modificar. -->

## Impact

- **Código del toolkit (sirve a todos los juegos con soundtrack, no solo a ssf2t):**
  - `machine/soundtrack.h`, `machine/soundtrack.cpp`: recordar la última música que pidió el juego, y una forma de mandarle a la placa una petición o el comando "para la música".
  - `machine/machine.h`, `machine/machine.cpp`: escribir una petición en la memoria compartida con la parte de sonido cuando esté libre.
  - `port/main.cpp`, `port/port.h`: la acción de alternar, el aviso con su duración y la tecla de prueba `m`; la versión de los save states sube de 7 a 8.
  - `port/frontend_sdl.cpp`: F12.
  - `port/soundtrack.h`, `port/soundtrack.cpp`: la clave `start`.
  - Dibujo del aviso junto al HUD existente (`port/hud.cpp` o donde se compone la imagen).
- **Documentación:** README (sección del custom soundtrack: F12 y `start`) y el comentario de cabecera de `port/soundtrack.h`.
- **Save states:** los guardados con la versión anterior siguen cargando (los campos nuevos toman valores por defecto).
- **Fuera de la repo:** respaldo del `ssf2t.exe` que funciona hoy antes de reemplazarlo en `D:\CPS2\CPS2Recomp`; el `soundtrack.ini` personal vive en la carpeta de la música del usuario.
- **Netplay (issue #70 del autor):** el custom soundtrack ya está fuera de la suma de verificación online; el reenvío a la placa debe apagarse durante una partida en línea cuando eso exista.
