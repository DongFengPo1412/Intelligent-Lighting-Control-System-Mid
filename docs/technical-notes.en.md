# Mid technical notes

[简体中文](technical-notes.zh-CN.md) · **English** · [日本語](technical-notes.ja.md) · [Project overview](../README_EN.md)

[Source and versions](#source-map) · [Voice tools](#speech) · [Audio algorithms](#audio) · [Display and games](#display-games) · [Vision](#vision) · [Communication](#communication) · [Implementation scope](#implementation-status)

These notes primarily follow current repository source. Course demonstrations, desktop copies, and slides establish historical context; features from different versions are not combined into an assumed validated system.

<a id="source-map"></a>

## Modules, files, and versions

| Module | Main files | Confirmed responsibility |
| :--- | :--- | :--- |
| S3 startup/transport | [main.cc](../esp32-s3r16n8-idf/main/main.cc), [application.cc](../esp32-s3r16n8-idf/main/application.cc) | UART1 initialization, XiaoZhi sessions/state machine, network audio, MCP dispatch. |
| Voice tool | [mcp_server.cc](../esp32-s3r16n8-idf/main/mcp_server.cc) | Registers `self.device.set_light_mode`, schedules mode numbers, toggles music analysis. |
| Audio processing | [audio_service.cc](../esp32-s3r16n8-idf/main/audio/audio_service.cc), [audio_service.h](../esp32-s3r16n8-idf/main/audio/audio_service.h) | Input/output/Opus tasks and queues; FFT and serial transmission from playback PCM. |
| WROOM entry | [Main sketch](../esp32-wroom-32-arduino/esp32-wroom-32-arduino.ino) | Wi-Fi/Blinker startup, three serial inputs, mode initialization, display updates. |
| Display | [MatrixLayout.h](../esp32-wroom-32-arduino/MatrixLayout.h), [DisplayManager.h](../esp32-wroom-32-arduino/DisplayManager.h) | Shared serpentine mapping, pixels, glyphs, faces, display presets 1–14. |
| Spectrum renderer | [SpectrumManager.h](../esp32-wroom-32-arduino/SpectrumManager.h) | Seventeen-integer parsing, column decay, bottom-up drawing, beat-color overlay. |
| Games/phone | [GameEngine.h](../esp32-wroom-32-arduino/GameEngine.h), [BlinkerManager.h](../esp32-wroom-32-arduino/BlinkerManager.h) | Snake, Tetris, two-digit scores, six button callbacks. |
| Vision | [run_gesture2.py](../jetson-nano-py36/run_gesture2.py), [face_system_cn.py](../jetson-nano-py36/face_system_cn.py) | Separate gesture and face-enrollment programs emitting UART mode numbers. |

The audit baseline is local commit `f9419ba`, following the earlier remote `4dcabf9`. It changes panel mapping and adds `MatrixLayout.h`; it was initially local-only and has now been published with this documentation following owner approval. It remains unbuilt, unflashed, and hardware-unvalidated. Course photographs and the desktop `LightSystem3` copy use four 8×8 panels. This documentation describes the new mapping while identifying historical demonstrations separately.

All five desktop `py36` scripts are byte-identical to the repository. WROOM files in older `LightSystem1/3` copies differ and are not used to overwrite this version. The S3 tree contains many upstream XiaoZhi boards/examples; their presence does not establish support on this prototype. Nested upstream READMEs do not define this project's complete deployment conditions.

<a id="speech"></a>

## Online speech, MCP, and state

`app_main → Application::Initialize/Run` starts the application. Input tasks feed microphone data to wake/audio processing and Opus encoding; network services return audio and MCP messages. Playback consumes decoded PCM. A complete offline-reproducible server for recognition, dialogue, synthesis, and music is not distributed here.

The actual tool is `self.device.set_light_mode`, with integer `mode` in 0–22. Both 15 and 16 become WROOM mode 15; `Schedule` sends the newline-terminated number, and the tool sets `is_music_mode_`. The music branch returns instructions requesting server-side `%search_music`/`%play_music`; it is not a local music downloader and does not guarantee arbitrary song availability. Other modes disable S3 music analysis.

WROOM mode 16 displays a yellow smile, but no automatic `16` transmission was found in the S3 wake path. Automatic matrix wake feedback is therefore not established. Arguments 17/18 are accepted by the tool but have no display implementation. Commented A2DP/`play_online_music` code is not a working Bluetooth speaker feature.

On `tts stop`, non-manual sessions return to Listening; closing the audio channel returns to Idle. This client state flow is not permanent offline continuous recognition. Speech SDK, Opus, and MCP foundations are upstream work; project extensions mainly connect mode tools and playback analysis.

<a id="audio"></a>

## FFT of playback audio and visualization

### Source and windows

FFT runs in `AudioOutputTask` after `codec_->OutputData(task->pcm)`, using the same PCM. `OpusCodecTask` decodes and, if needed, resamples to the codec output rate. The local, untracked `sdkconfig` selects `BREAD_COMPACT_WIFI`, whose configuration uses 16 kHz input and 24 kHz output. Other board choices require rechecking the output rate.

Analysis requires music mode and available PCM. N=512; ESP-DSP generates a Hann window and performs a complex radix-2 FFT with zero imaginary input, followed by bit reversal and magnitudes. For int16 PCM s[n]:

```math
u[n]=s[n]/32768,\quad w[n]=\tfrac12\left(1-\cos\frac{2\pi n}{N-1}\right),\quad
X[k]=\sum_{n=0}^{N-1}u[n]w[n]e^{-j2\pi kn/N}
```

RMS is computed on **unwindowed** u[n]; RMS<0.005 enters the silence branch. Accumulation beyond 1536 samples retains only the latest 1024, while normal processing consumes 512 at a time. Windows do not overlap, and not every sample is guaranteed analysis. At 24 kHz, the theoretical window duration is 21.33 ms and bin spacing is 46.875 Hz; neither is measured compute latency or a guaranteed packet rate.

### Bands, amplitude, and beat rule

Edges are `[1,2,3,4,5,7,9,12,16,22,30,41,56,76,103,140,200]`. Each half-open interval produces the **mean magnitude**. DC and bins 200–255 are omitted. This is not a standard Mel/Bark filter bank or compression of all 256 bins. At 24 kHz, used bin centers span approximately 46.875–9328.125 Hz.

For band mean a_i and shared peak tracker P, initialized to 0.01:

```math
P_t=\max(0.98P_{t-1},\max_i a_i,0.01),\qquad
v_i=\left\lfloor15\frac{\ln(1+10a_i/P_t)}{\ln11}\right\rfloor
```

Integer values at indices 13–15 are then halved and clamped to 0–15. The multiplier for indices >=10 is 1.0, not extra gain. Low-frequency quantity L is the **sum of magnitudes**, not squared energy, at bins 1–4: approximately 46.875–187.5 Hz at 24 kHz.

```math
\mathrm{beat}_t=[L_t>1.3E_{t-1}]\land[L_t>0.15]\land[\mathrm{RMS}>0.015],\qquad
E_t=0.6L_t+0.4E_{t-1}
```

This is a heuristic low-frequency transient flag, not an evaluated beat/BPM/drum classifier. The previous 1.45 ratio, RMS>0.08, and 0–120 Hz descriptions do not match the source.

### WROOM columns

A valid frame requires beat plus sixteen bands after `f,`: seventeen integers. If column 0 is <=1, it becomes column 1 plus 1; beat=1 forces column 0 to 15. The display therefore includes visual compensation and is not a calibrated acoustic measurement.

Rises immediately follow input. Falls apply a fixed-plus-proportional decrement **per packet render**:

```math
h_i'=\begin{cases}b_i,&b_i\ge h_i\\h_i-(0.4+0.05h_i),&b_i<h_i\end{cases}
```

In the falling branch, if h_i'<b_i+0.2 and b_i<=2, the result becomes b_i, not necessarily zero, then is bounded below by zero. There is no velocity state or time step: this is empirical decay smoothing rather than gravity/air-resistance simulation. Drawing uses `y=0..int(h_i)` and physical row `15-y`, so even zero draws a bottom pixel. Beats add `CRGB(0,25,25)`, a cyan component rather than white flashing.

<a id="display-games"></a>

## Coordinates, modes, and games

### Single-panel serpentine mapping

`MatrixLayout::PixelIndex` returns -1 outside the panel and is shared by ordinary drawing and spectra. The front-view origin is top-left, with even rows left-to-right and odd rows reversed:

```math
i(x,y)=16y+\begin{cases}x,&y\bmod2=0\\15-x,&y\bmod2=1\end{cases},\qquad 0\le x,y<16
```

Examples: (0,0)→0, (15,0)→15, (0,1)→31, (15,1)→16. This does not match the four-tile wiring in course photographs or validate that historical wiring. The implementation uses no lookup table, but no nanosecond benchmark exists.

### Actual modes

| ID | Action | Entry and scope |
| :--- | :--- | :--- |
| 0 | Clear/standby | Serial or voice tool; S3 reset suppression may temporarily reject it. |
| 1/2/3 | Red/blue/green | Serial/tool; faces send only 1/2. |
| 4/5/6/7 | Rainbow/breathing/meteor/stars | Gestures target 5/6/7; effects use FastLED helpers. |
| 8/9/10 | Color wash/random rain/radial ripple | 8 uses uniform HSV, 9 random pixels; only 10 computes radial distance. |
| 11/12/13/14 | Smile/sad/neutral/alternating face | Circular outline and fixed eye/mouth pixels; 14 alternates smile/sad with hue. |
| 15 | Spectrum | Refreshed by spectrum packets; sending 15 alone creates no audio. |
| 16 | Yellow smile | Defined on WROOM; tool argument 16 becomes 15 on S3. |
| 19/20 | Snake/score | E or tool 19; self-collision enters 20. |
| 21/22 | Tetris/score | F or tool 21; spawn collision enters 22. |
| 17/18, others | No defined effect | Not completed features; receiver lacks a strict mode whitelist. |

### Game mechanics and limits

Snake uses x/y arrays of length 256, starts at length 3, and updates using `millis()` with a 450 ms initial interval. Food adds one point; interval becomes `max(80,450-15*score)`. Head coordinates wrap modulo 16; body collision ends play. Direction callbacks reject an immediate reversal relative to current direction, but multiple inputs before one movement tick need testing. Food placement does not exclude the body, and growth does not explicitly copy the old tail into the new slot. Basic's food-exclusion implementation must not be attributed to Mid.

Tetris stores a 16×16 array and uses x=3..12 as a ten-column playfield with side walls. Seven shapes have four stored rotations. Collision checks horizontal bounds, floor, and settled cells. Initial drop interval is 800 ms; each cleared row adds one point and subtracts 30 ms, down to 100 ms. A cleared row is rechecked after shifting. W rotates, S moves down one cell, and A/D move sideways. Wall kicks, hard drop, and rotation interpolation are absent.

`drawGameScore` renders two glyphs, effectively the last two score digits. Initialization runs only when `currentMode != lastMode` on entering 19/21; sending the same active mode again does not restart a game.

<a id="vision"></a>

## Jetson vision events

![Face and victory-gesture scenes in the course demonstration](images/demo_edge_vision.png)

These are interaction scenes, without recognition-window or timing evidence; they do not establish accuracy, algorithm output, or 30 FPS throughput.

- [run_gesture.py](../jetson-nano-py36/run_gesture.py): MediaPipe Hands for one hand; compares thumb x and other fingertips/joints in y, emitting finger counts 0–5.
- `run_gesture2.py`: adds a 0.02 y threshold for four fingers; victory takes priority as 6, fist as 7, open hand as 5, and other poses still emit counts 1–4. Rules depend on orientation/mirroring; no rotation-invariant classifier or project-trained gesture network exists. Only changed results are sent, with deduplication reset on hand loss; no temporal voting.
- `face_system_cn.py`: Haar detection, grayscale 100×100 crops, OpenCV LBPH training/prediction; full local retraining at startup and T. S saves the first detected face under an entered directory name; Q exits. `conf<80` is a distance threshold, not 80% probability. `ylk→1` means red and `zzc→2` means blue; the local guide's “2 green” is outdated wording.
- [face_system.py](../jetson-nano-py36/face_system.py): headless by default, threshold 95, an additional 0.05 s loop sleep, and no enrollment keys. It is not an equivalent substitute for the preceding script.
- [face_pyserial.py](../jetson-nano-py36/face_pyserial.py): earlier enrollment version without later uniform crop sizing; the `pickle` import and `DATA_FILE` do not implement model persistence.

There is no `recognizer.update` or saved model, so this is not online incremental learning. Only `faces/ylk/.gitkeep` and `faces/zzc/.gitkeep` are tracked, without face images or trained weights. Main scripts share a camera and `/dev/ttyTHS1` and should run separately; no process resource arbitration is implemented. MediaPipe performs inference, with no code evidence of CUDA acceleration.

<a id="communication"></a>

## Communication and mode arbitration

S3 UART1/WROOM Serial1 and Jetson/WROOM Serial2 use 115200, 8N1. A mode frame is a decimal number plus newline; spectra use `f,beat,b0,...,b15\n`. There is no checksum, sequence number, ACK, or retransmission. Reverse pins are configured, but application heartbeat/status confirmation transmission is absent.

Ordinary display submissions use `millis()-lastShow>30`. Music skips that gate and calls `FastLED.show()` per received spectrum packet. The loop services Blinker, S3, Jetson, and USB sequentially; hard real-time scheduling is not established.

| Path | Actual behavior | Limitation |
| :--- | :--- | :--- |
| S3 | CR/LF framing; mode 15 rejects other modes while the latest f stream is <1200 ms old. Zero may also be rejected for 4000 ms after a nonzero mode. | Path-specific protection; no 50 ms timeout; `toInt` can turn invalid text into zero. |
| Jetson/USB | Digit checks; parsing on CR/LF or >50 ms after the last byte. | Timeout processes a partial frame rather than validating/discarding it; no music lock or legal-mode whitelist. |
| Blinker | W/S/A/D control active games; E/F enter games. | E/F always reject in mode 15; no color picker, full effects menu, or music-exit button. |

`String.reserve(128)` reserves capacity, not a fixed maximum length. Buffers can grow and PCM accumulation uses vector; zero allocation/fragmentation and industrial reliability are unsupported claims.

With one-digit beat and bands 0–15, valid spectrum text occupies roughly 36–52 bytes. At 8N1/115200 its wire serialization is about 3.13–4.51 ms, excluding processing/queues; this is not measured end-to-end delay. FastLED's 5 V/1200 mA setting is an estimated software power budget, not measured current, a hardware current regulator, or supply rating.

<a id="implementation-status"></a>

## Validation scope, known issues, and further work

This pass statically traced calls/configuration and compared files. No hardware, firmware build, training, or end-to-end test was performed. Historical videos, photographs, and guides contextualize the course prototype without validating every function of the current revision.

| Issue | Source and impact | Status |
| :--- | :--- | :--- |
| Short silence frame | The literal in `audio_service.cc` has only sixteen integers; the receiver requires seventeen, retaining and rendering old bands. | Statically confirmed; correction and hardware test pending. |
| Unchecked band range | Parsed heights are stored directly and drive render-loop length; malformed frames can cause excessive work or repeated bottom-row drawing. | No fault-injection test. |
| Unsynchronized music exit | S3 can disable analysis while WROOM rejects the switch within 1.2 s; no automatic resend/standby timeout. | Integration test needed; decay also stops when frames stop. |
| Single-panel revision | `f9419ba` publishes an unvalidated mapping change; course images show tiled hardware. | Not built/flashed/hardware-tested. |
| Old configuration template | Replacing `Config.h` with `Config.example.h` loses current mapping and power definitions. | Create only the private credential file as documented. |
| S3 pin reuse | Local `bread-compact-wifi` config assigns GPIO18 to both UART RX and a `LampController` output. | Resolve against the actual board before testing; source unchanged in this pass. |
| Dependencies/assets | Manifest now requires IDF>=5.5.2, versus old 5.4 wording; sdkconfig, lock, and managed_components are untracked. | A clean-clone build is not guaranteed. |
| Game/vision edges | Food overlap, new tail slot, rapid turning, two-digit score, shared camera/serial resources. | Not presented as a complete stable product. |

Previous precise latency, 100% delivery, 0% accidental switching, endurance testing, electrical isolation, a rated 3 A supply, and nanosecond mapping lack reproducible records and are not claimed. Next steps are protocol/configuration fixes followed by recorded toolchain, panel version, input cases, timing, and recognition trials. Library capabilities are not project benchmarks.

Course slides list Liu, Zhang, and Yang with broad roles; Bluetooth, enclosure, and module descriptions do not fully match the current implementation. This document covers team work, with individual module authorship still requiring confirmation. Third-party framework capabilities are not individual original research.
