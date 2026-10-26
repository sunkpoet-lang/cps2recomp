# Spec Delta

## Purpose

La página "Board" que el toolkit arma desde el `game.cfg` de un juego con DIP switches: opciones con nombre para la dificultad, las vidas, el free play y demás, que el jugador cambia desde el menú del juego o desde el launcher.

## ADDED Requirements

### Requirement: Opciones declaradas en game.cfg
Un juego con DIP switches SHALL poder declarar en `game.cfg` opciones de placa, cada una con título, descripción, banco (A, B o C), bits y la lista de sus valores con nombre. Una declaración mal escrita SHALL impedir que el juego arranque, con un mensaje que nombre la opción.

#### Scenario: Declaración válida
- **WHEN** Ghouls declara cinco opciones de placa
- **THEN** el juego arranca y la página Board muestra esas cinco, con sus nombres y valores

#### Scenario: Declaración rota
- **WHEN** una opción declara bits fuera del banco (por ejemplo, bits 6-9)
- **THEN** el juego no arranca y el mensaje nombra la opción y el problema

### Requirement: En el menú del juego y en el launcher
Las opciones SHALL aparecer en una página "Board" del menú del juego (F1/Esc) y en la del juego en el launcher, y SHALL poder cambiarse en los dos lugares.

#### Scenario: Desde el menú
- **WHEN** en SF2 CE se abre el menú con F1 y se elige Board > Free play > On
- **THEN** la opción queda en On y sigue así al cerrar y volver a abrir el juego

#### Scenario: Desde el launcher
- **WHEN** en el launcher se elige Ghouls'n Ghosts y en su página Board se pone Vidas en 5
- **THEN** al jugar, el menú del juego muestra Vidas en 5

### Requirement: Como viene la placa
Cada opción SHALL empezar en "As the board is set", que deja esos bits como los trae la línea `dip` del juego. Con todas las opciones así, el juego SHALL correr igual que antes de este cambio.

#### Scenario: Sin elegir nada
- **WHEN** se corre SF2 CE y Ghouls sin `settings.ini`, con la misma corrida de antes del cambio
- **THEN** los hashes por cuadro son idénticos a los de antes

### Requirement: Lo elegido llega al juego
El valor elegido SHALL ponerse en los bits de su banco antes de cada cuadro, sin tocar los demás bits, y el juego SHALL leerlo cuando lo necesite. La descripción de cada opción SHALL decir cuándo hace efecto (al instante, al empezar una partida o con Reset).

#### Scenario: Free play en SF2
- **WHEN** se pone Free play en On en SF2 CE y se hace Reset
- **THEN** la pantalla de título pide Start sin créditos y se puede empezar sin moneda

#### Scenario: Vidas en Ghouls
- **WHEN** se ponen 5 vidas en Ghouls y se empieza una partida
- **THEN** el marcador muestra las vidas de más

#### Scenario: Dificultad
- **WHEN** se cambia la dificultad de SF2 CE a "7 (Hardest)"
- **THEN** el valor llega al banco B (se ve en un log de los DIP) y el juego lo usa desde el momento que dice la descripción

### Requirement: Se guarda por juego
Lo elegido SHALL guardarse en el `settings.ini` del juego, con un nombre que no se confunda con la opción de otro juego que se llame igual.

#### Scenario: Dos juegos
- **WHEN** se pone Dificultad en el valor más alto en SF2 CE
- **THEN** la dificultad de Hyper Fighting y la de Ghouls no cambian

### Requirement: Juegos sin opciones
Un juego que no declara opciones de placa SHALL seguir sin página Board generada, igual que hoy.

#### Scenario: HSF2
- **WHEN** se corre HSF2 con la misma corrida de antes del cambio
- **THEN** los hashes por cuadro son idénticos y su menú no gana una página Board nueva
