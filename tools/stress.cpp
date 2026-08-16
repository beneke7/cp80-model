#include "../src/cp80.hpp"
#include <cstdio>
#include <chrono>

static int edgeCase(int sr, int block)
{
  cp80::CP80 p;
  p.prepare(sr, block);
  p.setSustain(true);
  for (int note = 21; note < 45; ++note) p.noteOn(note, 0.7f);
  for (int note = 45; note < 53; ++note) p.noteOn(note, 0.7f); // all voices held: steal oldest

  const int request = block * 2 + 17; // deliberately exceeds prepare(maxBlock)
  std::vector<float> audio(size_t(request), 0.f);
  p.process(nullptr, 0);
  p.process(audio.data(), request);
  p.setSustain(false);
  p.process(audio.data(), request);

  int bad = p.activeVoices() != cp80::kMaxVoices ? 1 : 0;
  for (float v : audio) if (!std::isfinite(v)) ++bad;
  return bad;
}

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
  for (int sr : {44100, 48000, 96000})
    for (int block : {1, 17, 128, 2048})
      bad += edgeCase(sr, block);
  printf("        edge configurations bad = %d\n", bad);
  return bad?1:0;
}
