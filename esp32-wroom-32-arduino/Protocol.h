#ifndef LIGHTING_PROTOCOL_H
#define LIGHTING_PROTOCOL_H
#include <stddef.h>

namespace LightingProtocol {
inline bool ParseMode(const char *s, int &mode) {
    if (!s || !*s) return false;
    int value=0, digits=0;
    while (*s) {
        if (*s<'0'||*s>'9'||++digits>2) return false;
        value=value*10+(*s++-'0');
    }
    if (!(value<=16 || (value>=19 && value<=22))) return false;
    mode=value;return true;
}
inline bool ParseSpectrum(const char *s, int &beat, int *bands) {
    if (!s || s[0]!='f' || s[1]!=',') return false;
    s+=2;int values[17];
    for(int i=0;i<17;++i) {
        int value=0,digits=0;
        while (*s>='0'&&*s<='9') {
            if (++digits>2) return false;
            value=value*10+(*s++-'0');
        }
        if (!digits || value>(i==0?1:15)) return false;
        values[i]=value;
        if (i<16) { if(*s++!=',')return false; }
        else if(*s!='\0')return false;
    }
    beat=values[0];for(int i=0;i<16;++i)bands[i]=values[i+1];return true;
}
class Line {
    char buffer_[96];size_t count_=0;bool overflow_=false;
public:
    const char *Push(char c) {
        if (c=='\r'||c=='\n') {
            if (overflow_) { Reset();return nullptr; }
            if (!count_)return nullptr;
            buffer_[count_]='\0';count_=0;return buffer_;
        }
        if (!overflow_) {
            if (count_+1<sizeof(buffer_)) buffer_[count_++]=c;
            else {count_=0;overflow_=true;}
        }
        return nullptr;
    }
    void Reset(){count_=0;overflow_=false;}
};
}
#endif
