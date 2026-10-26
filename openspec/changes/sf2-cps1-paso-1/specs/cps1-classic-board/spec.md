# Spec Delta

## Purpose

La placa CPS-1 clásica (sin QSound) en el toolkit: cómo la declara un juego, qué hace el cargador con su programa de sonido mientras no haya chips de sonido, y cómo lee los seis botones de los juegos de pelea de esa placa.

## ADDED Requirements

### Requirement: Declarar la placa clásica
Un juego SHALL poder declarar en su `game.cfg` que está en una placa CPS-1 clásica, distinta del CPS-1 con QSound. Los juegos que hoy declaran `board = cps1` SHALL seguir comportándose igual.

#### Scenario: Juego de CPS-1 clásico
- **WHEN** un `game.cfg` declara la placa CPS-1 clásica
- **THEN** el juego se carga como CPS-1 (programa sin cifrar, objetos en la memoria de video, chip CPS-B según su `cps_b`) sin QSound

#### Scenario: Dino sin cambios
- **WHEN** se compila un juego con `board = cps1` (CPS-1 con QSound)
- **THEN** su programa de sonido se descifra con Kabuki y su QSound funciona como antes

### Requirement: Programa de sonido sin cifrar
En la placa clásica, el cargador SHALL comprobar los archivos del programa de sonido y de los samples por su CRC32, pero NO SHALL intentar descifrarlos con Kabuki ni rechazar el set por no encontrar esa clave.

#### Scenario: SF2 CE carga
- **WHEN** se carga el set `sf2ce` en la placa clásica
- **THEN** el set carga sin el error "the sound program's cipher could not be worked out"

### Requirement: Z80 detenido sin chips de sonido
Mientras el toolkit no tenga los chips de sonido de la placa clásica, su Z80 SHALL quedar detenido y el juego SHALL correr en silencio. Las escrituras del 68000 a los registros de sonido de la placa SHALL aceptarse sin efecto y sin llenar el registro de accesos no mapeados en cada cuadro.

#### Scenario: Corrida en silencio
- **WHEN** se corre un juego de la placa clásica sin ventana con `--audio-dump`
- **THEN** el juego avanza normalmente y el volcado de audio es silencio

### Requirement: Seis botones
En la placa clásica, las patadas (botones 4 a 6) de los jugadores 1 y 2 SHALL leerse en la dirección donde la placa de SF2 las pone, activas en bajo, con las del jugador 1 en los bits 0 a 2 y las del jugador 2 en los bits 4 a 6.

#### Scenario: Patada fuerte
- **WHEN** el jugador 1 mantiene la patada fuerte en una pelea
- **THEN** el juego lee ese botón como presionado y el personaje patea
