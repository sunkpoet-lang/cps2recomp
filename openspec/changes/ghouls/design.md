# Design

## Context

Ver `proposal.md`. Lo observado en el código (rama `sf2-sonido`, commit `19fc2f8`) y en MAME (`src/mame/capcom/cps1.cpp` y `cps1_v.cpp`, master del 2026-10-26):

- **Reloj**: `Machine` usa `kCps1CpuCyclesPerLine = 768` para todo CPS-1 (12 MHz, con 15 625 líneas por segundo). MAME pone `ghouls` en `cps1_10MHz` y `sf2ce`/`sf2hf` en `cps1_12MHz`.
- **Programa** (`machine/romset.cpp`, `LoadRomSet`): cada archivo es una tira de palabras con el byte bajo primero (el `ROM_LOAD16_WORD_SWAP` de MAME). Ghouls usa dos pares `ROM_LOAD16_BYTE` (`dme_29`/`dme_30` en 0x00000, `dme_27`/`dme_28` en 0x40000) y un `ROM_LOAD16_WORD` (`dm-17.7j` en 0x80000, byte alto primero).
- **Gráficos**: los archivos van en grupos de cuatro, dos bytes de cada uno por palabra de 64 bits (`ROM_LOAD64_WORD`). Ghouls tiene cuatro de esos (`dm-05`, `dm-07`, `dm-06`, `dm-08`, 2 MB) y luego dos grupos de ocho archivos de 64 KB, uno por byte (`ROM_LOAD64_BYTE`): `09 18 13 22 11 20 15 24` en 0x200000 y `10 19 14 23 12 21 16 25` en 0x280000.
- **Mapper** (`gfx_map`, `MapTile` en `video/video.cpp`): solo lo usan las capas de fondo; **los objetos no pasan por él**. Con el S9263B de SF2 da igual, porque su tabla deja los objetos donde están. El DM620 de Ghouls no:
  - bancos de 0x8000, 0x2000 y 0x2000;
  - `scroll3` de 0x8000 a 0xBFFF al banco 1;
  - objetos de 0x2000 a 0x3FFF al banco 2;
  - todo lo demás, de 0 a 0x1FFFF, al banco 0.

  El mapper se lee de arriba abajo y gana la primera fila que coincide.
- **CPS-B-01**: los mismos registros que el `cps_b` de SF2 (`26 28 2A 2C 2E 30 02 04 08`), sin multiplicador ni ID.
- **DIP switches**: `ReadCps1` devuelve `0xFFFF` en 0x80001A-0x80001F; la placa clásica fuerza `0xDFFF` en 0x80001E (C6 encendido, en el byte alto). Valores de fábrica en MAME:
  - **Ghouls**: A = FF, B = FD (dificultad 2), C = FF (3 vidas; aquí el demo sound encendido es el bit en 1, al revés que en SF2).
  - **SF2**: A = FF, B = FC (dificultad 4), C = 9F (demo sound y continuar encendidos).
- **Sonido**: el `26.10a` de Ghouls es una ROM de 64 KB como la de SF2, con el mismo banco. No hay ROMs del OKI (MAME: "Samples (not present)").
- **El launcher** (`launcher/src/description.cpp`) usa el mismo `ParseRomList`, así que entiende lo que entienda el juego.

## Goals / Non-Goals

**Goals:**
- Que la sintaxis de `game.cfg` cubra las formas de carga de MAME que usan los CPS-1 de la primera época, sin tocar las listas que ya existen.
- Que los objetos pasen por el mapper como en la placa, y que SF2 quede idéntico.

**Non-Goals:**
- Un menú de DIP switches con nombres (los valores van fijos en `game.cfg`).
- Más relojes que 10 y 12 MHz, o relojes del Z80 por juego (todos los CPS-1 clásicos usan 3.579545 MHz).

## Decisions

### 1. `cpu_clock` en `game.cfg`
Clave nueva `cpu_clock = 10` (en MHz). Va a `Board` (`cpuClock`) y `Machine` calcula sus ciclos por línea con ella: 768 × clock / 12, es decir 640 a 10 MHz. Sin la clave queda en 12 en CPS-1, y CPS-2 no la mira.
- **Por qué en `game.cfg` y no por placa:** la misma placa A llevó 10 y 12 MHz según el juego (incluso `daimakair` es 12 MHz).

### 2. Programa: pares con `+` y palabras sin invertir con `/be`
En la lista `program`:
- `a:crc+b:crc` es un par `ROM_LOAD16_BYTE`: `a` da los bytes pares y `b` los impares.
- `a:crc/be` es un archivo de palabras con el byte alto primero (`ROM_LOAD16_WORD`).
- Todo lo demás sigue igual (byte bajo primero).

Ghouls queda así:
`program = dme_29.10h:166a58a2+dme_30.10j:7ac8407a dme_27.9h:f734b2be+dme_28.9j:03d3e714 dm-17.7j:3ea1b0f2/be`

