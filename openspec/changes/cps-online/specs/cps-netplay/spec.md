## ADDED Requirements

### Requirement: Anfitrión e invitado
Un juego de CPS SHALL jugar en línea con `--host [PUERTO]` (jugador 1, 7845 si no se dice) o `--join HOST:PUERTO`
(jugador 2). El anfitrión SHALL aceptar al primero que salude, desde cualquier dirección (el túnel). Mientras no estén
los dos, el juego SHALL esperar y decirlo en pantalla.

#### Scenario: Dos programas en una PC
- **WHEN** uno corre `ssf2t --host 7845` y otro `ssf2t --join 127.0.0.1:7845`
- **THEN** arrancan juntos, y los controles de cada uno son su jugador

### Requirement: Los dos lados iguales
Antes de jugar, los dos lados SHALL comparar el juego, sus ROMs, la versión y los ajustes que cambian la partida, y
SHALL negarse a jugar si algo difiere, diciendo qué. Durante la partida SHALL comparar la suma del estado de los cuadros
confirmados; una diferencia SHALL terminar la sesión diciendo el cuadro.

#### Scenario: Ajustes distintos
- **WHEN** un jugador tiene otra dificultad en la página Board
- **THEN** ninguno de los dos juegos arranca, y los dos dicen qué ajuste es distinto

### Requirement: Lo que se apaga en línea
En línea SHALL NOT haber trucos, save states, pausa, cámara lenta, run-ahead, modos, interruptor de prueba ni el
soundtrack propio: se oye la música de la placa. El menú SHALL NOT detener el juego.

#### Scenario: El soundtrack encendido
- **WHEN** un jugador tiene el soundtrack del CD y empieza una partida en línea
- **THEN** oye la música de la placa mientras dura la sesión

### Requirement: La prueba de estrés
`--stress` SHALL correr la sesión de estrés de GekkoNet (rollback en cada cuadro) y SHALL avisar del primer cuadro que
difiera. Un juego SHALL pasarla antes de tener la tarjeta ONLINE.

#### Scenario: Super Turbo la pasa
- **WHEN** `ssf2t --headless --stress --auto-play` corre 6000 cuadros
- **THEN** no avisa ninguna diferencia
