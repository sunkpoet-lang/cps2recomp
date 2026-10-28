# cps-tunel

El túnel de red para jugar en línea sin abrir puertos (change `ki-online-tunel`). Lleva el UDP de un juego con netplay
propio (UltraCombo, para Killer Instinct) por una conexión [Iroh](https://github.com/n0-computer/iroh): directa entre
las dos PCs cuando se puede, o por un relay público de n0 cuando no. El juego juega contra `127.0.0.1` como en una LAN.

```
cps-tunel host [--game-port 7845]        CODE <código>; cada invitado llega al juego desde un puerto local propio
cps-tunel join <código> [--listen N]     LISTEN <puerto>; el juego se une a 127.0.0.1:<puerto>
los dos: --parent PID (termina con ese proceso), --relay-only (nunca directo, para pruebas)
```

Eventos en stdout, uno por línea: `CODE`, `LISTEN`, `CONNECTED [n]`, `PATH [n] direct|relay`, `PING [n] <ms>`,
`TOOBIG [n] <cuántos>`, `CLOSED [n]`, `ERROR [n] <mensaje>`. En el anfitrión llevan el número de invitado.

El código de sala (`src/code.rs`): versión, clave del anfitrión, URL de su relay y una suma de control, en base32 sin
relleno, 112 caracteres. Tolera espacios, saltos de línea y minúsculas.

## Compilar

`tools/tunel.sh [CARPETA]` (Rust estable con rustup; en Windows, la toolchain MSVC con las Build Tools de Visual
Studio). Deja el programa en `build/tunel/` y lo copia a la carpeta (por omisión `build/collection`). Las pruebas del
código: `CARGO_TARGET_DIR=build/tunel cargo test --release --manifest-path tunel/Cargo.toml`.

## Mediciones (2026-10-27, una PC en México, Windows 10)

| Prueba | Resultado |
|---|---|
| KI sin ventana por el túnel, directo, 2,400 cuadros | el mismo hash en los dos lados (`21af707d808c78fc`), 0 paquetes demasiado grandes |
| Lo mismo con un tercer UltraCombo como invitado de más | sin cambios: UltraCombo atiende solo a su primer rival |
| Lo mismo solo por relay (`--relay-only`, retraso 4) | el mismo hash; el relay fue `use1-1.relay.n0.iroh.link` (este de EE. UU.) |
| Ping por relay | túnel: mediana 141 ms (138–505); UltraCombo: 153–160 ms |
| Ping directo en la misma PC | 0–1 ms |
| Tres invitados con un eco UDP | cada uno recibe solo sus respuestas; el eco ve tres puertos de origen |
| El anfitrión se cae | cada invitado informa `CLOSED` en 9 s (keep-alive 1 s, inactividad 8 s) |

El ping por relay de la prueba es PC → relay → la misma PC: el peor caso, que solo se usa cuando el camino directo no
se puede abrir. Entre dos casas en México por relay se espera algo parecido (las dos van al relay del este de EE. UU.),
jugable con retraso 3–4 y rollback. Falta medirlo con un amigo en otra red (tarea 6.3).
