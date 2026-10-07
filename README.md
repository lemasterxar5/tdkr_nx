# tdkr_nx — The Dark Knight Rises (Gameloft) para Switch

Wrapper de 32 bits (AArch32) sobre el runtime **android32** que corre la
versión Android de **The Dark Knight Rises** en Nintendo Switch. No incluye
código ni datos del juego: usa tu APK y tus datos.

## Versión requerida

Esta build está hecha y probada contra:

* **The Dark Knight Rises 1.1.6** (`com.gameloft.android.AMAZ.GloftKRAS`,
  versionCode 116, `lib/armeabi-v7a/libKRAS.so`). El APK se reconoce por su
  contenido, no por el nombre.
* Los datos extraídos (`data/`, `textures/`, `welcome/`… ≈ 1.9 GB). También
  acepta los OBB (`datakrhm.obb`, `patchkrhm.obb` opcional, o
  `main.<n>.<paquete>.obb` / `patch.<n>.<paquete>.obb`).

Otras versiones no están probadas.

## Carpeta en la SD

```
sdmc:/switch/tdkr_nx/
├── tdkr_nx.nro          ← el launcher (forwarder con portada de Batman)
├── <cualquier nombre>.apk
├── obb/                 ← si tienes OBB (si no, no la crees)
└── data/files/          ← datos extraídos
    ├── data/ textures/ welcome/ ...
```

`config.ini`, `debug.log`, `libKRAS.so` (desempaquetado) y `classes.txt` se
generan solos al arrancar. Si cambias el nombre de la carpeta, cambia
`PORT_NAME` en `source/port_config.h`.

Copia `tdkr_nx.nro` a `sdmc:/switch/tdkr_nx/`, instala el forwarder desde
sphaira (Homebrew > The Dark Knight Rises > Install Forwarder) y lanza el
icono.

## Controles (Joy-Cons)

El juego es táctil (joystick virtual izquierda, botones derecha, swipe para
cámara). Los Joy-Cons lo emulan con dedos virtuales compartidos con la
pantalla táctil (ids 0-15), más el modo gamepad nativo del motor
(Xperia con slide abierto + MOGA anunciados: las teclas van por sus tablas
y el stick derecho cae en sus variables).

| Joy-Con | Acción |
| --- | --- |
| Stick izquierdo o cruceta | arrastra el joystick virtual (`touch_joy_*`, 170/540 r90) y flechas nativas 8-dir |
| Stick derecho | cámara nativa del motor (Y invertible con `gamepad_invert_y`); con `gamepad_native = false`, swipe |
| A / L | salto (`touch_btn_jump_*` 1030/610); A además confirma menús (tecla 23) |
| B / R / click der. | ataque (`touch_btn_attack_*` 1150/550) |
| X / ZR | contraataque (`touch_btn_counter_*` 1150/420) |
| Y / ZL / click izq. | interactuar / gar grapnel (`touch_btn_use_*` 1030/480) |
| Cruceta | flechas en menús; `+` = Menu, `-` = Back |
| `Minus+R3` | conmuta en caliente nativo (gameplay) / crudo (tienda) |
| Pantalla táctil | sigue activa a la vez (conducción con slider, hacks, puertas) |

Las posiciones están en píxeles 1280x720 en `[controls]` de `config.ini`.
Los botones del juego cambian con el contexto: si un toque falla, ajusta su
coordenada. `gamepad_touch = false` deja solo táctil + teclas.

## Ajustes (`config.ini`)

* `[display] resolution`: 720, 1080 o auto. `frame_rate`: 60 o 30 (en
  portátil 30 rinde mejor). `show_fps`: línea `[fps]` cada 2 s (default on).
* `[performance] boost_cpu_when_loading`: `false` (relojes stock: el boost a
  1785 MHz en cargas está apagado; GPU/RAM siempre stock).
* `[controls]`: todo lo de la tabla de arriba.
* `[debug] log_java_calls`: cada llamada Java (lento; solo para reportes).

## Archivos del port (`source/`)

| archivo | qué hace |
| --- | --- |
| `port_config.h` | nombre, paquete, rol del APK, región, readahead/buffer I/O |
| `tdkr_main.c` | setup, carga de `libKRAS.so`, orden `init`/`nativeInit`/`setPaths`, bucle `step`+swap, FPS |
| `tdkr_java.c` | el «Java» del motor + anuncio del mando nativo |
| `tdkr_gl.c` | el EGL/GLSurfaceView que pide Java |
| `tdkr_audio.c` | `AudioTrack` sobre audout |
| `tdkr_input.c` + `touchmap.h/.c` | táctil + mapeo reutilizable mando→toques/teclas |
| `tdkr_obb.c` | OBB por rol + `obb/`→`data/files/` sin OBB |
| `tdkr_libc.c` | `rewind`, `wcscmp`, `wcscpy` |
| `tdkr_config.c` | opciones de `config.ini` (migraciones incluidas) |
| `imports.c` | generado: `python3 runtime/tools/gen_imports.py` |

## Estado

* Menú, mundo abierto (Gotham), audio, guardado y táctil funcionando; 40-56
  fps en gameplay tras la carga. La carga del menú tarda minutos (streaming
  de ~200 MB por SD a relojes stock): es normal, déjalo terminar.
* Sin servicios online (Gameloft Live, tienda, IAB responden vacío).
* Si algo falla, manda `debug.log` (y `crash.log` si se cerró solo). Para
  reportes de input deja `log_java_calls = true` y anota qué botón no responde
  y dónde sale en pantalla.

## Compilar

```sh
./build.sh              # tdkr_nx.nsp + tdkr_nx.build  (toolchain AArch32, Docker)
./launcher/build.sh     # tdkr_nx.nro                  (devkitA64)
```

Necesitas Docker (imagen `ghcr.io/vita2hos/devcontainer/vita2hos`), libnx32
(`DCR_LIBNX32` o `../libnx32/prefix`, ver `runtime/tools/docker_build.sh`) y
mesa32 (`portlibs32/`). Para recompilar limpio: `./build.sh clean`.

## Créditos

* The Dark Knight Rises: Gameloft (el juego; no incluido).
* Runtime android32, libnx32 y toolchain: aks796, vita2hos, devkitPro.
* Mesa 3D, miniz: sus autores (ver sus licencias).
* Cargador `.so` original: Andy Nguyen (TheOfficialFloW), fgsfds.
* Patrones de mapeo/input estudiados de los ports Ducktales-NX, Sonic
  All-Stars Racing, TASM2, Modern Combat 3 y Asphalt 8 (Thorhax, aks796,
  Rinnegatamante).

## Aviso

Proyecto no oficial de fans, sin afiliación con Nintendo ni Gameloft. No
incluye código ni datos del juego: necesitas tu propio APK y tus datos.
