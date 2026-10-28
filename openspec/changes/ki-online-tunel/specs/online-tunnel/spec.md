# Spec Delta

## Purpose

El túnel de red que permite jugar en línea un juego con netplay propio por UDP (hoy Killer Instinct en UltraCombo) sin abrir puertos ni usar una VPN: lleva los paquetes del juego entre dos PCs, directo o por un relay público, y se conecta con un código para copiar y pegar.

## ADDED Requirements

### Requirement: Modo anfitrión
El túnel SHALL tener un modo anfitrión que recibe el puerto UDP local del juego, se registra en la red, imprime un código de sala y espera invitados. Al conectarse uno, SHALL entregar al juego, en `127.0.0.1:<puerto>`, cada paquete de ese invitado, y al invitado cada respuesta del juego dirigida a él.

#### Scenario: Crear sala
- **WHEN** se arranca el túnel como anfitrión para el puerto 7845
- **THEN** imprime un código de sala y espera, sin que el usuario abra puertos en su módem

#### Scenario: Llega un invitado
- **WHEN** un invitado se une con ese código y su juego manda un saludo
- **THEN** el juego anfitrión lo recibe desde `127.0.0.1` y sus respuestas llegan al juego invitado

### Requirement: Varios invitados en una sala
El anfitrión SHALL aceptar varios invitados con el mismo código (hasta 8), para que un juego con espectadores los reciba. Cada invitado SHALL llegar al juego desde su propio puerto en `127.0.0.1`, siempre el mismo mientras esté conectado, para que el juego los distinga. Qué hace el juego con cada uno (jugar, mirar o ignorarlo) lo decide el juego.

#### Scenario: Un segundo invitado
- **WHEN** dos invitados se unen a la misma sala
- **THEN** el juego anfitrión recibe los paquetes de cada uno desde un puerto local distinto y sus respuestas llegan a cada uno por separado

#### Scenario: KI con un invitado de más
- **WHEN** a una sala de KI ya con rival se une un segundo invitado
- **THEN** la partida sigue sin cambios: UltraCombo atiende solo a su primer rival

#### Scenario: Sala llena
- **WHEN** ya hay 8 invitados y llega otro
- **THEN** el túnel lo rechaza y el túnel de ese invitado informa que la sala está llena

### Requirement: Modo invitado
El túnel SHALL tener un modo invitado que recibe un código de sala, abre un puerto UDP local en `127.0.0.1` y lo informa. El juego invitado se une a ese puerto como si fuera el anfitrión; el túnel SHALL llevar sus paquetes al anfitrión y devolverle las respuestas.

#### Scenario: Unirse con un código
- **WHEN** se arranca el túnel como invitado con un código válido
- **THEN** informa un puerto local y, al conectarse, el juego unido a `127.0.0.1:<ese puerto>` juega contra el anfitrión

#### Scenario: Código inválido
- **WHEN** el código pegado está incompleto o no es un código de sala
- **THEN** el túnel informa un error que dice que el código no es válido y termina

### Requirement: Código de sala sin servidor
El código de sala SHALL contener todo lo necesario para encontrar al anfitrión (su identidad y su relay), sin un servidor propio. SHALL usar solo letras y números, medir a lo más 120 caracteres y tolerar espacios, saltos de línea y minúsculas al pegarse.

#### Scenario: Pegado desde WhatsApp
- **WHEN** el código llega partido en dos líneas y con minúsculas
- **THEN** el túnel invitado lo acepta y se conecta

### Requirement: Conexión directa o por relay
El túnel SHALL intentar una conexión directa entre las dos PCs y, si no se puede, SHALL usar un relay público, sin que el usuario configure nada. SHALL informar cuál de los dos usa y cambiar al directo si se vuelve posible a media partida.

#### Scenario: Detrás de routers distintos
- **WHEN** anfitrión e invitado están en casas distintas sin puertos abiertos
- **THEN** se conectan, por directo o por relay, y el túnel dice cuál

### Requirement: Paquetes sin cambios ni retrasos
El túnel SHALL entregar cada paquete del juego tal cual, sin reordenarlo ni retransmitirlo: si uno se pierde, el netplay del juego lo resuelve. Los paquetes de hasta 1,030 bytes (el mayor de UltraCombo) SHALL pasar siempre; uno que no quepa SHALL contarse e informarse.

#### Scenario: Una partida sin desync
- **WHEN** dos UltraCombo juegan 2,400 cuadros con el mismo guion de entradas a través del túnel
- **THEN** los dos terminan con el mismo hash de estado

#### Scenario: Bloque del disco
- **WHEN** el anfitrión manda los bloques de 1,030 bytes de los ajustes del disco
- **THEN** llegan todos y la sesión empieza

### Requirement: Líneas de estado
El túnel SHALL informar su estado por la salida estándar, un evento por línea con una palabra clave al inicio: código de sala, puerto local, conectado, tipo de camino (directo o relay), ping en milisegundos, cerrado y error con su mensaje. En el anfitrión, los eventos de cada invitado SHALL llevar su número de invitado.

#### Scenario: El launcher lee el estado
- **WHEN** el túnel se conecta y pasa al camino directo
- **THEN** imprime una línea de conectado, una de camino directo y, cada pocos segundos, una de ping con un número

#### Scenario: Eventos de dos invitados
- **WHEN** en el anfitrión hay dos invitados conectados
- **THEN** sus líneas de conectado, camino y ping dicen a qué invitado (1 o 2) corresponden

### Requirement: Fin del túnel
El túnel invitado SHALL terminar e informarlo cuando el anfitrión se desconecta o deja de responder unos segundos. El anfitrión SHALL informar la salida de cada invitado, liberar su puerto local y seguir con los demás. Los dos SHALL poder cerrarse desde fuera sin dejar puertos ocupados.

#### Scenario: El rival cierra el juego
- **WHEN** el invitado cierra su juego y su túnel
- **THEN** el túnel anfitrión informa que ese invitado se fue y sigue abierto para la sala

#### Scenario: El anfitrión cierra el juego
- **WHEN** el launcher del anfitrión cierra su túnel
- **THEN** cada túnel invitado informa que se cerró y termina
