// Prints the stretch curve the model applies, against the two measured anchors.
#include "../src/cp80.hpp"
#include <cstdio>
int main(){
  printf("%5s %5s %8s %10s %12s\n","midi","key","note","cents","f0 Hz");
  const char* N[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
  for(int m : {21,27,33,42,48,54,60,66,72,78,84,96,108}){
    auto s=cp80::specForNote(m);
    printf("%5d %5d %6s%-2d %9.1f %12.3f\n",m,m-20,N[m%12],m/12-1,cp80::stretchCents(m),s.f0);
  }
  printf("\nmeasured anchors: D#1 (midi 27) = -19.0 cents, F#2 (midi 42) = -5.0 cents\n");
}
