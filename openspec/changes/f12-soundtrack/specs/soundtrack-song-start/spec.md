# Spec Delta

## Purpose

Permite que un archivo del soundtrack propio empiece en un punto distinto de su comienzo, para usar partes de una misma pista (como un tema y su versión acelerada pegadas en un CD) en canciones distintas del juego.

## ADDED Requirements

### Requirement: Inicio de un archivo por canción
La sección de una canción en `soundtrack.ini` SHALL aceptar `start` con un tiempo en el mismo formato que `loop` (minutos:segundos o segundos). La canción SHALL empezar a sonar en ese punto del archivo en lugar de 0. Sin `start`, SHALL empezar en 0 como hasta ahora.

#### Scenario: Versión acelerada en la misma pista
- **WHEN** `[ryu_later]` dice `file = 03 Track 3.mp3` y `start = 60.43`, y el juego pide `ryu_later`
- **THEN** suena "03 Track 3.mp3" desde el segundo 60.43

#### Scenario: Sin start
- **WHEN** una canción tiene `file` pero no `start`
- **THEN** suena desde el principio del archivo, como antes

### Requirement: Inicio y repetición
Con `start` y sin `loop`, la canción SHALL repetirse desde `start` hasta el final del archivo. Con `loop`, SHALL repetirse el tramo que dice `loop`, que SHALL poder quedar antes de `start`, como el final de la versión normal que se repite sin llegar a la acelerada.

#### Scenario: Acelerada repetida
- **WHEN** `[ryu_later]` tiene `start = 60.43` y no tiene `loop`
- **THEN** al terminar el archivo vuelve a sonar desde el segundo 60.43, nunca desde 0

#### Scenario: Normal sin pasar a la acelerada
- **WHEN** `[ryu]` usa el mismo archivo con `loop = 0 - 60.43`
- **THEN** la canción se repite del 0 al 60.43 y nunca suena la parte acelerada

### Requirement: Valores de start inválidos
Un `start` que no se pueda leer, negativo o más allá del final del archivo SHALL tomarse como 0, y la canción SHALL sonar igual que sin `start`.

#### Scenario: Start mayor que el archivo
- **WHEN** `start = 9:00` y el archivo dura 2:05
- **THEN** la canción suena desde el principio del archivo
