# Tasks

## 1. Base

- [x] 1.1 Confirmar la rama `ghouls` en `4a44c8e` y sacar hashes de referencia: SF2 CE (`--frames 6000 --auto 5:900`, con audio), Ghouls (`--frames 6000 --auto 5:600`) y HSF2 (`h_mus.txt`).
- [x] 1.2 `Machine::dip[3]` copiado de `board.dip` y leído por `ReadCps1` (decisión 3). Verificar que los tres juegos dan los mismos hashes que en 1.1.

## 2. Opciones de placa en el puerto

- [x] 2.1 Leer y validar `board_option_1`...`_12` (decisión 1) y registrar la página Board y sus opciones con `kFeatureOwn` (decisión 2), tanto al arrancar como con `--list-features`. Verificar con `--list-features` que salen las opciones con sus valores, y que una opción rota en una compilación de prueba impide arrancar con un mensaje que la nombra.
- [x] 2.2 Aplicar lo elegido a `machine->dip` antes de cada cuadro (decisión 3). Verificar que, sin elegir nada, los hashes son los de 1.1; y que con `--features board_difficulty=8` un log de lecturas de 0x80001C muestra el valor elegido.
- [x] 2.3 Verificar que un juego sin opciones (HSF2) no gana la página Board y da los mismos hashes que en 1.1.

## 3. Las opciones de cada juego

- [x] 3.1 Opciones de Ghouls en su `game.cfg` (decisión 5). Verificar sin ventana con `--features`: 5 vidas se ven en el marcador al empezar, y la dificultad y la vida extra llegan a sus bits. Anotar en cada descripción cuándo hace efecto (decisión 4).
- [x] 3.2 Opciones de SF2 CE y Hyper Fighting en sus `game.cfg` (decisión 5). Verificar sin ventana: con Free play en On la pantalla de título deja empezar sin monedas; con Continue en No, sin cuenta atrás al perder; con Demo sound en Off, la intro sale muda. Anotar cuándo hace efecto cada una.
- [x] 3.3 Corregir "difficulty 4 (normal)" a "difficulty 3 (normal)" en los README y en los comentarios de `game.cfg` de SF2 (decisión 6), y documentar la página Board en los README de los tres juegos. Verificar que `tools/audit.sh` pasa.

## 4. Launcher e instalación

- [x] 4.1 Reconstruir el launcher de la colección y los tres exes. Verificar que el launcher, con `--list-features` de cada juego, muestra la página Board (y, en su página del juego, las opciones).
- [x] 4.2 Respaldar en `D:\CPS2\CPS2Recomp\_respaldo_dip_menu` lo que se reemplace; instalar. Verificar que arrancan con un PATH mínimo.
- [x] 4.3 Prueba del usuario con ventana: cambiar desde F1 y desde el launcher la dificultad, las vidas (Ghouls) y el free play (SF2), y ver que se guardan entre sesiones. Anotar lo que reporte.
  - 2026-10-26: el usuario reporta que la página Board funciona bien.

## Workflow follow-up

- Commit en la rama `ghouls` cuando el usuario lo apruebe; sin push sin su OK.
- Actualizar la skill `cps2recomp`: el mecanismo `dip-menu` ya existe y cómo se declara una opción.
