# Spec Delta

## Purpose

Los modos de Hyper Street Fighter II: The Anniversary Edition, tomados de Super Turbo: el modo de entrenamiento y su dummy, los modos alternativos, y las páginas Board y Cheats, en el menú del juego y en el launcher.

## ADDED Requirements

### Requirement: Modo de entrenamiento
HSF2 SHALL tener el modo de entrenamiento de Super Turbo: empezar una sesión desde el menú, energía y barra que se rellenan, readouts de combo y stun, input display, hitboxes, frame data y frame meter, y elegir escenario y punto de reinicio.

#### Scenario: Sesión de entrenamiento
- **WHEN** se empieza una sesión de entrenamiento sin ventana con hitboxes y frame meter encendidos y se golpea al dummy
- **THEN** las capturas muestran las cajas sobre los dos personajes, el frame meter y el daño del combo, y la energía del dummy vuelve a llena

### Requirement: Dummy
El dummy SHALL poder quedarse parado, agacharse o saltar, grabar y repetir lo que el jugador hace (con slots), bloquear y responder con una grabación.

#### Scenario: Grabar y repetir
- **WHEN** se graba una secuencia en el slot 1 y se pide que el dummy la repita
- **THEN** el dummy hace la misma secuencia (las mismas entradas, cuadro por cuadro)

#### Scenario: Bloqueo
- **WHEN** el dummy está en "bloquea siempre" y recibe una serie de golpes
- **THEN** los bloquea y su energía casi no baja

### Requirement: Modos alternativos
HSF2 SHALL tener la batalla infinita, survival, ring out, el modo demo con sus partes y el visor de escenarios. Cada pelea que estos modos arman SHALL tener una versión para cada luchador; por defecto, la de Super Turbo.

#### Scenario: Batalla infinita
- **WHEN** se elige la batalla infinita con dos luchadores y un escenario
- **THEN** la pelea empieza con ellos en ese escenario y sigue sin fin, con la versión elegida para cada uno

#### Scenario: Visor de escenarios
- **WHEN** se elige un escenario en el visor
- **THEN** se ve ese escenario sin luchadores y la cámara lo recorre

### Requirement: Página Board y página Cheats
HSF2 SHALL tener una página Board con los ajustes de su placa (dificultad, velocidad, continuar, demo sound) y una página Cheats (sin daño para cada jugador, un golpe gana, tiempo infinito), según la regla para todo juego nuevo.

#### Scenario: Tiempo infinito
- **WHEN** se enciende "Infinite time" a media pelea
- **THEN** el reloj del round se detiene y vuelve a correr al apagarlo

### Requirement: HSF2 sin funciones igual que antes
Con todas sus funciones apagadas, HSF2 SHALL jugar igual que antes de este cambio.

#### Scenario: Corrida de antes
- **WHEN** se corre HSF2 sin `settings.ini` con la misma corrida que dio `h_mus.txt`
- **THEN** los hashes por cuadro son idénticos

### Requirement: Run-ahead con los modos
Los modos SHALL dar el mismo resultado con y sin run-ahead.

#### Scenario: Entrenamiento con run-ahead
- **WHEN** se corre la misma película en una sesión de entrenamiento con y sin run-ahead
- **THEN** los hashes por cuadro son idénticos
