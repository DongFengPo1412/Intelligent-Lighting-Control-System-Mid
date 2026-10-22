# Mid setup and operation

[简体中文](setup.zh-CN.md) · **English** · [日本語](setup.ja.md) · [Project overview](../README_EN.md)

[Structure](#structure) · [Hardware](#hardware) · [WROOM](#wroom) · [S3](#s3) · [Jetson](#jetson) · [Operation checks](#operation) · [Troubleshooting](#troubleshooting)

Fourteen offline protocol/game cases and the complete WROOM build pass. S3 receives source inspection only. No hardware, training or end-to-end test occurred; historical demonstrations do not validate the maintenance revision.

<a id="structure"></a>

## Repository structure

```text
.
├── esp32-s3r16n8-idf/
│   ├── CMakeLists.txt
│   ├── sdkconfig.defaults
│   ├── partitions/
│   └── main/
│       ├── main.cc
│       ├── application.cc
│       ├── mcp_server.cc
│       ├── idf_component.yml
│       ├── audio/
│       └── boards/
├── esp32-wroom-32-arduino/
│   ├── esp32-wroom-32-arduino.ino
│   ├── Config.h
│   ├── Config.example.h
│   ├── MatrixLayout.h
│   ├── DisplayManager.h
│   ├── SpectrumManager.h
│   ├── GameEngine.h
│   └── BlinkerManager.h
├── jetson-nano-py36/
│   ├── run_gesture.py
│   ├── run_gesture2.py
│   ├── face_system_cn.py
│   ├── face_system.py
│   ├── face_pyserial.py
│   └── faces/
└── docs/
    ├── images/
    ├── technical-notes.{zh-CN,en,ja}.md
    └── setup.{zh-CN,en,ja}.md
```

Braces denote three language files. `Config.private.h`, `sdkconfig`, `dependencies.lock`, `managed_components`, and builds are local/ignored and cannot be assumed in a clean clone. Face directories contain only placeholders.

<a id="hardware"></a>

## Hardware and connections

| Path | Source pins/settings | Checks |
| :--- | :--- | :--- |
| S3 → WROOM | S3 GPIO17 TX → WROOM GPIO16 RX; 115200 8N1. | `main.cc` and `Config.h`. |
| UART | Connect S3 GPIO17 TX to WROOM GPIO16 RX with signal ground. Reverse UART is unused: WROOM TX is -1 and S3 does not assign GPIO18 to RX. GPIO18 remains available to the lamp controller; remove the old WROOM-to-S3 reverse wire. Confirm the actual board wiring before operation. | 115200 8N1 |
| Jetson → WROOM | WROOM GPIO25 RX; config comments specify Jetson J41 pin8 TX, reverse pin10 RX to WROOM GPIO26. | Confirm Nano carrier, device node, and signal levels; share signal ground. |
| Matrix | WROOM GPIO4 → DIN; WS2812B, GRB, 256 LEDs, initial brightness 30. | Current mapping: one 16×16 row-serpentine panel, front-view top-left DIN. Historical four-panel wiring differs. |
| Audio/display | Selected S3 `boards/.../config.h`. | Directory names do not establish the module part number. Identify microphone, amplifier/speaker, optional OLED, and pins. |
| Power | Local course guide records USB-powered WROOM with VIN to matrix V+. | Historical wiring is not a current-capacity measurement. FastLED 5 V/1200 mA is only a software budget. |

Power devices according to their actual circuits and share the signal reference ground. Do not parallel independent 5 V outputs without a verified power design. No complete verifiable schematic, PCB, or rated 3 A supply evidence was located; electrical isolation is not claimed. Verify supply and logic levels against the actual boards.

<a id="wroom"></a>

## WROOM build configuration

The WROOM build passes with Arduino ESP32 3.3.8, FastLED 3.9.15, Blinker 0.3.10230510 and ArduinoJson 7.4.3: application 1348101 bytes, static RAM 60588 bytes. It uses a 4 MiB Flash configuration and the official `min_spiffs` partition (1966080-byte application limit), without verifying physical Flash capacity. The default app partition is too small; confirm hardware and partitioning before upload. FastLED 3.10.5 triggered a compiler internal error here; 3.9.15 passed in isolation without changing global libraries.

Keep [Config.h](../esp32-wroom-32-arduino/Config.h). Create a same-directory `Config.private.h`, excluded by `.gitignore`:

```cpp
#pragma once
const char auth[] = "YOUR_BLINKER_AUTH";
const char ssid[] = "YOUR_WIFI_SSID";
const char pswd[] = "YOUR_WIFI_PASSWORD";
```

Do not overwrite `Config.h`: the old `Config.example.h` lacks current mapping/power definitions. Open the [main sketch](../esp32-wroom-32-arduino/esp32-wroom-32-arduino.ino), confirm all sibling headers, then compile. First verify corners and alternating rows at low brightness, then game/spectrum orientation. Initial Wi-Fi attempts last up to 15 s; failure continues offline, with Blinker initialized if connection later succeeds.

Configure Blinker buttons with exact keys `W`, `S`, `A`, `D`, `E`, `F`. No interface export or exact app version is archived; verify callbacks and serial logs rather than assuming a generic template matches.

<a id="s3"></a>

## S3 toolchain and board selection

The current [manifest](../esp32-s3r16n8-idf/main/idf_component.yml) requires ESP-IDF **>=5.5.2**. Root CMake's 2.2.4 is the XiaoZhi project version, not IDF. Record the actual qualifying toolchain. Building also requires Component Manager dependencies, ESP-DSP, speech assets, and components for the selected board.

From the repository root in an activated ESP-IDF terminal:

```powershell
cd esp32-s3r16n8-idf
idf.py --version
idf.py set-target esp32s3
idf.py menuconfig
idf.py build
idf.py -p COMx flash monitor
```

Replace `COMx` with the actual port. `set-target` regenerates local configuration; back up existing configuration first. Select actual board, Flash/PSRAM, partitions, audio, language, and wake assets in menuconfig. The local snapshot uses `BREAD_COMPACT_WIFI`, 16 MB Flash, Octal PSRAM, and SSD1306 128×32. These choices do not identify every prototype or prove a commercial chip variant.

That board maps microphone WS/SCK/DIN=4/5/6, speaker DOUT/BCLK/LRCK=7/15/16, and OLED SDA/SCL=41/42. These are S3 pins, distinct from equal-numbered WROOM pins. Resolve GPIO18 lamp/UART RX reuse first. For XiaoZhi Wi-Fi provisioning, activation, and server tool setup, see the [upstream guide](../esp32-s3r16n8-idf/README.md) and [protocol notes](../esp32-s3r16n8-idf/docs/mcp-protocol.md). Server credentials, model services, and media services are not supplied.

<a id="jetson"></a>

## Jetson environment and launch

The `py36` directory records a historical environment, not assurance that current dependencies install from PyPI under Python 3.6. Desktop materials include cp36/aarch64 packages for MediaPipe 0.8/0.8.4, NumPy 1.19.4, and OpenCV contrib 4.5.2.52. They are not distributed here, and no complete environment lock exists. Match JetPack, system Python, and architecture.

Required APIs include `cv2.face`, Haar cascade data, `mp.solutions.hands`, NumPy, and pyserial. Check the target environment first; install compatible packages available for that environment rather than mixing multiple OpenCV wheels:

```bash
python3 -c "import cv2, mediapipe as mp, numpy, serial; print(cv2.__version__); print(cv2.face.LBPHFaceRecognizer_create); print(mp.solutions.hands)"
ls -l /dev/ttyTHS1
systemctl status nvgetty
```

Stop/disable `nvgetty` only if that service occupies the intended J41 UART. Configure group-based permissions and log in again; world-writable `chmod 666` is not the default procedure. Device names vary across carriers. With exclusive port access, test the actual node:

```bash
stty -F /dev/ttyTHS1 115200 raw -echo
printf '1\n' > /dev/ttyTHS1
cd jetson-nano-py36
python3 run_gesture2.py
```

The command requests red; it is not a hardware ACK. Confirm WROOM logs and the physical display. Stop gestures before running `python3 face_system_cn.py`. Both use GUI windows: Esc exits gestures; S enrolls, T retrains, and Q exits faces. Only face directories named `ylk`/`zzc` trigger 1/2; other names can be recognized without triggering these modes. After S, enter a name in the terminal and press T after saving to update the model. Keep face images local.

<a id="operation"></a>

## Per-path checks

1. **Matrix/USB:** at 115200 on WROOM, send newline-terminated `1`, `2`, `3`, `0`; observe red, blue, green, off, and coordinate orientation.
2. **Phone games:** E enters Snake; W/S/A/D steer. F enters Tetris; W rotates, S steps down, A/D move sideways. Observe two-digit scores after game over. Repeating the active mode does not reinitialize it.
3. **S3 speech:** verify the online session, then the actual `self.device.set_light_mode` call and UART output. A tool success return is not display confirmation.
4. **Playback spectrum:** obtain playable server audio, enable music analysis, and confirm `15` plus `f,` packets with seventeen integers. Serial 15 alone creates no FFT data. Correct the short silence frame before accepting silence/exit behavior.
5. **Vision:** open hand 5, victory 6, fist 7; enrolled `ylk` red, `zzc` blue. Separately check target loss, person changes, serial failure, and competing inputs.

Record board, firmware commit, panel revision, toolchain, inputs, logs, and observations. This is a checklist to execute, not a report of tests passed in this audit.

<a id="troubleshooting"></a>

## Troubleshooting

| Symptom | What to inspect |
| :--- | :--- |
| Missing macros/build failure after copying configuration | Restore the correct `Config.h` content; use only credentials in the private file, not the old example replacement. |
| Tiled/reversed image | Distinguish historical four-panel and current single-panel layouts; check DIN and `MatrixLayout.h`. |
| Frozen spectrum or failed music exit | Check valid frames, silence field count, 1.2 s guard, and S3 flag; no frame-independent decay exists. |
| No faces or missing `cv2.face` | Supply local images and a contrib build; verify 100×100 grayscale crops and retraining. |
| Camera/UART busy | Run face/gesture separately; inspect camera 0, service ownership, permissions, and shared ground. |
| Voice answers without light change | Trace server result, MCP arguments, UART output, WROOM receipt, and suppression logic separately. |

The 2026-10 revision sends beat plus sixteen bands in silence frames and validates spectrum field count/ranges before updating. After 500 ms without a valid frame, decay continues every 30 ms; zero input no longer forces a lit bottom row. Serial reception uses a fixed 96-byte buffer, drops oversized frames through the delimiter, permits modes 0–16 and 19–22, and requires CR/LF from every sender. Explicit valid mode commands, including 0, can exit music immediately instead of being discarded by the old guard.

Connect S3 GPIO17 TX to WROOM GPIO16 RX with signal ground. Reverse UART is unused: WROOM TX is -1 and S3 does not assign GPIO18 to RX. GPIO18 remains available to the lamp controller; remove the old WROOM-to-S3 reverse wire. Confirm the actual board wiring before operation.

The WROOM build passes with Arduino ESP32 3.3.8, FastLED 3.9.15, Blinker 0.3.10230510 and ArduinoJson 7.4.3: application 1348101 bytes, static RAM 60588 bytes. It uses a 4 MiB Flash configuration and the official `min_spiffs` partition (1966080-byte application limit), without verifying physical Flash capacity. The default app partition is too small; confirm hardware and partitioning before upload. FastLED 3.10.5 triggered a compiler internal error here; 3.9.15 passed in isolation without changing global libraries.

Set `$FastLedDir` to an isolated official FastLED 3.9.15 checkout and `$BuildDir` to an external build directory, both absolute paths; install the platform/other libraries listed above. The command only compiles. Confirm actual Flash capacity before upload.

```powershell
arduino-cli compile --fqbn "esp32:esp32:esp32:FlashSize=4M,PartitionScheme=min_spiffs" --library "$FastLedDir" --build-path "$BuildDir" esp32-wroom-32-arduino
```
