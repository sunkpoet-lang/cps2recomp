# Spec Delta

## Purpose

La tarjeta ONLINE del launcher, en la página de cada juego que trae netplay propio (Killer Instinct y Killer Instinct 2): crear una sala o unirse a una con un código, con el túnel de red y el juego arrancados por el launcher, para que alguien que apenas sabe usar Windows pueda jugar en línea.

## ADDED Requirements

### Requirement: Solo en juegos con netplay propio
El launcher SHALL mostrar la tarjeta ONLINE solo en la página de los juegos que declaren netplay propio (Killer Instinct y Killer Instinct 2), y solo si el túnel está junto al launcher. En los demás juegos SHALL no aparecer.

#### Scenario: Página de KI
- **WHEN** se abre la página de Killer Instinct con `cps-tunel.exe` junto al launcher
- **THEN** aparece la tarjeta ONLINE con "Create room" y "Join"

#### Scenario: Juego sin netplay
- **WHEN** se abre la página de Super Street Fighter II Turbo
- **THEN** no aparece la tarjeta ONLINE

#### Scenario: Falta el túnel
- **WHEN** `cps-tunel.exe` no está junto al launcher
- **THEN** la página de KI no muestra la tarjeta ONLINE

### Requirement: Crear sala
"Create room" SHALL arrancar el túnel como anfitrión para el puerto de netplay del juego, mostrar el código de sala en cuanto lo tenga, con un botón para copiarlo, y abrir el juego como anfitrión. Mientras espera, SHALL decir que espera a un rival.

#### Scenario: El anfitrión comparte el código
- **WHEN** el usuario da "Create room" en la página de KI
- **THEN** ve el código con "Copy" y KI se abre esperando a un jugador

#### Scenario: Copiar el código
- **WHEN** el usuario da "Copy"
- **THEN** el código queda en el portapapeles listo para pegarlo en WhatsApp, y la tarjeta dice que se copió

### Requirement: Unirse a una sala
"Join" SHALL ofrecer un campo para pegar el código y un botón para unirse. Al unirse, SHALL arrancar el túnel como invitado y, cuando informe su puerto local, abrir el juego unido a ese puerto. Un código inválido SHALL mostrarse como error sin abrir el juego.

#### Scenario: El invitado pega el código
- **WHEN** el usuario pega el código del anfitrión y da "Join"
- **THEN** KI se abre conectado al anfitrión y los dos juegos empiezan juntos

#### Scenario: Código mal pegado
- **WHEN** el usuario pega solo la mitad del código
- **THEN** la tarjeta dice que el código no es válido y el juego no se abre

### Requirement: Estado de la conexión
Mientras hay sala, la tarjeta SHALL mostrar el estado del túnel en palabras sencillas: esperando, conectando, conectado directo o por relay, y el ping en milisegundos. Un error del túnel SHALL mostrarse en la tarjeta.

#### Scenario: Conectado por relay
- **WHEN** los dos jugadores se conectan a través de un relay
- **THEN** la tarjeta dice que están conectados por relay y muestra el ping

### Requirement: El túnel vive con el juego
El launcher SHALL cerrar el túnel cuando el juego se cierra, y SHALL dejar la tarjeta lista para crear o unirse a otra sala. Mientras hay sala, "Play" SHALL decir "Playing online…" y estar desactivado, y la otra opción de la tarjeta también, para ese juego.

#### Scenario: Fin de la partida
- **WHEN** el anfitrión cierra KI
- **THEN** su túnel se cierra, la tarjeta vuelve a "Create room" y "Join", y el invitado ve que la sala se cerró

#### Scenario: Play mientras hay sala
- **WHEN** hay una sala abierta para KI
- **THEN** el botón Play dice "Playing online…" y no se puede presionar

### Requirement: Salir de la sala desde la tarjeta
La tarjeta SHALL ofrecer salir sin cerrar el juego a mano: "Close room" mientras el anfitrión espera y "Leave" una vez conectados, en los dos lados. Al usarlo, el launcher SHALL cerrar el juego y el túnel, y dejar la tarjeta lista para otra sala.

#### Scenario: El anfitrión se arrepiente
- **WHEN** el anfitrión da "Close room" mientras espera
- **THEN** KI y el túnel se cierran y la tarjeta vuelve a "Create room" y "Join"

#### Scenario: Salir de una partida
- **WHEN** el invitado da "Leave" a media partida
- **THEN** su KI y su túnel se cierran, y la tarjeta del anfitrión dice que el otro jugador se fue

### Requirement: Argumentos del juego en línea
Al abrir el juego en línea, el launcher SHALL pasarle, además de sus argumentos normales, los de netplay del juego: anfitrión con su puerto, o invitado con `127.0.0.1` y el puerto que informó el túnel. Los demás ajustes del juego SHALL ser los mismos que con "Play".

#### Scenario: KI como invitado
- **WHEN** el túnel invitado informa el puerto 53000
- **THEN** UltraCombo arranca con `--game ki1 --join 127.0.0.1:53000`
