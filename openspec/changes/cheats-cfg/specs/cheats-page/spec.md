# Spec Delta

## Purpose

La página "Cheats" que el toolkit arma desde el `game.cfg` de un juego: trucos que escriben en la RAM del juego mientras están encendidos, para probar algo rápido (energía, tiempo, personaje, arma) y luego validarlo sin ellos.

## ADDED Requirements

### Requirement: Trucos declarados en game.cfg
Un juego SHALL poder declarar en `game.cfg` hasta 16 trucos. Cada uno lleva nombre, título, descripción y, por cada opción, las escrituras a RAM (dirección, tamaño byte, palabra o long, y valor), y puede llevar una condición (una dirección que debe valer algo). Una declaración mal escrita SHALL impedir que el juego arranque, con un mensaje que nombre el truco.

#### Scenario: Truco de encendido y apagado
- **WHEN** SF2 CE declara "Energía infinita P1" con una escritura de palabra
- **THEN** la página Cheats lo muestra con Off y On

#### Scenario: Truco con opciones
- **WHEN** Ghouls declara "Elegir arma" con una opción por arma
- **THEN** la página lo muestra con Off y el nombre de cada arma

#### Scenario: Declaración rota
- **WHEN** un truco escribe en una dirección fuera de la RAM de trabajo (FF0000-FFFFFF)
- **THEN** el juego no arranca y el mensaje nombra el truco y la dirección

### Requirement: En el menú y en el launcher
Los trucos SHALL aparecer en una página "Cheats" del menú del juego (F1/Esc) y en la del juego en el launcher, empezar apagados, guardarse en el `settings.ini` del juego y poder cambiarse en vivo.

#### Scenario: Prender y apagar en vivo
- **WHEN** a media pelea se prende "Tiempo infinito" y luego se apaga
- **THEN** el reloj se detiene mientras está prendido y vuelve a correr al apagarlo

### Requirement: Se aplican antes de cada cuadro
Mientras un truco esté encendido (y su condición se cumpla), sus escrituras SHALL hacerse antes de cada cuadro que corra el juego, también los del run-ahead.

#### Scenario: Energía infinita
- **WHEN** con "Energía infinita P1" encendido el jugador 1 recibe golpes durante un round
- **THEN** su barra vuelve a llena antes del cuadro siguiente y el round no termina por su energía

#### Scenario: Run-ahead
- **WHEN** se corre la misma película con un truco encendido, con y sin run-ahead
- **THEN** los hashes por cuadro y el audio son idénticos

#### Scenario: Condición
- **WHEN** un truco de personaje con condición "solo en la selección" está encendido y empieza la pelea
- **THEN** deja de escribir al terminar la selección, y el personaje elegido sigue en la pelea

### Requirement: Sin trucos, sin cambios
Con todos los trucos apagados, o en un juego que no declara ninguno, el juego SHALL correr igual que antes de este cambio y no SHALL ganar una página Cheats.

#### Scenario: Todo apagado
- **WHEN** se corre SF2 CE, Ghouls y HSF2 sin `settings.ini`, con la misma corrida de antes del cambio
- **THEN** los hashes por cuadro son idénticos y HSF2 no tiene página Cheats

### Requirement: Trucos de los juegos
SF2 CE, Hyper Fighting y Ghouls SHALL traer sus trucos con direcciones comprobadas en su ROM: energía infinita de cada jugador, tiempo infinito, un golpe gana y jugar con los jefes en SF2; vidas infinitas, tiempo infinito, armadura y arma en Ghouls. Un truco que no se pueda hacer con escrituras a RAM SHALL quedar anotado como no hecho.

#### Scenario: Comprobados sin ventana
- **WHEN** se corre cada juego sin ventana con cada truco encendido (con `--features`)
- **THEN** las capturas y la RAM muestran el efecto que dice su descripción
