# Proposal

## Why

Killer Instinct 1 y 2 ya traen netplay con rollback (UltraCombo, con el arreglo de `fix-winsock-join`), pero para jugar por internet el anfitrión tiene que abrir el puerto UDP 7845 en su módem o los dos instalar una VPN. Eso es demasiado para los amigos del usuario, que apenas saben arrancar Windows. El usuario quiere jugar KI en línea con ellos pronto, en su fork; no espera al issue #70 del autor.

Una prueba (spike) del 2026-10-27 ya llevó una partida de KI entre dos ventanas por un túnel con Iroh, sin desync y con 17–21 ms de ping, sin cambiar UltraCombo.

## What Changes

- **Túnel de red (`tunel/`, Rust con Iroh 1.4)**: un programa pequeño, `cps-tunel.exe`, que lleva los paquetes UDP de un juego por una conexión Iroh (QUIC con datagramas, directa o por los relays públicos de n0). Dos modos: `host` (imprime un código y entrega los paquetes al juego en `127.0.0.1:<puerto>`) y `join <código>` (abre un puerto local al que el juego se une). Informa su estado en líneas de texto que lee el launcher.
- **Código de sala largo, sin servidor**: el código es la dirección Iroh del anfitrión (su clave y su relay) en binario y base32, de unos 100 caracteres, para copiar y pegar por WhatsApp. No hay servidor propio.
- **Tarjeta ONLINE** en la página de Killer Instinct y Killer Instinct 2 del launcher:
  - **Create room**: arranca el túnel en modo anfitrión, muestra el código con un botón para copiarlo y abre el juego con `--host`.
  - **Join**: un campo para pegar el código; arranca el túnel en modo invitado y abre el juego con `--join 127.0.0.1:<puerto>`.
  - Muestra el estado: esperando, conectado (directo o por relay) y el ping.
  - Al cerrar el juego se cierra el túnel.
- **Puentes**: `kinst` y `kinst2` pasan al juego los argumentos extra que les da el launcher (`--host`, `--join`), además de los suyos.
- **Paquete y LEEME**: `cps-tunel.exe` va junto al launcher en `CPS-LAUNCHER` y en el zip; el LEEME explica cómo crear y unirse a una sala y qué hacer si Windows pide permiso del firewall.
- **Rama**: `killer-instinct`, después de `1d4f181`.

Fuera de alcance:
- **Rollback para los juegos de Capcom**: es el issue #70 del autor. El túnel servirá cuando esos juegos acepten `--host`/`--join` por UDP, pero aquí solo se conecta KI.
- **3rd Strike en línea**: el FBNeo oficial no trae netplay.
- **Código corto, salas públicas, lobby, espectadores y relay propio**: se podrán agregar después sin cambiar el túnel ni la tarjeta.
- **Aportarlo al autor**: es del fork del usuario.
- **macOS, Linux y Steam Deck**: el túnel compila ahí, pero se prueba y se empaqueta solo para Windows.

## Capabilities

### New Capabilities
- `online-tunnel`: el túnel de red: el código de sala, cómo lleva los paquetes UDP del juego por Iroh (directo o relay), los límites de tamaño y el protocolo de líneas de estado que lee el launcher.
- `online-card`: la tarjeta ONLINE del launcher para juegos con netplay propio: crear sala, copiar el código, unirse pegándolo, el estado de la conexión y cómo arranca y cierra el túnel junto con el juego.

### Modified Capabilities
<!-- Ninguna: openspec/specs está vacía (no hay specs archivadas). -->

## Impact

- **Nuevo**: `tunel/` (crate de Rust, a partir de la prueba en `D:\CPS2\cps-tunel`) y un script `tools/tunel.sh` que lo compila con `cargo`, como `tools/librashader.sh` del autor.
- **Launcher**: `recomp-common/launcher/src/app.cpp` y `app.h` (la tarjeta), `launcher/src/description.cpp` (qué juegos tienen netplay y con qué argumentos), `launcher/src/outside_bridge.cpp` (pasar argumentos extra) y `tools/collection.sh` (copiar `cps-tunel.exe` junto al launcher).
- **Dependencias**: Rust estable (instalado con rustup, MSVC) e Iroh 1.4 con sus crates; los relays públicos de n0, que sirven para pocos jugadores.
- **Distribución**: `CPS-LAUNCHER`, `CPS-LAUNCHER-compartir` y el zip (unos 13 MB más); los dos jugadores necesitan el mismo UltraCombo corregido, como ya pasa.
- **Riesgos**: latencia por relay sin medir entre redes reales; el aviso del firewall de Windows la primera vez.
