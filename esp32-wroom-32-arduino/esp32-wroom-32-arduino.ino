#define BLINKER_WIFI
#include <WiFi.h>
#include <FastLED.h>
#include "Config.h"
#include "DisplayManager.h"
#include "GameEngine.h"
#include "BlinkerManager.h"
#include "SpectrumManager.h" // 单块 16x16 蛇形矩阵音律管理器

// --- 全局变量定义 ---
CRGB leds[NUM_LEDS];          
uint8_t gHue = 0;              
int currentMode = 0;          // 当前运行模式
int lastMode = -1;            
bool blinkerInitialized = false; // Blinker 初始化状态
// Fixed bounded input frames replace dynamically growing Strings.
// String serial1Buffer = "";    // 接收 S3 (大脑) 的指令
// String serial2Buffer = "";    // 接收 NVIDIA Jetson Nano (人脸与手势识别) 的指令 (UART2)

// --- 实例化音律随动核心 ---
SpectrumManager spectrum(&leds[0]); // 将 FastLED 的物理灯珠数组首地址挂载进去

// --- Blinker 按钮物理实例化 ---
BlinkerButton BtnW("W");
BlinkerButton BtnS("S");
BlinkerButton BtnA("A");
BlinkerButton BtnD("D");
BlinkerButton BtnE("E");
BlinkerButton BtnF("F");

// --- 游戏引擎变量定义 (WROOM-32 专用) ---
int snakeX[256], snakeY[256], snakeLen = 3;
int sDx = 1, sDy = 0;
int foodX, foodY;
int snakeScore = 0;
int snakeSpeed = 450; 
unsigned long lastSnakeUpdate = 0;
bool snakeStarted = false;

uint8_t tetrisField[16][16] = {0}; 
int pType, pRot, pX, pY;
int tetrisScore = 0;
int tetrisSpeed = 800;
unsigned long lastTetUpdate = 0;
bool tetrisStarted = false;

/**
 * @brief 统筹模式切换时的初始化工作
 */
void changeToMode(int m) {
    if (!(m>=0 && (m<=16 || (m>=19 && m<=22))))return;
    if (m==15 && currentMode!=15)spectrum.reset();
    currentMode = m;
    FastLED.clear();
    // 每次切换模式重置一次亮度，防止“呼吸极光”等模式改变了全局亮度
    FastLED.setBrightness(BRIGHTNESS); 
    Serial.printf(">>> [核心]: 模式切换 -> %d\n", m);
}

// Every sender must terminate frames with CR or LF. Oversized input is
// discarded through the next delimiter; a suffix is never parsed as a command.
void handleFrame(const char* line) {
    if (line[0]=='f' && line[1]==',') {
        if(currentMode==15)spectrum.parseSerialData(String(line));
        return;
    }
    int mode;
    if(LightingProtocol::ParseMode(line,mode) && mode!=currentMode)changeToMode(mode);
}
void handleS3Communication() {
    static LightingProtocol::Line frame;
    while(Serial1.available()>0) {const char* s=frame.Push(Serial1.read());if(s)handleFrame(s);}
}
void handleSerial2Communication() {
    static LightingProtocol::Line frame;
    while(Serial2.available()>0) {const char* s=frame.Push(Serial2.read());if(s)handleFrame(s);}
}
void handleUSBSerialCommunication() {
    static LightingProtocol::Line frame;
    while(Serial.available()>0) {const char* s=frame.Push(Serial.read());if(s)handleFrame(s);}
}

void setup() {
    // Fixed frame capacity; other libraries may still allocate.
    // Frame capacity is bounded by LightingProtocol::Line.

    // 1. 初始化与 PC 串口调试 (UART0)
    Serial.begin(115200);
    
    // 🌟【防浮空电磁屏蔽】：强制拉高串口 RX 物理引脚，防止在杜邦线未接或松动时，引脚高阻抗悬空吸收空间电磁辐射产生垃圾乱码
    pinMode(JETSON_RX, INPUT_PULLUP);
    pinMode(S3_RX, INPUT_PULLUP);
    
    // 1.5 初始化与 NVIDIA Jetson Nano 异步串口通信 (UART2，引脚定义在 Config.h 中)
    Serial2.begin(115200, SERIAL_8N1, JETSON_RX, JETSON_TX);
    
    // 2. 初始化跨板通信串口 (与 S3 对接)
    // 使用 Config.h 中定义的 S3_RX (16) 和 S3_TX (17)
    Serial1.begin(115200, SERIAL_8N1, S3_RX, S3_TX); 
    
    // 3. 联网 (Blinker 远程控制)
    WiFi.begin(ssid, pswd);
    Serial.print(">>> [系统]: 正在连接 WiFi");
    unsigned long startAttemptTime = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < 15000) {
        delay(500);
        Serial.print(".");
    }

    // 4. 初始化 FastLED 与 logic 模块
    initDisplay();    // 内部已包含 FastLED.addLeds
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n>>> [系统]: WiFi 已连接");
        initBlinker();
        blinkerInitialized = true;
    } else {
        Serial.println("\n>>> [系统]: WiFi 连接超时，跳过 Blinker 初始化，进入离线工作模式");
    }
    
    Serial.println("--- [UESTC 智能灯阵执行端: WROOM-32 已启动] ---");
}

void loop() {
    // 1. Blinker 物联网心跳
    if (blinkerInitialized) {
        Blinker.run(); 
    } else if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n>>> [系统]: WiFi 在后台已连接，正在初始化 Blinker IoT...");
        initBlinker();
        blinkerInitialized = true;
    }

    // 2. 监听 S3 (大脑) 指令
    handleS3Communication(); 

    // 3. 监听 NVIDIA Jetson Nano (人脸/手势识别) 的串口指令 (UART2)
    handleSerial2Communication();

    // 3.5 监听 PC USB 调试串口 (Serial) 的指令
    handleUSBSerialCommunication();

    // 4. 模式切换初始化调度 (仅在切换瞬间运行一次)
    if (currentMode != lastMode) {
        printModeName(currentMode); // 串口打印当前模式名称
        
        if(currentMode == 19) initSnake();
        if(currentMode == 21) initTetris();
        
        lastMode = currentMode;
    }

    // 5. 渲染引擎状态机
    switch (currentMode) {
        case 15:
            spectrum.tick(); // Decay stale bands even after input stops.
            break;
        case 19: runSnake(); break;               
        case 21: runTetris(); break;               
        case 20: drawGameScore(snakeScore); break; 
        case 22: drawGameScore(tetrisScore); break;
        // 16 号模式在 DisplayManager 对应 AI 唤醒反馈表情
        case 16: drawFace(CRGB::Yellow, 1); break; 
        default: runDisplayEffects(currentMode); break; 
    }

    // 6. 全局彩虹色相更新
    EVERY_N_MILLISECONDS(20) { gHue++; }
    
    // 7. 物理刷新灯板 (限制在 30FPS 左右，保证 WROOM-32 稳定性)
    // 🌟【精细微调】：音乐模式 15 下彻底跳过此处的刷新锁，把帧率上限全部释放给 SpectrumManager 内部的 FastLED.show()！
    if (currentMode != 15) {
        static unsigned long lastShow = 0;
        if (millis() - lastShow > 30) { 
            lastShow = millis();
            FastLED.show();
        }
    }
}
