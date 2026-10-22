"""Compile and execute real protocol/game functions with no connected devices."""
import argparse
import os
from pathlib import Path
import re
import subprocess
import tempfile


def function(source,name):
    m=re.search(r'(?:void|int|bool)\s+'+name+r'\([^;]*?\)\s*\{',source)
    end=m.end();depth=1
    while depth:
        depth+=(source[end]=='{')-(source[end]=='}');end+=1
    return source[m.start():end]


def run(a,out):
    root=Path(__file__).parents[1];board=root/'esp32-wroom-32-arduino'
    game=(board/'GameEngine.h').read_text(encoding='utf-8')
    audio=(root/'esp32-s3r16n8-idf/main/audio/audio_service.cc').read_text(encoding='utf-8')
    silence=re.search(r'kSilenceFrame\[\] = "([^"]+)"',audio).group(1).replace('\\n','')
    pre=r'''
#include "Protocol.h"
extern "C" void *memset(void *p,int v,unsigned long n){unsigned char*q=(unsigned char*)p;for(unsigned long i=0;i<n;i++)q[i]=v;return p;}
int currentMode=19,snakeX[256],snakeY[256],snakeLen,sDx,sDy,foodX,foodY,snakeScore,snakeSpeed;
unsigned long now=0,lastSnakeUpdate=0; bool snakeStarted=true;
unsigned long millis(){return now;} int random(int n){return n-1;}int max(int a,int b){return a>b?a:b;}
char pendingSnakeDirection=0;
struct CRGB{enum{Red,Lime,Green};};
struct LEDs{void clear(){}} FastLED;
void drawPixel(int,int,int){}
'''
    core='\n'.join(function(game,n) for n in ('requestSnakeDirection','placeSnakeFood','initSnake','runSnake'))
    tests=r'''
extern "C" int run_tests(){
    using namespace LightingProtocol;int beat=-1,b[16],mode=-1;
    if(!ParseMode("15",mode)||mode!=15)return 1;
    if(ParseMode("17",mode)||ParseMode("18",mode)||ParseMode("abc",mode)||ParseMode("9999999",mode))return 2;
    if(!ParseMode("0",mode)||mode!=0)return 3;
    if(!ParseSpectrum(SILENCE,beat,b)||beat!=0)return 4;
    if(ParseSpectrum("f,0,0",beat,b))return 5;
    if(ParseSpectrum("f,2,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0",beat,b))return 6;
    if(ParseSpectrum("f,0,99,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0",beat,b))return 7;
    if(ParseSpectrum("f,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0x",beat,b))return 8;
    Line line;for(int i=0;i<100;++i)line.Push('9');line.Push('1');
    if(line.Push('\n'))return 9;
    line.Push('1');const char*s=line.Push('\n');if(!s||s[0]!='1'||s[1])return 10;
    snakeLen=255;for(int i=0;i<255;i++){snakeX[i]=i%16;snakeY[i]=i/16;}
    if(!placeSnakeFood()||foodX!=15||foodY!=15)return 11;
    snakeX[255]=15;snakeY[255]=15;snakeLen=256;if(placeSnakeFood())return 12;
    snakeLen=255;snakeX[0]=14;snakeY[0]=15;
    for(int i=0;i<254;i++){snakeX[i+1]=i%16;snakeY[i+1]=i/16;}
    sDx=1;sDy=0;foodX=15;foodY=15;snakeScore=0;snakeSpeed=1;now=10;
    runSnake();if(snakeLen!=256||snakeStarted||currentMode!=20||snakeScore!=1)return 13;
    initSnake();foodX=15;foodY=15;now+=500;requestSnakeDirection('W');requestSnakeDirection('A');
    runSnake();if(sDx!=1||sDy!=0||snakeX[0]!=6)return 14;
    return 0;
}
'''
    tests=tests.replace('SILENCE','"'+silence+'"')
    out.mkdir(parents=True,exist_ok=True);cpp=out/'logic.cpp';wasm=out/'logic.wasm'
    cpp.write_text(pre+core+tests,encoding='utf-8');env=os.environ.copy();env['TMP']=env['TEMP']=str(out.resolve())
    subprocess.run([a.clang,'--target=wasm32','-std=c++11','-fno-builtin','-nostdlib','-nostdinc++','--sysroot='+a.sysroot,'-I',str(board),'-Wl,--no-entry','-Wl,--export=run_tests',str(cpp),'-o',str(wasm)],check=True,env=env)
    js='WebAssembly.instantiate(require("fs").readFileSync(process.argv[1])).then(x=>{let n=x.instance.exports.run_tests();console.log({cases:14,failedCase:n});process.exitCode=n;})'
    subprocess.run([a.node,'-e',js,str(wasm)],check=True)


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--clang',required=True);p.add_argument('--sysroot',required=True);p.add_argument('--node',default='node');p.add_argument('--output-dir');a=p.parse_args()
    if a.output_dir:run(a,Path(a.output_dir))
    else:
        with tempfile.TemporaryDirectory(prefix='lighting-mid-tests-') as t:run(a,Path(t))
