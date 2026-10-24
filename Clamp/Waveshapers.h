#pragma once

// Clean-room reconstruction of Clamp's Waveshapers namespace, recovered from
// the Windows build (Clamp.vst3 v2.2.0) with REA + Ghidra.

#include <algorithm>
#include <cmath>

namespace Waveshapers
{

/** Channel processing modes, in the order of the MODE parameter. */
enum EMode
{
  kModeLR = 0,
  kModeL,
  kModeR,
  kModeMS,
  kModeM,
  kModeS,
};

/** Shared gain staging and channel routing (FUN_180542380 in the original). */
class Waveshaper
{
public:
  virtual ~Waveshaper() = default;

  /** Shapes one sample in place. */
  virtual void Shape(double& x) = 0;

  void Process(double& l, double& r)
  {
    const double dryL = l;
    const double dryR = r;

    if (mMode < kModeMS)
    {
      if (mMode == kModeLR || mMode == kModeL)
        ProcessOne(l);
      if (mMode == kModeLR || mMode == kModeR)
        ProcessOne(r);
    }
    else
    {
      double m = (l + r) * 0.5;
      double s = (l - r) * 0.5;
      if (mMode == kModeMS || mMode == kModeM)
        ProcessOne(m);
      if (mMode == kModeMS || mMode == kModeS)
        ProcessOne(s);
      l = m + s;
      r = m - s;
    }

    // The original keeps a dry/wet stage here that is always fully wet.
    l = (l - dryL) * mMix + dryL;
    r = (r - dryR) * mMix + dryR;
  }

  int mMode = kModeLR;
  double mInGain = 1.;
  double mOutGain = 1.;
  double mMix = 1.;

private:
  void ProcessOne(double& x)
  {
    x *= mInGain;
    Shape(x);
    x *= mOutGain;
  }
};

/** TYPE A: crossfade between tanh saturation (SHAPE 0) and a hard clip (SHAPE 1). */
class NaiveClipper final : public Waveshaper
{
public:
  void Shape(double& x) override
  {
    const double c = mCeiling;
    const double soft = std::tanh(x / c) * c;
    const double hard = std::clamp(x, -c, c);
    x = (hard - soft) * mShape + soft;
  }

  /** Linear ceiling. */
  double mCeiling = 1.;
  double mShape = 1.;
};

/** TYPE B: infinite-ratio gain computer with a quadratic soft knee in dB. */
class Clipper final : public Waveshaper
{
public:
  void SetThreshold(double thresholdDB, double kneeDB)
  {
    mThresholdDB = thresholdDB;
    mKneeDB = kneeDB;
    mKneeLoDB = thresholdDB - kneeDB * 0.5;
    mKneeHiDB = thresholdDB + kneeDB * 0.5;
  }

  void Shape(double& x) override
  {
    const double levelDB = 20. * std::log10(std::fabs(x));
    double gainDB;
    if (mKneeDB > 0. && levelDB > mKneeLoDB && levelDB < mKneeHiDB)
      gainDB = ((levelDB - mKneeLoDB) / mKneeDB) * 0.5 * (mKneeLoDB - levelDB);
    else
      gainDB = std::min(mThresholdDB - levelDB, 0.);
    x *= std::pow(10., gainDB / 20.);
  }

private:
  double mThresholdDB = 0.;
  double mKneeDB = 0.;
  double mKneeLoDB = 0.;
  double mKneeHiDB = 0.;
};

} // namespace Waveshapers
