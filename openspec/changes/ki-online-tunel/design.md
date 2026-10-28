# Design

## Context

Ver proposal.md (Why). Lo que ya existe y condiciona el diseño:

- **UltraCombo** (con el arreglo `e78020b` de `fix-winsock-join`) juega en línea con `--host [PUERTO]` y `--join HOST[:PUERTO]`. Todo su netplay va por UDP con reintentos y sin suponer orden. El paquete más grande es un bloque de los ajustes del disco de 1,030 bytes; los de entradas miden menos de 450. El anfitrión acepta al primero que lo saluda y guarda su dirección.
- **Los puentes** `kinst` y `kinst2` (`launcher/src/outside_bridge.cpp`) reciben `--rom <UltraCombo.exe>` y lo abren con `BRIDGE_ARGS` (`--game ki1`); hoy descartan cualquier otro argumento.
- **El launcher** (`recomp-common/launcher`) arranca cada juego con `GameProcess::start(programa, args, carpeta, error, log)`, que manda su salida a un archivo y avisa cuando termina. No tiene forma de detenerlo. Cada juego se describe con un `launcher::Description` (`arguments`, `find_nearby`, etc.) que da `description.cpp`.
- **La prueba** de `D:\CPS2\cps-tunel` (Iroh 1.4.1, Rust 1.99.1 con MSVC) llevó KI entre dos ventanas sin desync: el mismo hash a los 2,400 cuadros y 17–21 ms de ping. Iroh permitió datagramas de 1,162 a 1,414 bytes. Su código medía 520 caracteres (JSON con todas las IPs) y la lectura del ping no funcionaba.
- **El autor** ya compila Rust para los shaders (`tools/librashader.sh` con `cargo`).

## Goals / Non-Goals

**Goals:**
- Que el túnel no sepa nada de KI: solo lleva datagramas entre un puerto UDP local y la otra PC, para que sirva a cualquier juego con netplay por UDP.
- Que el launcher no cambie la forma de arrancar los juegos: el modo en línea son los mismos argumentos más los de netplay.
- Que UltraCombo no cambie más.
- Que el túnel ya acepte varios invitados por sala, para que los espectadores de los juegos con GekkoNet (change `capcom-online`) no obliguen a rehacerlo.

**Non-Goals:**
- Un protocolo propio de netplay, sesiones o verificación de versión: los resuelve el juego (UltraCombo ya compara versión, ROM y disco).
- Espectadores en la tarjeta ONLINE de KI, salas públicas o lobby: UltraCombo no tiene espectadores y la tarjeta maneja un solo rival.
- Cifrado o autenticación propios: Iroh ya cifra la conexión con la clave del anfitrión que va en el código.

## Decisions

### 1. El túnel es un programa aparte, no una biblioteca dentro del launcher
`cps-tunel.exe` corre como proceso hijo del launcher, igual que los juegos.
- **Por qué**: el launcher es C++ del autor y el túnel es Rust. Como proceso aparte no hay que enlazar Rust con C++, y si el túnel falla no tumba al launcher. Además se puede probar solo desde la línea de comandos, como en la prueba.
- **Alternativa descartada**: una biblioteca Rust con interfaz en C dentro del launcher (como librashader). Es más integrada, pero obliga a compilar Rust dentro del build de CMake del launcher y a manejar el runtime asíncrono de Rust dentro de un programa ImGui. Queda para después si hace falta.
- **Alternativa descartada**: meterlo dentro de UltraCombo (como Ember dentro de SF4). Alejaría UltraCombo del autor y no serviría para otros juegos.

### 2. El código va en `tunel/` dentro de la repo, compilado con `tools/tunel.sh`
El crate de la prueba pasa a `tunel/` (Cargo.toml, `src/main.rs`). `tools/tunel.sh` corre `cargo build --release` y copia `cps-tunel.exe` a la carpeta de la colección; `tools/collection.sh` lo llama si `cargo` está disponible, y sin él la colección se arma sin túnel (la tarjeta no aparece).
- **Por qué**: el change solo puede tocar la repo, y así sigue el patrón de `tools/librashader.sh`. Rust se usa con su toolchain MSVC, que ya compila aquí con las Build Tools 2019; no pasa por MSYS2.

