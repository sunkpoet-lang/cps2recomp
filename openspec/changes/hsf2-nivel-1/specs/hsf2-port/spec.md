# Spec Delta

## Purpose

El port nativo de Hyper Street Fighter II: The Anniversary Edition hecho con el toolkit: qué copia del juego acepta, que corra con su código traducido y que se pueda abrir desde el launcher de la colección.

## ADDED Requirements

### Requirement: Set aceptado
El port SHALL aceptar el set `hsf2j` (Japón 040202) con su llave `hsf2j.key`, tomando del set padre `hsf2` los archivos que el clon no trae, y SHALL comprobar cada archivo por su CRC32. Un set de otra región o revisión SHALL rechazarse con un mensaje que nombre el archivo que no coincide.

#### Scenario: Carpeta con el clon y su padre
- **WHEN** la carpeta de ROMs tiene `hsf2j.zip` y `hsf2.zip`
- **THEN** el juego carga todos sus archivos y arranca

#### Scenario: Falta el padre
- **WHEN** la carpeta tiene `hsf2j.zip` pero no `hsf2.zip`
- **THEN** el juego no arranca y el mensaje nombra el primer archivo que falta

#### Scenario: Otra revisión
- **WHEN** se le da un archivo de programa de otra revisión (otro CRC32)
- **THEN** el juego no arranca y el mensaje nombra ese archivo y su CRC32

### Requirement: Corre en nativo
El juego SHALL correr con su programa del 68000 traducido a código nativo después de compilar y explorar, dejando al intérprete solo lo que la exploración no alcanzó. Una corrida sin ventana SHALL llegar a la intro y a una partida.

#### Scenario: Corrida sin ventana
- **WHEN** se corre sin ventana 3000 cuadros con jugadas inventadas (`--auto`)
- **THEN** termina sin error, las capturas muestran la intro y una pelea, y `--stats` informa cuánto pasó por el intérprete

### Requirement: En el launcher de la colección
El launcher de la colección SHALL listar Hyper Street Fighter II, encontrar el set en la carpeta de ROMs y abrirlo con Play o con `--play hsf2`.

#### Scenario: Primera vez
- **WHEN** se abre el launcher con `hsf2j.zip` y `hsf2.zip` en la carpeta `ROM` junto a él
- **THEN** Hyper Street Fighter II aparece entre los juegos encontrados y Play lo abre

### Requirement: Sin datos del juego en la repo
El cambio SHALL agregar a la repo solo la configuración y la documentación del juego, nunca archivos del set ni nada derivado de ellos.

#### Scenario: Archivos rastreados
- **WHEN** se listan los archivos rastreados de la rama
- **THEN** ninguno está bajo `rom/`, `work/` ni `build/`
