# Spec Delta

## Purpose

El port nativo de Ghouls'n Ghosts (World) sobre la placa CPS-1 clásica: qué set acepta, que corra traducido con su imagen, sus dos botones y su sonido FM, y que se abra desde el launcher de la colección.

## ADDED Requirements

### Requirement: Set aceptado
El port SHALL aceptar el set `ghouls` (World), comprobando cada uno de sus 26 archivos por CRC32 aunque el zip traiga también los clones (`ghoulsu`, `daimakai`, `daimakair`). Un archivo de otra revisión SHALL rechazarse nombrando el archivo.

#### Scenario: Zip del usuario
- **WHEN** la carpeta de ROMs tiene el `ghouls.zip` del usuario, con 74 archivos
- **THEN** el port encuentra sus 26 archivos por CRC y arranca

#### Scenario: Otra revisión
- **WHEN** se le da un archivo de programa de la versión USA (`dmu_29.10h`) en lugar del World
- **THEN** no arranca y el mensaje nombra el archivo que falta y su CRC32

### Requirement: Imagen como en la placa
El juego SHALL dibujar sus tres capas de fondo y sus objetos con los tiles correctos, según el mapper de gráficos de su placa (DM620), en 384 x 224.

#### Scenario: Primer nivel
- **WHEN** se llega al cementerio del primer nivel
- **THEN** la captura muestra el escenario, a Arthur, los enemigos y el marcador con tiles correctos, sin basura ni capas vacías

### Requirement: Dos botones
El juego SHALL leer dos botones por jugador (disparo y salto) y el launcher SHALL mostrarlos con esos nombres en su página de controles.

#### Scenario: Disparo y salto
- **WHEN** en una partida se presiona disparo y después salto
- **THEN** Arthur lanza su arma y salta

### Requirement: Sonido FM
El juego SHALL sonar con su música y sus efectos del YM2151, sin voces (la placa de Ghouls no tiene ROMs del OKI), y el OKI SHALL quedar en silencio sin fallar.

#### Scenario: Música del primer nivel
- **WHEN** se vuelca el audio de 6000 cuadros que incluyen la intro y el primer nivel
- **THEN** hay música y efectos, ninguna muestra recortada y ningún error del OKI

### Requirement: Corre en nativo
El juego SHALL correr con su programa del 68000 traducido después de compilar y explorar, y el run-ahead SHALL dejar el audio idéntico.

#### Scenario: Corrida sin ventana
- **WHEN** se corre sin ventana con jugadas inventadas durante 6000 cuadros, con y sin run-ahead
- **THEN** `--stats` informa cuánto pasó por el intérprete y los dos volcados de audio son idénticos

### Requirement: En el launcher
El launcher de la colección SHALL listar Ghouls'n Ghosts, encontrar su set en la carpeta de ROMs y abrirlo.

#### Scenario: Carpeta del usuario
- **WHEN** se abre el launcher instalado con `ghouls.zip` en la carpeta `ROM`
- **THEN** el juego aparece entre los encontrados con Play activo

### Requirement: Sin datos del juego en la repo
El cambio SHALL agregar a la repo solo código del toolkit, configuración y documentación, nunca archivos del set ni nada derivado de ellos.

#### Scenario: Auditoría
- **WHEN** se corre `tools/audit.sh` con los archivos nuevos
- **THEN** pasa, y ningún archivo rastreado está bajo `rom/`, `work/` ni `build/`
