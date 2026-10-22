#ifndef SPECTRUM_MANAGER_H
#define SPECTRUM_MANAGER_H

#include <Arduino.h>
#include <FastLED.h>
#include "Config.h"
#include "Protocol.h"

class SpectrumManager {
private:
    CRGB* _leds;
    int _bands[16];
    float _fall_bands[16]; // 用于频谱平滑下落的重力滤波器
    int _is_beat;
    unsigned long _last_feed = 0, _last_decay = 0;

    // 与普通绘图共用整板蛇形映射，避免频谱和游戏方向不一致。
    int getMatrixIndex(int x, int y) {
        return MatrixLayout::PixelIndex(x, y);
    }

public:
    SpectrumManager(CRGB* leds_ptr) {
        _leds = leds_ptr;
        _is_beat = 0;
        for (int i = 0; i < 16; i++) {
            _bands[i] = 0;
            _fall_bands[i] = 0.0f;
        }
    }

    // 核心流解析状态机：揉碎主板发来的 "f,beat,b0,b1..." 裸字符串
    // Validate a bounded packet; invalid input does not update state.
    void reset() {
        _is_beat=0;_last_feed=millis();_last_decay=_last_feed;
        for(int i=0;i<16;++i){_bands[i]=0;_fall_bands[i]=0;}
    }
    bool parseSerialData(const String& cmd) {
        int beat=0,b[16];
        if (!LightingProtocol::ParseSpectrum(cmd.c_str(),beat,b))return false;
        _last_feed=millis();_is_beat=beat;bool energy=false;
        for(int i=0;i<16;++i){_bands[i]=b[i];energy=energy||b[i]>0;}
        if (energy && _bands[0]<=1)_bands[0]=min(15,_bands[1]+1);
        if (_is_beat==1)_bands[0]=15;
        renderSpectrum();return true;
    }
    void tick() {
        unsigned long now=millis();
        if(now-_last_feed>500 && now-_last_decay>=30) {
            _last_decay=now;_is_beat=0;
            for(int i=0;i<16;++i)_bands[i]=0;
            renderSpectrum();
        }
    }

    // 像素阵列图形渲染引擎
    void renderSpectrum() {
        // 1. 整体渐暗：保留轻微拖尾余晖，让频谱舞动更有质感
        fadeToBlackBy(_leds, NUM_LEDS, 160); 

        // 2. 遍历横向 16 列大音轨
        for (int x = 0; x < MATRIX_WIDTH; x++) {
            // 🌟🌟🌟【黄金调校：重力加速度衰减，彻底绝杀半空死锁】🌟🌟🌟
            if ((float)_bands[x] >= _fall_bands[x]) {
                _fall_bands[x] = (float)_bands[x]; // 上升瞬间响应
            } else {
                // Empirical fixed-step and proportional decay.
                // 这样当频谱在高位（比如15）时下落平滑优雅，跌到低位（低于3）时会加速坠落，瞬间砸向地面
                _fall_bands[x] -= (0.4f + _fall_bands[x] * 0.05f); 
                
                // 如果当前下落的重力线已经跌到了新输入信号的底噪区间（例如小于1.2），且新信号很弱，直接强行归零
                if (_fall_bands[x] < (float)_bands[x] + 0.2f && _bands[x] <= 2) {
                    _fall_bands[x] = (float)_bands[x];
                }
                
                if (_fall_bands[x] < 0.0f) _fall_bands[x] = 0.0f;
            }

            int current_height = (int)_fall_bands[x];
            if (current_height <= 0) continue;

            // 3. 开始纵向绘制每一列的像素
            for (int y = 0; y <= current_height; y++) {
                // 🌟【核心世界线修正】：利用 15 - y 倒算物理硬件索引！
                // 从整板最底行（物理 y=15）开始向上生长。
                int target_physical_y = 15 - y;
                // 严密越界锁死：确保翻转后的物理 Y 轴安全落在 0~15 空间内
                if (target_physical_y < 0) target_physical_y = 0;
                // 颜色渐变继续从底部向顶部排列。
                CHSV pixel_color = CHSV(y * 14 + 140, 245, 255);

                // 传入翻转后的物理 Y 坐标，计算出正确的串联物理 LED 编号
                int led_idx = getMatrixIndex(x, target_physical_y);
                if (led_idx >= 0) _leds[led_idx] = pixel_color;
            }
        }

        // 🌟【Beat 瞬态大鼓氛围】：若抓到 beat=1，全屏叠加轻微的冷色微光氛围
        if (_is_beat == 1) {
            for (int i = 0; i < NUM_LEDS; i++) {
                _leds[i] += CRGB(0, 25, 25); 
            }
        }

        // FastLED software power estimate; not a physical current regulator.
        FastLED.setMaxPowerInVoltsAndMilliamps(MAX_POWER_VOLTS, MAX_POWER_MA); 

        // 瞬间推向物理引脚发光！
        FastLED.show();
    }
};

#endif
