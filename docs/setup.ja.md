# Mid 導入と操作

[简体中文](setup.zh-CN.md) · [English](setup.en.md) · **日本語** · [プロジェクト概要](../README_JA.md)

[構成](#structure) · [ハードウェア](#hardware) · [WROOM](#wroom) · [S3](#s3) · [Jetson](#jetson) · [操作確認](#operation) · [問題の切分け](#troubleshooting)

まず[既知の実装課題](technical-notes.ja.md#implementation-status)を参照してください。以下はソースに基づく再現手順で、今回ビルド・書込み・実機試験は実施しておらず、動作保証済みの自動導入ではありません。

<a id="structure"></a>

## リポジトリ構成

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

波括弧は3言語のファイルを表します。`Config.private.h`、`sdkconfig`、`dependencies.lock`、`managed_components`、ビルドはローカル／追跡対象外で、clone直後に揃うとは限りません。顔ディレクトリはプレースホルダーのみです。

<a id="hardware"></a>

## ハードウェアと配線

| 経路 | ソースのピン・設定 | 確認事項 |
| :--- | :--- | :--- |
| S3 → WROOM | S3 GPIO17 TX → WROOM GPIO16 RX、115200 8N1。 | `main.cc`と`Config.h`。 |
| 逆向きの予約線 | WROOM GPIO17 TX → S3 GPIO18 RX。 | アプリACKなし。ローカルS3設定では18がランプ出力と競合します。 |
| Jetson → WROOM | WROOM GPIO25 RX。設定コメントはJetson J41 pin8 TX、逆向きはpin10 RXとWROOM GPIO26。 | Nanoキャリア、デバイス名、信号レベルを確認し、信号GNDを共有。 |
| パネル | WROOM GPIO4 → DIN、WS2812B、GRB、256灯、初期輝度30。 | 現行は1枚16×16蛇行で正面左上DIN。過去の4枚構成と異なります。 |
| 音声・表示 | 選択したS3の`boards/.../config.h`。 | ディレクトリ名だけでモジュール型番は確定しません。マイク、アンプ／スピーカー、必要なOLEDとピンを確認。 |
| 電源 | 授業時のガイドはWROOMをUSB給電し、VINをパネルV+へ接続。 | 過去の配線記述は容量測定ではなく、FastLEDの5 V/1200 mAはソフトウェア予算です。 |

実回路に沿って給電し、信号の基準GNDを共有します。電源構成を確認せず独立した5 V出力を並列接続しないでください。完全な回路図・PCB・3 A定格の電源根拠は確認できず、電気的絶縁設計とは主張しません。電源と論理レベルは実ボードの資料で確認します。

<a id="wroom"></a>

## WROOMのビルド設定

Arduino IDEのESP32対応とFastLED／Blinkerを用います。検証済みバージョンは固定されておらず、旧下限値は環境ロックではありません。ボード、Flash、分割、書込み速度は実物に合わせます。

[Config.h](../esp32-wroom-32-arduino/Config.h)を保持し、同じ場所に`.gitignore`で除外される`Config.private.h`を作ります：

```cpp
#pragma once
const char auth[] = "YOUR_BLINKER_AUTH";
const char ssid[] = "YOUR_WIFI_SSID";
const char pswd[] = "YOUR_WIFI_PASSWORD";
```

古い`Config.example.h`には現行変換・電力定義がないため、`Config.h`を上書きしません。[主スケッチ](../esp32-wroom-32-arduino/esp32-wroom-32-arduino.ino)と同じ場所のヘッダを確認してビルドします。まず低輝度で四隅と偶奇行を確認し、次にゲーム・スペクトルの向きを見ます。起動時のWi-Fi試行は最大15 sで、失敗時もオフライン処理を続け、後から接続した時点でBlinkerを初期化します。

Blinkerのキー名を`W`、`S`、`A`、`D`、`E`、`F`と一致させます。画面のエクスポートや正確なアプリ版は未保存なので、コールバックとログで照合します。

<a id="s3"></a>

## S3のツールチェーンとボード選択

現在の[依存宣言](../esp32-s3r16n8-idf/main/idf_component.yml)はESP-IDF **>=5.5.2**です。ルートCMakeの2.2.4は小智プロジェクト版で、IDF版ではありません。条件を満たす環境を用い、実際の版を記録します。Component Manager、ESP-DSP、音声リソース、選択ボードの部品も必要です。

ESP-IDFを有効にした端末で、リポジトリのルートから実行します：

```powershell
cd esp32-s3r16n8-idf
idf.py --version
idf.py set-target esp32s3
idf.py menuconfig
idf.py build
idf.py -p COMx flash monitor
```

`COMx`は実ポートに置換します。`set-target`はローカル設定を再生成するため、既存設定は先に保存します。menuconfigで実ボード、Flash/PSRAM、パーティション、音声、言語、ウェイク資源を選びます。現在のローカル設定は`BREAD_COMPACT_WIFI`、16MB Flash、Octal PSRAM、SSD1306 128×32ですが、全試作機への適合や商品型番の証明ではありません。

この設定のマイクWS/SCK/DINは4/5/6、スピーカーDOUT/BCLK/LRCKは7/15/16、OLED SDA/SCLは41/42です。S3のピンであり、WROOMの同番号とは別ノードです。GPIO18のランプ出力とUART RXの競合を先に解消します。小智の配網・有効化・サーバーツールは[上流ガイド](../esp32-s3r16n8-idf/README_ja.md)と[プロトコル資料](../esp32-s3r16n8-idf/docs/mcp-protocol.md)を参照してください。サーバー認証情報、モデル・メディアサービスは同梱しません。

<a id="jetson"></a>

## Jetson環境と起動

`py36`は過去の環境名で、現行依存をPython3.6のPyPIからすべて導入できる保証ではありません。デスクトップ資料にはMediaPipe0.8/0.8.4、NumPy1.19.4、OpenCV contrib4.5.2.52のcp36/aarch64パッケージがありますが、本リポジトリには配布せず、完全な環境固定もありません。JetPack、Python、アーキテクチャを合わせます。

必要なのは`cv2.face`、Haarデータ、`mp.solutions.hands`、NumPy、pyserialです。対象環境で先に確認し、その環境用の互換パッケージを用い、複数OpenCV wheelの混在を避けます：

```bash
python3 -c "import cv2, mediapipe as mp, numpy, serial; print(cv2.__version__); print(cv2.face.LBPHFaceRecognizer_create); print(mp.solutions.hands)"
ls -l /dev/ttyTHS1
systemctl status nvgetty
```

意図したJ41 UARTを`nvgetty`が占有している場合に限り停止・無効化します。デバイスのグループ権限を設定し再ログインし、`chmod 666`による全ユーザー書込みを標準手順にはしません。キャリアによりノード名は異なります。排他利用を確保して実ノードを試します：

```bash
stty -F /dev/ttyTHS1 115200 raw -echo
printf '1\n' > /dev/ttyTHS1
cd jetson-nano-py36
python3 run_gesture2.py
```

上記は赤の要求であってACKではありません。WROOMログとパネルを観察します。手のプログラム終了後に`python3 face_system_cn.py`を実行します。GUIを使い、手はEsc、顔はS登録・T再学習・Q終了です。`ylk`／`zzc`だけが1／2を送り、他の登録名は認識のみです。Sの後は端末で名前を入力し、保存後Tでモデルへ反映します。顔画像はローカルに保持します。

<a id="operation"></a>

## 経路別の操作確認

1. **パネルとUSB：** WROOMの115200端末から改行付き`1`、`2`、`3`、`0`を送り、赤・青・緑・消灯と座標方向を見ます。
2. **スマートフォン：** Eでスネーク、W/S/A/Dで方向。Fでテトリス、W回転、Sは1マス下、A/D横移動。終了時2桁得点を見ます。同じ実行モードでは再初期化しません。
3. **S3音声：** オンライン会話を確認し、実際の`self.device.set_light_mode`呼出しとUART出力を追います。ツールの成功応答だけでは点灯確認になりません。
4. **スペクトル：** サーバーから再生可能音声を受け、音楽フラグと`15`、17整数の`f,`を確認します。15だけではFFTデータは生成しません。無音フレームを修正してから無音・終了を評価します。
5. **視覚：** 開いた手5、ピース6、拳7、登録`ylk`赤、`zzc`青。目標消失、交代、UART失敗、多入力切替を別々に試します。

ボード、コミット、パネル版、ツールチェーン、入力、ログ、観察を記録します。これは今後実施する項目で、今回の試験合格一覧ではありません。

<a id="troubleshooting"></a>

## 問題の切分け

| 現象 | 確認箇所 |
| :--- | :--- |
| 設定コピー後のマクロ不足 | 正しい`Config.h`を保ち、私有ファイルに認証だけを記載。旧exampleで置換しません。 |
| 表示が分割・反転 | 過去の4枚と現行1枚を区別し、DINと`MatrixLayout.h`を照合。 |
| スペクトル停止・音楽終了失敗 | 有効フレーム、無音整数数、1.2 s保護、S3フラグ。独立した定時減衰はありません。 |
| 顔データなし・`cv2.face`なし | ローカル画像とcontrib、100×100灰度、再学習操作。 |
| カメラ／UART使用中 | 手と顔を別起動し、カメラ0、サービス、権限、共通GNDを確認。 |
| 音声応答はあるが点灯なし | サーバー結果、MCP引数、UART送出、WROOM受信、抑制処理を個別に追跡。 |
