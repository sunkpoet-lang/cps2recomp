# Spec Delta

## Purpose

El sonido de la placa CPS-1 clásica (sin QSound): su Z80 corriendo con su mapa de memoria y su interrupción, los pedidos de sonido del 68000, el YM2151 y el OKI MSM6295, su mezcla a la salida del juego y su lugar en los save states.

## ADDED Requirements

### Requirement: El Z80 corre con sus chips
En la placa CPS-1 clásica, el Z80 SHALL correr desde el inicio a 3.579545 MHz con su programa de sonido sin cifrar, leyendo y escribiendo su ROM, su banco, su RAM y los registros del YM2151 y del OKI donde la placa los tiene, en lugar de quedar detenido.

#### Scenario: Arranque
- **WHEN** arranca SF2 Champion Edition
- **THEN** el Z80 ejecuta su programa, inicializa los dos chips y queda esperando pedidos sin colgarse

### Requirement: Interrupción del YM2151
La interrupción del Z80 en la placa clásica SHALL venir del YM2151: activa mientras un timer del chip que tenga la interrupción habilitada haya vencido, e inactiva cuando el programa lo reinicia. No SHALL haber interrupción periódica fija.

#### Scenario: Tempo de la música
- **WHEN** suena una pieza de música durante 10 segundos de juego
- **THEN** su tempo coincide con el de la grabación de referencia de la misma pieza (diferencia menor al 1 %)

### Requirement: Pedidos del 68000
Las escrituras del 68000 a los latches de sonido de la placa (`0x800181` y `0x800189`) SHALL llegar al Z80 en sus direcciones de lectura, y el Z80 SHALL tocar el sonido o la música pedidos.

#### Scenario: Efecto al golpear
- **WHEN** un personaje golpea a otro en una pelea
- **THEN** el audio del juego tiene el efecto del golpe en ese momento

### Requirement: YM2151
El YM2151 SHALL generar la música FM de la placa, en estéreo, a partir de las escrituras del Z80 a sus registros, con sus timers, su LFO y su ruido.

#### Scenario: Música de la selección
- **WHEN** el juego está en la selección de personaje
- **THEN** suena su música, con la melodía y el tempo de la grabación de referencia

### Requirement: OKI MSM6295
El OKI MSM6295 SHALL tocar las voces y los efectos digitalizados de la placa: cuatro voces ADPCM de 4 bits desde las ROMs de samples, iniciadas y detenidas por los comandos del Z80, con el volumen de cada comando y la frecuencia que fija el pin 7.

#### Scenario: Voz del anunciador
- **WHEN** empieza un round
- **THEN** se oye la voz del anunciador ("Round 1"... "Fight!") clara y a su tono, como en la grabación de referencia

### Requirement: Mezcla a la salida del juego
El audio de los dos chips SHALL mezclarse y entregarse a la salida que ya usa el juego (estéreo, la frecuencia de `QSound::kSampleRate`), sin saturar en los momentos más fuertes de una pelea.

#### Scenario: Pelea con todo sonando
- **WHEN** se vuelca el audio de un round completo
- **THEN** no hay muestras recortadas al máximo por más de una centésima del tiempo

### Requirement: Save states y run-ahead
El estado del Z80 de la placa clásica, de sus latches, del YM2151 y del OKI SHALL guardarse en los save states y recuperarse al cargarlos, de modo que el run-ahead no cambie el sonido. Los save states de versiones anteriores SHALL seguir cargando.

#### Scenario: Run-ahead
- **WHEN** se corre la misma película de entradas con y sin run-ahead
- **THEN** el audio volcado es idéntico

### Requirement: Los demás juegos sin cambios
CPS-2 y el CPS-1 con QSound SHALL seguir comportándose igual: mismos hashes por cuadro y mismo audio.

#### Scenario: HSF2
- **WHEN** se corre HSF2 con la misma corrida de antes del cambio
- **THEN** los hashes por cuadro son idénticos