### 3. Datagramas QUIC de Iroh, con conexión directa o relay de n0
Cada paquete UDP del juego es un datagrama (`send_datagram` / `read_datagram`): no confiable y sin orden, como UDP, así que el rollback de UltraCombo se comporta igual que sin túnel. Se usan el preset `N0` y sus relays públicos; Iroh busca el camino directo y cambia a él cuando lo logra.
- **Alternativa descartada**: un stream confiable de QUIC. Agrega retransmisiones y bloqueo en cabeza de fila, que es justo lo que el rollback no quiere.
- **Tamaño**: antes de mandar, el túnel compara con `max_datagram_size()`. Uno que no cabe se cuenta y se informa (requisito de la spec), sin partirlo; con 1,030 bytes contra los más de 1,100 que da Iroh no debería pasar. Si una red real lo provoca, una tarea de seguimiento lo mandará por un stream.
- **Tiempo de espera**: keep-alive de 1 s y desconexión a los 8 s sin respuesta (se nota antes de 10 s), para que "el rival se fue" se note pronto. Con 10 s la prueba de la tarea 2.4 tardaba 11 s.

### 4. El código de sala: identidad + relay en binario, base32
El código es `versión (1 byte) + EndpointId (32 bytes) + URL del relay del anfitrión`, en base32 sin relleno. Mide unos 100–110 caracteres, en vez de los 520 del JSON con todas las IPs. Al decodificar se quitan espacios y saltos de línea y se pasa a mayúsculas.
- **Por qué sin las IPs directas**: con la identidad y el relay, Iroh se conecta por el relay y desde ahí descubre y prueba el camino directo. Las IPs en el código no ganan nada y lo alargan, además de mostrar la IP de casa del anfitrión.
- **El byte de versión** permite cambiar el formato después, por ejemplo a un código corto con servidor, sin confundir códigos viejos.

### 5. Protocolo de estado: una línea por evento, leída del log
El túnel imprime en stdout `CODE <código>`, `LISTEN <puerto>`, `CONNECTED`, `PATH direct|relay`, `PING <ms>`, `CLOSED` y `ERROR <mensaje>`; en el anfitrión, los de cada invitado llevan su número después de la palabra clave (`CONNECTED 1`, `PING 1 18`, `CLOSED 2`; decisión 9). El launcher lo arranca con `GameProcess::start` y su log en `<carpeta del juego>/logs/tunel.log`, y lee las líneas nuevas de ese archivo en cada cuadro de la interfaz (es pequeño).
- **Por qué**: `GameProcess` ya manda la salida a un archivo. Leerlo evita agregar tuberías y hilos al launcher.
- **El ping** sale de `Connection::rtt` del camino elegido y el tipo de camino de `paths()`, no del `Debug` de las estadísticas como en la prueba.

### 6. Argumentos del juego: lo normal + `--` + los de netplay; el puente los pasa
`Description` gana un campo opcional `online` con el puerto de netplay y una función que da los argumentos de anfitrión o invitado. Si está y existe el túnel, aparece la tarjeta. Al jugar en línea, el launcher arranca el juego con sus argumentos de siempre, luego `--`, y luego los de netplay (`--host 7845` o `--join 127.0.0.1:<puerto>`). El puente agrega lo que va después de `--` a `BRIDGE_ARGS`.
- **Por qué `--`**: es la convención que el launcher ya usa en `--play ID -- ARGS`, y no choca con `--rom`.
- En `description.cpp`, `kinst` y `kinst2` declaran `online` con el puerto 7845; 3rd Strike no.

### 7. Vida del túnel
- **Anfitrión**: el launcher arranca el túnel y el juego juntos. El juego espera a un jugador por su cuenta, y el código aparece en la tarjeta en cuanto el túnel lo imprime (1–3 s, lo que tarda en registrarse en el relay).
- **Invitado**: el launcher arranca el túnel y, cuando imprime `LISTEN`, el juego con `--join 127.0.0.1:<puerto>`. Los reintentos del juego cubren el tiempo hasta que el túnel conecta.
- **Al terminar el juego**, el launcher detiene el túnel. Para eso `GameProcess` gana `stop()` (TerminateProcess en Windows, `kill` en POSIX).
- **Si el launcher se cierra o se cae**, el túnel recibe `--parent <pid>` y termina solo cuando ese proceso ya no existe, para no dejar huérfanos ocupando puertos.

