# Spec Delta

## Purpose

La música de Hyper Street Fighter II para el custom soundtrack: qué piezas reconoce el port, cómo se llaman y que el soundtrack del jugador y F12 funcionen en este juego como en Super Turbo.

## ADDED Requirements

### Requirement: Opción de soundtrack propio
El port de HSF2 SHALL ofrecer la feature "Custom soundtrack" en el launcher y en el menú del juego, y SHALL escribir el `songs.txt` con sus piezas de música en la carpeta del soundtrack, como los demás juegos que tienen música con nombre.

#### Scenario: Carpeta nueva
- **WHEN** el jugador enciende el soundtrack de HSF2 con una carpeta sin `songs.txt`
- **THEN** el juego escribe ahí `songs.txt` con las 68 piezas de música, cada una con su nombre y su número

### Requirement: Nombres de las piezas
Cada pieza de música que el juego pide en las pantallas probadas (título, selección de personaje, versus, cada escenario y su versión "later", ganador, continue y game over) SHALL tener el mismo nombre que su equivalente en Super Turbo cuando el número y la pantalla coinciden, para que un `soundtrack.ini` hecho para Super Turbo sirva sin cambios.

#### Scenario: Los mismos nombres que Super Turbo
- **WHEN** el soundtrack de HSF2 apunta a su carpeta propia, con un `soundtrack.ini` que usa las mismas secciones que el de Super Turbo
- **THEN** en el escenario de Guile suena la pista del CD asignada a `guile`, y en la pantalla del ganador la asignada a `winner`

#### Scenario: Pieza que solo tiene HSF2
- **WHEN** el juego pide una pieza que Super Turbo no pide en esa pantalla
- **THEN** la pieza tiene un nombre propio en `songs.txt` y, sin archivo para ella, suena la de la placa

### Requirement: F12 en HSF2
F12 SHALL alternar en HSF2 entre el soundtrack propio y la música de la placa al instante, con el mismo letrero, como lo especifica `soundtrack-live-toggle`.

#### Scenario: A media pelea
- **WHEN** suena la pista del CD en una pelea de HSF2 y el jugador presiona F12
- **THEN** la placa toca esa música desde su principio y el letrero dice "Musica: Arcade"

### Requirement: La partida no cambia sin la tecla
Con el soundtrack apagado, HSF2 SHALL correr exactamente igual que el port del nivel 1: las mismas entradas SHALL dar los mismos hashes por cuadro.

#### Scenario: Corrida inventada
- **WHEN** se corre la misma corrida de juego inventado (`--auto`) con el `hsf2.exe` del nivel 1 y con el nuevo, con el soundtrack apagado
- **THEN** los hashes por cuadro son idénticos
