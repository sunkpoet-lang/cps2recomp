# Spec Delta

## Purpose

Permite alternar mientras se juega entre el soundtrack propio del jugador y la música de la placa con una sola tecla, oyendo el cambio al instante y viendo en pantalla cuál suena, sin cambiar la partida cuando no se usa.

## ADDED Requirements

### Requirement: Tecla para alternar la música
Mientras el juego corre, F12 SHALL alternar el soundtrack propio entre apagado y su último valor encendido (en orden o aleatorio); si nunca estuvo encendido, SHALL encenderse en orden. El cambio SHALL quedar en la misma opción que muestra el menú. Si el jugador asignó F12 a un control del juego, F12 SHALL hacer solo eso.

#### Scenario: Encender desde apagado
- **WHEN** el soundtrack propio está apagado y el jugador presiona F12
- **THEN** el soundtrack propio queda encendido y el menú lo muestra encendido

#### Scenario: Apagar y volver al modo anterior
- **WHEN** el soundtrack propio está en "aleatorio" y el jugador presiona F12 dos veces
- **THEN** queda apagado tras la primera vez y otra vez en "aleatorio" tras la segunda

#### Scenario: F12 asignado al juego
- **WHEN** el jugador asignó F12 a un botón del juego y lo presiona
- **THEN** solo se aprieta ese botón y la música no cambia

### Requirement: Encender se oye al instante
Al encender el soundtrack propio con la tecla mientras suena una música del juego que tiene archivo, la placa SHALL dejar de tocar esa música y el archivo SHALL empezar a sonar en el mismo cuadro, en el punto que corresponde al tiempo que lleva sonando esa canción en el juego.

#### Scenario: A media pelea
- **WHEN** suena la música de la placa del escenario de Ryu desde hace 40 s y el jugador enciende el soundtrack con F12
- **THEN** la placa calla su música y el archivo de Ryu suena desde el segundo 40 de su parte de la canción, sin esperar al siguiente round

#### Scenario: Canción sin archivo
- **WHEN** suena una música para la que el jugador no tiene archivo y enciende el soundtrack con F12
- **THEN** la placa sigue tocando esa música

### Requirement: Apagar se oye al instante
Al apagar el soundtrack propio con la tecla mientras un archivo suena en lugar de una música del juego, el archivo SHALL callar y la placa SHALL tocar esa misma música desde su principio.

#### Scenario: A media pelea
- **WHEN** suena el archivo del escenario de Guile y el jugador apaga el soundtrack con F12
- **THEN** el archivo calla y la placa empieza a tocar el tema de Guile, sin silencio hasta el siguiente round

#### Scenario: Nada sonando
- **WHEN** no suena ninguna música (la que sonaba ya terminó o el juego la paró) y el jugador apaga el soundtrack con F12
- **THEN** la placa no recibe ninguna canción y sigue en silencio hasta que el juego pida otra

### Requirement: Aviso en pantalla
Cada vez que la tecla alterna la música, el juego SHALL mostrar sobre la imagen, durante unos 2 segundos, un letrero con la música activa: "Musica: Arreglada" al encender y "Musica: Arcade" al apagar (sin acento: la fuente del juego es solo ASCII). Una nueva pulsación SHALL reemplazar el letrero anterior. El letrero SHALL verse en la ventana y no SHALL quedar en las capturas automáticas sin ventana.

#### Scenario: Alternar dos veces
- **WHEN** el jugador presiona F12 dos veces seguidas
- **THEN** se ve solo el último letrero, y desaparece unos 2 segundos después de la segunda pulsación

#### Scenario: Carpeta sin archivos
- **WHEN** el jugador enciende el soundtrack con F12 pero su carpeta no tiene ningún archivo de música
- **THEN** el letrero dice "Musica: sin archivos" y la placa sigue tocando su música

### Requirement: La partida no cambia sin la tecla
Mientras no se presione la tecla, el juego SHALL correr exactamente igual que sin este cambio: las mismas entradas SHALL dar los mismos hashes por cuadro, con el soundtrack encendido o apagado, con o sin run-ahead. Una pulsación SHALL afectar solo a la parte de sonido de la placa, nunca a la memoria del juego.

#### Scenario: Corrida grabada sin F12
- **WHEN** se reproduce la misma película de entradas con la versión anterior y con esta, sin presionar F12
- **THEN** los hashes por cuadro son idénticos

#### Scenario: Con F12 y run-ahead
- **WHEN** el jugador presiona F12 con run-ahead encendido
- **THEN** la placa recibe la canción o el comando una sola vez, no una vez por cada cuadro que el run-ahead repite

### Requirement: Save states con la última canción
Los save states SHALL guardar cuál fue la última música que pidió el juego y desde cuándo suena, para que la tecla funcione igual tras cargar uno. Los save states de la versión anterior SHALL seguir cargando.

#### Scenario: Cargar y alternar
- **WHEN** el jugador carga un save state hecho a media pelea y presiona F12
- **THEN** el cambio de música suena con la canción de ese escenario

#### Scenario: Save state antiguo
- **WHEN** el jugador carga un save state hecho antes de este cambio
- **THEN** carga como antes, y la tecla actúa a partir de la siguiente canción que pida el juego

### Requirement: Alternar en pruebas sin ventana
Las pruebas sin ventana SHALL poder alternar la música en un tick dado con la misma lógica que la tecla, para comprobar el cambio con el volcado de audio.

#### Scenario: Tecla de prueba
- **WHEN** se corre sin ventana con una tecla de prueba que alterna la música en el tick 600 y se vuelca el audio
- **THEN** a partir de ese tick el volcado tiene la música del otro lado
