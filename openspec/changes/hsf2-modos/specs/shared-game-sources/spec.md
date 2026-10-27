# Spec Delta

## Purpose

Cómo un juego compila las fuentes de otro juego hecho del mismo programa: cada dirección traducida a la suya, sus propios archivos en lugar de los compartidos del mismo nombre y los compartidos que no le sirven dejados fuera.

## ADDED Requirements

### Requirement: Direcciones traducidas al compilar
Las fuentes compartidas SHALL nombrar cada dirección de la ROM o de la RAM del juego original con `At()` o `AtA5()`. Al compilarlas para otro juego, cada dirección SHALL traducirse con la lista de su `family.h`, y una dirección que falte en la lista SHALL detener la compilación con un mensaje.

#### Scenario: Super Turbo igual que antes
- **WHEN** se compila Super Turbo con sus fuentes ya escritas con `At()` y se corre la misma corrida de antes del cambio
- **THEN** los hashes por cuadro y el audio son idénticos

#### Scenario: Dirección que falta
- **WHEN** a la lista de HSF2 le falta una dirección que usan las fuentes
- **THEN** la compilación de HSF2 se detiene diciendo que su `family.h` no la tiene

### Requirement: Archivo propio en lugar del compartido
Si un juego tiene en su carpeta `src/` un archivo con el mismo nombre que uno de las fuentes compartidas, SHALL compilarse el suyo y no el compartido.

#### Scenario: game.cpp de HSF2
- **WHEN** HSF2 tiene su `game.cpp` y compila las fuentes de Super Turbo
- **THEN** se compila el `game.cpp` de HSF2 y no el de Super Turbo, sin símbolos duplicados

### Requirement: Archivos compartidos dejados fuera
Un juego SHALL poder dejar fuera archivos de las fuentes compartidas con `sources_leave_out` en su `game.cfg`. Lo que esos archivos ofrecían no SHALL aparecer en su menú.

#### Scenario: Sin tag team en HSF2
- **WHEN** HSF2 deja fuera `livetag.cpp`
- **THEN** compila sin él y su menú no tiene la página del tag team

### Requirement: Otros juegos sin cambios
Los juegos que no comparten fuentes, y los de la familia de Vampire Savior, SHALL compilarse y correr igual que antes.

#### Scenario: Vampire Savior 2
- **WHEN** se compila Vampire Savior 2 después del cambio
- **THEN** compila con las mismas fuentes que antes
