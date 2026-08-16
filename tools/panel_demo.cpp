// Renders the same phrase through several front-panel settings.
#include "../src/cp80.hpp"
#include <cstdio>
#include <string>
static void wav(const char* p,const std::vector<float>& x,int sr){
  FILE* f=fopen(p,"wb"); uint32_t nB=uint32_t(x.size()*2);
  auto u32=[&](uint32_t v){fwrite(&v,4,1,f);}; auto u16=[&](uint16_t v){fwrite(&v,2,1,f);};
  fwrite("RIFF",1,4,f);u32(36+nB);fwrite("WAVE",1,4,f);fwrite("fmt ",1,4,f);u32(16);u16(1);u16(1);
  u32(sr);u32(sr*2);u16(2);u16(16);fwrite("data",1,4,f);u32(nB);
  for(float v:x){float c=v<-1?-1:(v>1?1:v);u16(uint16_t(int16_t(c*32767.f)));} fclose(f);
}
int main(){
  const int SR=48000,BLK=128,SEC=6;
  struct P{const char*name;float b,m,t;int bril;};
  P ps[]={
    {"flat",            0, 0, 0, 1},
    {"brilliance-low",  0, 0, 0, 0},
    {"brilliance-high", 0, 0, 0, 2},
    {"bass+6_treble+4", 6, 0, 4, 1},
  };
  for(auto&p:ps){
    cp80::CP80 e; e.prepare(SR,BLK);
    e.setTone(p.b,p.m,p.t); e.setBrilliance(p.bril);
    std::vector<float> o(SR*SEC,0.f), b(BLK);
    int notes[]={48,55,64,67,72};
    for(int i=0;i<SR*SEC;i+=BLK){
      if(i==0) for(int n:notes) e.noteOn(n,0.6f);
      std::memset(b.data(),0,4*BLK); e.process(b.data(),BLK);
      int c=std::min(BLK,SR*SEC-i); memcpy(&o[i],b.data(),4*c);
    }
    float pk=0; for(float v:o) pk=std::max(pk,std::fabs(v));
    for(float&v:o) v/= (pk>0?pk/0.89f:1.f);
    std::string fn=std::string("out/panel-")+p.name+".wav";
    wav(fn.c_str(),o,SR); printf("wrote %s\n",fn.c_str());
  }
}
