#include "../src/cp80.hpp"
#include <cstdio>
#include <chrono>
int main(){
  const int SR=48000,BLK=128; cp80::CP80 p; p.prepare(SR,BLK);
  std::vector<float> b(BLK); double worst=0; int bad=0; int pm=0;
  auto t0=std::chrono::high_resolution_clock::now();
  // hammer 24 voices at full velocity across the whole compass, repeatedly
  for(int blkI=0;blkI<48000/BLK*30;++blkI){
    if(blkI%12==0) for(int k=0;k<6;++k) p.noteOn(21+(blkI*7+k*13)%88, 1.0f);
    if(blkI%12==8) for(int k=0;k<6;++k) p.noteOff(21+((blkI-8)*7+k*13)%88);
    std::memset(b.data(),0,sizeof(float)*BLK);
    p.process(b.data(),BLK);
    for(float v:b){ if(!std::isfinite(v)) ++bad; worst=std::max(worst,double(std::fabs(v))); }
    pm=std::max(pm,p.activeModes());
  }
  auto t1=std::chrono::high_resolution_clock::now();
  double s=std::chrono::duration<double>(t1-t0).count();
  printf("stress: 30 s audio in %.3f s (%.0fx RT, %.2f%% core)\n",s,30.0/s,100*s/30.0);
  printf("        peak |out| = %.4f   non-finite samples = %d   peak modes = %d\n",worst,bad,pm);
  return bad?1:0;
}
