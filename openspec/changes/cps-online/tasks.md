## 1. GekkoNet en el port de CPS

- [x] 1.1 GekkoNet en `CMakeLists.txt` (tarball fijado con su SHA-256, sin asio, estático) y ws2_32; ST y CE compilan (el cuadro sin conexión solo se separó en `GatherInput`; sus hashes no se compararon con la versión anterior)
- [x] 1.2 `port/online.{h,cpp}` desde `neogeo_online.cpp`: sesión, adaptador UDP, saludo con juego, ROMs, versión y ajustes
- [x] 1.3 `port/main.cpp`: `--host`, `--join`, `--delay`, `--stress`; el cuadro de la sesión (máquina y fuentes del juego); lo que se apaga en línea; el bucle sin ventana
- [x] 1.4 `THIRD_PARTY_NOTICES.md`: GekkoNet y zpp_bits

## 2. Pruebas sin ventana

- [x] 2.1 Estrés de 6000 cuadros con juego al azar en ST y en CE, sin diferencias (la detección de una diferencia plantada se probó con el mismo código en KOF '97); ST también con pantalla ancha
- [x] 2.2 Anfitrión e invitado en una PC, con juego al azar distinto en cada lado: mismo estado al final

## 3. La ventana y el launcher

- [x] 3.1 `frontend_sdl.cpp` en línea: espera, cuadros de la sesión, ping y rollback en la esquina, avisos
- [x] 3.2 La tarjeta ONLINE de ST y CE en el launcher; ST, CE y el launcher nuevos en la carpeta de prueba
- [ ] 3.3 El usuario juega ST y CE en línea con un amigo
