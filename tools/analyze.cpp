#include "../src/cp80.hpp"
#include <cstdio>
#include <cstdlib>
// Renders one isolated note and dumps raw samples to stdout as binary float.
int main(int argc,char**argv){
  if(const char* o=getenv("CP80_ANCHORS")) cp80::loadAnchorOverrides(o);
  int note = argc>1?atoi(argv[1]):60; float vel = argc>2?atof(argv[2]):0.8f;
  double secs = argc>3?atof(argv[3]):3.0;
  const int SR=48000,BLK=128; cp80::CP80 p; p.prepare(SR,BLK);
  if (const char* lp = getenv("CP80_PICKUP_LP"))
    p.setPickupTone(float(atof(lp)), 32.f);
  if (const char* hs = getenv("CP80_HAMMER_SCALE"))
    p.setHammerScale(float(atof(hs)));
  if (const char* hp = getenv("CP80_HAMMER_P"))
    p.setHammerExponent(float(atof(hp)));
  if (const char* hq = getenv("CP80_HAMMER_RATE_P"))
    p.setHammerRateExponent(float(atof(hq)));
  if (const char* hc = getenv("CP80_HAMMER_DAMPING"))
    p.setHammerDamping(float(atof(hc)));
  if (const char* hf = getenv("CP80_HAMMER_FACING")) {
    const float tau = getenv("CP80_HAMMER_TAU_MS") ? float(atof(getenv("CP80_HAMMER_TAU_MS"))) : 0.1f;
    p.setHammerFacing(float(atof(hf)), tau);
  }
  if (const char* st = getenv("CP80_STRIKE"))
    p.setStrike(float(atof(st)));
  if (const char* hw = getenv("CP80_HAMMER_WIDTH"))
    p.setHammerWidth(float(atof(hw)));
  if (const char* wc = getenv("CP80_WAVE_CONTACT"))
    p.setWaveContact(atoi(wc) != 0);
  if (const char* wz = getenv("CP80_WAVE_Z"))
    p.setWaveImpedanceScale(float(atof(wz)));
  if (const char* bg = getenv("CP80_BODY_GAIN"))
    p.setBodyGain(float(atof(bg)));
  p.noteOn(note,vel);
  std::vector<float> b(BLK);
  for(int i=0;i<int(secs*SR);i+=BLK){
    std::memset(b.data(),0,sizeof(float)*BLK); p.process(b.data(),BLK);
    fwrite(b.data(),4,BLK,stdout);
  }
  return 0;
}
