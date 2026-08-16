// Component-level profiler.  make bench
// NOTE: measure with voices ALIVE. An earlier version timed 30k blocks after a single
// strike; the notes decayed, most blocks rendered silence, and every number came out
// ~20x optimistic. Re-strike inside the loop. Cross-check any result against `make check`.
#include "../src/cp80.hpp"
#include <cstdio>
#include <cstdlib>
#include <chrono>
using C = std::chrono::high_resolution_clock;
static double us(C::time_point a, C::time_point b){ return std::chrono::duration<double,std::micro>(b-a).count(); }
int main(){
  const int SR=48000, BLK=128; std::vector<float> b(BLK), os(2*BLK), out(BLK);
  const float hammerScale = std::getenv("CP80_HAMMER_SCALE") ?
      std::atof(std::getenv("CP80_HAMMER_SCALE")) : 1.f;
  printf("=== fixed per-block cost (%d samples @ %d Hz, budget %.0f us) ===\n",BLK,SR,1e6*BLK/SR);
  { auto t0=C::now(); for(int k=0;k<200000;++k) std::memset(os.data(),0,sizeof(float)*2*BLK);
    printf("  memset          %8.4f us\n",us(t0,C::now())/200000); }
  { cp80::Decimator2x d; d.prepare(BLK);
    for(int i=0;i<2*BLK;++i) os[i]=std::sin(i*0.07f);
    auto t0=C::now(); for(int k=0;k<200000;++k) d.process(os.data(),out.data(),BLK);
    printf("  decimator       %8.4f us\n",us(t0,C::now())/200000); }
  printf("\n=== free phase (voices re-struck so they stay alive) ===\n");
  for(int nv : {1,2,4,8,12}){
    cp80::CP80 p; p.prepare(SR,BLK);
    double tot=0; long long upd=0; int reps=0;
    for(int r=0;r<200;++r){
      for(int i=0;i<nv;++i) p.noteOn(50+i*3,0.7f);
      for(int k=0;k<24;++k){ std::memset(b.data(),0,4*BLK); p.process(b.data(),BLK); }
      for(int k=0;k<40;++k){
        std::memset(b.data(),0,4*BLK);
        int m=p.activeModes(); auto t0=C::now(); p.process(b.data(),BLK);
        tot+=std::chrono::duration<double,std::nano>(C::now()-t0).count();
        upd+=(long long)m*2*BLK; ++reps;
      }
      for(int i=0;i<nv;++i) p.noteOff(50+i*3);
    }
    printf("  %2d voices  %8.3f us/blk   %.4f ns per mode-sample\n",nv,tot/reps/1000.0,tot/double(upd));
  }
  printf("\n=== noteOn / hammer contact ===\n");
  { double tot=0; const int Q=20000;
    for(int i=0;i<Q;++i){ cp80::CP80 p; p.prepare(SR,BLK);
      auto t0=C::now(); p.noteOn(52,0.7f); tot+=us(t0,C::now()); }
    printf("  noteOn          %8.4f us\n",tot/Q); }
  for(int note : {28,40,52,60,72,84}){
    cp80::CP80 p; p.prepare(SR,BLK); p.setHammerScale(hammerScale); p.noteOn(note,0.8f);
    for(int k=0;k<SR/8;k+=BLK){ std::memset(b.data(),0,4*BLK); p.process(b.data(),BLK); }
    printf("  contact note %3d %7.2f ms   (real piano: ~4 ms bass, ~2 mid, <1 treble)\n",note,p.lastContactMs());
  }
  return 0;
}
