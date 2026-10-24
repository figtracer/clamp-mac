// Checks the reconstructed shapers against the formulas recovered from the binary.
#include "../Clamp/Waveshapers.h"

#include <cassert>
#include <cmath>
#include <cstdio>

static bool Near(double a, double b, double eps = 1e-9) { return std::fabs(a - b) < eps; }

int main()
{
  using namespace Waveshapers;

  NaiveClipper a;
  a.mCeiling = 0.5;
  a.mShape = 1.;
  double l = 0.9, r = -0.2;
  a.Process(l, r);
  assert(Near(l, 0.5) && Near(r, -0.2));

  a.mShape = 0.;
  l = 0.9; r = 0.9;
  a.Process(l, r);
  assert(Near(l, std::tanh(0.9 / 0.5) * 0.5));

  a.mMode = kModeL;
  a.mInGain = 2.;
  l = 0.1; r = 0.1;
  a.Process(l, r);
  assert(Near(r, 0.1));

  Clipper b;
  b.SetThreshold(-6., 0.);
  l = 1.; r = 0.1;
  b.Process(l, r);
  assert(Near(20. * std::log10(l), -6.) && Near(r, 0.1));

  b.SetThreshold(-6., 12.);
  double x = std::pow(10., -6. / 20.);
  double y = x;
  b.Shape(y);
  assert(Near(20. * std::log10(y), -6. - 1.5));

  b.mMode = kModeS;
  l = 0.3; r = 0.3;
  b.Process(l, r);
  assert(Near(l, 0.3) && Near(r, 0.3));

  std::puts("waveshapers: ok");
}