Los trozos se cargan uno detrás de otro, como hoy.
- **Alternativa descartada:** escribir direcciones como MAME (`@0x40000`). Más general, pero innecesario mientras los trozos vayan contiguos, y obligaría a cambiar todas las listas.

### 3. Gráficos: grupos de ocho con `+`
En `graphics`, una palabra con ocho archivos unidos por `+` es un grupo entero, un byte de cada archivo por palabra de 64 bits. Las palabras sin `+` siguen formando grupos de cuatro de dos bytes. Un grupo de ocho solo puede ir cuando no queda un grupo de cuatro a medias; si no, error de `game.cfg`.
- **Por qué `+` otra vez:** es la misma idea que en el programa (archivos que se intercalan), y una sola palabra se lee como una unidad.

### 4. `RomFile` con partes
`RomFile` gana:
- `std::vector<RomFile> parts`: los archivos unidos con `+`;
- una marca de byte alto primero.

`ParseFiles` arma eso. La comprobación por CRC del juego y del launcher recorre las partes, así que ambos encuentran cada archivo por separado.

### 5. Los objetos por el mapper
`DrawObjectsCps1` pasa el número de tile de cada objeto por `MapTile` con el tipo `objects` y el mismo corrimiento que `scroll2` (tiles de 16 × 16), como hacen las capas. Un tile sin fila en el mapper no se dibuja, como en MAME. Sin `gfx_map` (Dino, CPS-2) no cambia nada.
- **SF2 queda igual** porque sus filas de objetos dejan cada tile en su lugar; se comprueba con hashes.

### 6. `dip` en `game.cfg`
Clave nueva `dip = AA BB CC` (hex, bancos A, B y C). Va a `Board` y `ReadCps1` devuelve `(valor << 8) | 0xFF` en 0x80001A, 0x80001C y 0x80001E. Sin la clave, `FF FF FF`.
- **Se quita el `0xDFFF` fijo** de la placa clásica.
- **`sf2ce` y `sf2hf`** pasan a `dip = FF FC 9F` y **`ghouls`** usa `dip = FF FD FF`.
- **Por qué un valor crudo y no nombres:** basta para los valores de fábrica; los nombres son el menú de DIP, que queda fuera.

### 7. `games/ghouls/game.cfg`
- `board = cps1-classic`, `cpu_clock = 10`, el `cps_b` de SF2.
- `gfx_map = 8000 2000 2000 | scroll3:8000-bfff:1 objects:2000-3fff:2 objects:0-1ffff:0 scroll1:0-1ffff:0 scroll2:0-1ffff:0 scroll3:0-1ffff:0`
- `samples` vacío, `players = 2`, `buttons = Fire | Jump`, `code_end = 100000`, `dip = FF FD FF`, `name = Ghouls n Ghosts` (sin apóstrofo, porque el nombre es una carpeta).

Las listas de ROMs y sus CRC son las de `ROM_START( ghouls )`.

### 8. OKI sin ROMs
Con `samples` vacío, `RomSet::samples` queda vacío. El OKI lee 0 de una ROM ausente, así que su tabla queda con inicio = fin y ninguna voz arranca. Si `ParseRomList`, `LoadRomSet` o el launcher exigen la lista, se acepta vacía solo en la placa clásica.

## Risks / Trade-offs

- **[Corrimiento de los objetos mal elegido]** → Ghouls se vería con sprites rotos y SF2 cambiaría de hashes. Se compara SF2 antes y después, y se revisan las capturas de Arthur y los enemigos.
- **[La exploración automática llega a poco código en un juego de plataformas]** → Se mira `--stats`. Si el intérprete pasa del 2 %, se siembran entradas jugando el primer nivel con `--auto` más largo o con una película grabada.
- **[El cambio de DIP en SF2 cambia el juego que el usuario ya probó]** → Es el valor de fábrica de MAME. Se le avisa en la prueba y se puede volver con una línea de `game.cfg`.
- **[Pantallas de prueba al leer DIP]** → El bit C8 (modo de prueba) queda en 1 en los dos juegos.
- **[Ghouls a 10 MHz va más lento que a 12]** → Es lo correcto: el juego tiene ralentizaciones en la placa real. Se anota.

## Migration Plan

1. Rama `ghouls` desde `sf2-sonido`.
2. `RomFile` con partes, la sintaxis nueva y la comprobación del launcher; luego `cpu_clock`, `dip` y los objetos por el mapper. Después de cada paso, HSF2 y SF2 con hashes.
3. `games/ghouls` y SF2 con su `dip`; compilar, explorar y probar sin ventana.
4. Instalar con respaldo en `_respaldo_ghouls` y prueba del usuario.
5. **Si algo falla:** los exes del respaldo y la rama `sf2-sonido` intacta.