### 8. La tarjeta
En la página del juego, debajo de Play y como las tarjetas de Steam y DESKTOP:
- **Sin sala**: "Create room" y un campo "Room code" con "Paste" (lee el portapapeles con SDL, para quien no sabe Ctrl+V) y "Join".
- **Con sala**: el estado en una línea ("Waiting for a player", "Connected (direct), 18 ms", "Connected through a relay, 64 ms", el error si hay) y, si es anfitrión, el código con "Copy" (`SDL_SetClipboardText`), que cambia a "Copied" al usarlo.
- **Salir**: "Close room" mientras el anfitrión espera y "Leave" una vez conectados; los dos detienen el juego y el túnel con `GameProcess::stop()` y vuelven al inicio de la tarjeta.
- Mientras hay sala, Play dice "Playing online…" y queda desactivado, igual que la otra opción. Un código incompleto pinta el campo en rojo con "That code isn't complete. Copy it again from the message and paste it whole." En inglés, como el resto del launcher; el LEEME lo explica en español.
- La maqueta aprobada por el usuario (2026-10-27) tiene estos cuatro estados: sin sala, anfitrión esperando, conectados y código inválido. La GUI se puede ajustar después sin cambiar los requisitos.

### 9. El anfitrión acepta varios invitados, cada uno con su puerto local
El túnel anfitrión acepta conexiones mientras dure la sala, hasta 8. Por cada invitado abre un socket UDP propio en `127.0.0.1` (puerto efímero) que manda al puerto del juego, así que el juego ve a cada uno como una dirección distinta, fija mientras esté conectado. Lo que el juego responde a esa dirección vuelve solo a ese invitado. Al irse un invitado se cierra su socket y se informa `CLOSED <n>`; la sala sigue.
- **Por qué ahora**: GekkoNet (change `capcom-online`) recibe a sus espectadores como pares más en su puerto UDP. Si el túnel ya los separa por puerto local, los juegos de Capcom no necesitan otro túnel.
- **Para KI no cambia nada**: UltraCombo atiende al primero que lo saluda e ignora los paquetes de otras direcciones. La tarjeta muestra el estado del primer invitado y, si se va, "The other player left".
- **Alternativa descartada**: un solo socket local para todos con un encabezado que diga de quién es cada paquete. Obligaría a cambiar cada juego, y el objetivo es que el túnel sea transparente.
- **Límite de 8**: suficiente para dos jugadores y espectadores de un grupo de amigos; un noveno recibe "sala llena".

## Risks / Trade-offs

- [Latencia por relay sin medir] → La prueba fue directa en la misma PC. Se mide forzando el relay (pregunta abierta) y con un amigo en otra red antes de dar el paquete. Con relay en EE. UU. desde México se esperan unos 60–100 ms de ping, que el retraso de entrada 3–4 de UltraCombo absorbe.
- [Relays públicos de n0] → Son para desarrollo y pocos usuarios; para un grupo de amigos alcanza. Si se satura, Iroh publica su servidor de relay para correr uno propio, sin cambiar el código de sala (el relay va dentro del código).
- [Aviso del firewall de Windows] → La primera vez, Windows pregunta por `cps-tunel.exe`. El LEEME dice que se dé "Permitir". Si se niega, el túnel sigue funcionando por relay, solo que sin camino directo.
- [Antivirus contra un exe nuevo sin firmar] → Igual que los demás exes del paquete; el LEEME ya cubre ese caso.
- [Dos juegos en línea a la vez] → Se permite una sala a la vez por launcher; la tarjeta de los demás juegos se desactiva mientras tanto.
- [Tamaño del paquete] → `cps-tunel.exe` pesa unos 13 MB (12.7 MB en la prueba).
- [Diverger del launcher del autor] → Los cambios en `recomp-common` son aditivos (un campo de `Description`, una tarjeta, `GameProcess::stop`); se mantienen en la rama del fork como los de DESKTOP.

## Migration Plan

- Se agrega sin quitar nada: sin `cps-tunel.exe` el launcher se ve como hoy.
- Se instala en `D:\CPS2\CPS2Recomp`, `D:\CPS2\CPS-LAUNCHER` y la copia para compartir, con respaldo del launcher y de los puentes (`_respaldo_online`), y se rehace el zip.
- Para volver atrás, basta con quitar `cps-tunel.exe` o reponer el launcher del respaldo.

## Open Questions

- **Cómo forzar el relay en una prueba local**: buscar en Iroh 1.4 una opción para no usar transportes IP directos, o un modo de prueba del túnel (`--relay-only`). No cambia ni la spec ni las tareas: es cómo se mide.
- **Puerto de netplay ocupado**: si 7845 está en uso, UltraCombo muestra su error. Se puede revisar después si hace falta leer el puerto del `ki1.ini`.
