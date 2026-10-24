// Reads "kind mode in out ceil shape l r" lines; prints shaped l r from the port.
#include "../../Clamp/Waveshapers.h"
#include <cstdio>
int main() {
  char k; int mode; double in, out, c, sh, l, r;
  while (scanf(" %c %d %lf %lf %lf %lf %lf %lf", &k, &mode, &in, &out, &c, &sh, &l, &r) == 8) {
    Waveshapers::NaiveClipper a; Waveshapers::Clipper b;
    Waveshapers::Waveshaper& w = k == 'A' ? (Waveshapers::Waveshaper&)a : (Waveshapers::Waveshaper&)b;
    w.mMode = mode; w.mInGain = in; w.mOutGain = out;
    a.mCeiling = std::pow(10., c / 20.); a.mShape = sh; b.SetThreshold(c, (1. - sh) * 24.);
    w.Process(l, r);
    printf("%.17g %.17g\n", l, r);
  }
}
