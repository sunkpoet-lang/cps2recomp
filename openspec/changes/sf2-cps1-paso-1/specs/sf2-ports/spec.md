# Spec Delta

## Purpose

Los ports nativos de Street Fighter II: Champion Edition y Street Fighter II′: Hyper Fighting en su primer paso: qué sets aceptan, que corran traducidos a código nativo, en silencio, y que se abran desde el launcher de la colección.

## ADDED Requirements

### Requirement: Sets aceptados
El port de Champion Edition SHALL aceptar el set `sf2ce` (World 920513) y el de Hyper Fighting el set `sf2hf` (World 921209), comprobando cada archivo por CRC32, aunque el zip sea un set "merged" con clones adentro. Un archivo de otra revisión SHALL rechazarse nombrando el archivo.

#### Scenario: Zip merged
- **WHEN** la carpeta de ROMs tiene el `sf2ce.zip` merged del usuario, con 272 archivos
- **THEN** el port de Champion Edition encuentra sus 18 archivos por CRC y arranca

#### Scenario: Otra revisión
- **WHEN** se le da a Hyper Fighting un archivo de programa de otra revisión
- **THEN** no arranca y el mensaje nombra ese archivo y su CRC32

### Requirement: Corren en nativo y en silencio
Cada juego SHALL correr con su programa del 68000 traducido después de compilar y explorar, llegando a la intro, a la selección de personaje y a una pelea, sin sonido.

#### Scenario: Corrida sin ventana
- **WHEN** se corre cada juego sin ventana con jugadas inventadas durante 6000 cuadros
- **THEN** las capturas muestran la intro, la selección y una pelea, y `--stats` informa cuánto pasó por el intérprete

### Requirement: En el launcher
El launcher de la colección SHALL listar los dos juegos, encontrar sus sets en la carpeta de ROMs y abrirlos.

#### Scenario: Carpeta del usuario
- **WHEN** se abre el launcher instalado con `sf2ce.zip` y `sf2hf.zip` en la carpeta `ROM`
- **THEN** los dos juegos aparecen entre los encontrados con Play activo

### Requirement: Sin datos de los juegos en la repo
El cambio SHALL agregar a la repo solo código del toolkit, configuración y documentación, nunca archivos de los sets ni nada derivado de ellos.

#### Scenario: Auditoría
- **WHEN** se corre `tools/audit.sh` con los archivos nuevos
- **THEN** pasa, y ningún archivo rastreado está bajo `rom/`, `work/` ni `build/`
