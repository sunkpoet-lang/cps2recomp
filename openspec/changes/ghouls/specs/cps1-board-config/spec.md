# Spec Delta

## Purpose

Lo que el `game.cfg` de un juego CPS-1 puede declarar de su placa para que el toolkit la arme como la de fábrica: el reloj del 68000, cómo están grabados su programa y sus gráficos en las ROMs, y los valores de sus DIP switches.

## ADDED Requirements

### Requirement: Reloj del 68000 por juego
Un juego CPS-1 SHALL poder declarar en `game.cfg` el reloj de su 68000 (10 o 12 MHz), y el toolkit SHALL darle a la CPU los ciclos por línea de ese reloj. Sin la declaración, el reloj SHALL seguir siendo el de hoy (12 MHz), así que Dino, SF2 CE y Hyper Fighting no cambian.

#### Scenario: Ghouls a 10 MHz
- **WHEN** se corre Ghouls'n Ghosts, que declara 10 MHz
- **THEN** su 68000 recibe 10/12 de los ciclos por línea que recibe SF2 CE

#### Scenario: Juegos sin declarar
- **WHEN** se corre HSF2 con la misma corrida de antes del cambio
- **THEN** los hashes por cuadro son idénticos a los de antes

#### Scenario: SF2 sigue a 12 MHz
- **WHEN** se corre SF2 CE con DIP switches iguales a los de antes del cambio (todos apagados salvo C6)
- **THEN** los hashes por cuadro son idénticos a los de antes

### Requirement: Programa en pares de bytes
La lista de programa de `game.cfg` SHALL aceptar pares de archivos en los que uno guarda los bytes pares y el otro los impares de las mismas palabras, ocupando juntos el doble del tamaño de cada uno.

#### Scenario: Pares de Ghouls
- **WHEN** se cargan `dme_29.10h` + `dme_30.10j` y `dme_27.9h` + `dme_28.9j`
- **THEN** ocupan 0x00000-0x7FFFF del programa, intercalados byte a byte, como en MAME

### Requirement: Programa en palabras sin invertir
La lista de programa SHALL aceptar archivos de palabras de 16 bits grabadas en el orden del 68000 (byte alto primero), además de los de hoy (byte bajo primero).

#### Scenario: dm-17.7j
- **WHEN** se carga `dm-17.7j` después de los dos pares de Ghouls
- **THEN** ocupa 0x80000-0xFFFFF del programa con sus bytes en el orden en que están en el archivo

### Requirement: Gráficos en grupos de ocho bytes
La lista de gráficos SHALL aceptar grupos de ocho archivos de a byte, cada uno con un byte de las palabras de 64 bits, además de los grupos de cuatro archivos de dos bytes de hoy, en el orden en que se escriben.

#### Scenario: Gráficos de Ghouls
- **WHEN** se cargan los cuatro archivos `dm-05..08` y después los dos grupos de ocho archivos de 64 KB
- **THEN** los gráficos ocupan 3 MB en el orden de MAME (2 MB de los cuatro primeros, luego 512 KB por grupo)

### Requirement: DIP switches por juego
Un juego CPS-1 SHALL poder declarar en `game.cfg` los valores de sus tres bancos de DIP switches (A, B y C), y el 68000 SHALL leerlos donde los pone la placa. Sin la declaración, los tres SHALL leerse como hoy, todos apagados.

#### Scenario: Valores de fábrica de Ghouls
- **WHEN** Ghouls declara los valores de fábrica de MAME (3 vidas, dificultad 2, vida extra a 30K, 60K y cada 70K, demo sound, continuar)
- **THEN** el juego empieza con 3 vidas y la intro tiene sonido

#### Scenario: Valores de fábrica de SF2
- **WHEN** SF2 CE y Hyper Fighting declaran los valores de fábrica de MAME
- **THEN** juegan en dificultad 4 (normal), permiten continuar y la intro sigue con sonido

### Requirement: Formas viejas sin cambios
Las listas de ROMs de los juegos que ya existen SHALL cargarse igual que antes, byte por byte, y el launcher SHALL seguir encontrando sus archivos por CRC.

#### Scenario: Juegos de la colección
- **WHEN** se abren en el launcher los juegos instalados con sus ROMs
- **THEN** todos los que antes se encontraban se siguen encontrando, y Ghouls aparece con los suyos
