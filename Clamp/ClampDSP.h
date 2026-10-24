#pragma once

// The complete audio path of Clamp, without iPlug2 plugin or UI types, so that
// tools/emulate can run the same code as the plugin.

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>

// Oversampler.h expects these from earlier iPlug2 headers.
#ifndef DEFAULT_BLOCK_SIZE
  #define DEFAULT_BLOCK_SIZE 512
#endif

#include "Oversampler.h"
#include "Waveshapers.h"

class ClampDSP
{
public:
  struct Params
  {
    bool bypass = false;
    int oversampling = 0;
    double inDB = 0.;
    double outDB = 0.;
    double ceilingDB = 0.;
    double shape = 1.;
    int type = 0;
    int mode = 0;
  };

  /** Host block size. Unlike Reset(), this keeps the filter state, as the original does. */
  void SetBlockSize(int blockSize) { mHostBlockSize = blockSize; }

  /** The original's OnParamChange (0x180549140) arms this for OVERSAMPLING only. */
  void RequestOversamplingReset() { mResetPending = true; }

  /** Processes two channels. Mirrors the original ProcessBlock (0x180548b40). */
  void Process(double** ins, double** outs, int nFrames, const Params& p)
  {
    if (p.bypass)
    {
      for (int c = 0; c < 2; c++)
        std::copy(ins[c], ins[c] + nFrames, outs[c]);
      return;
    }

    const double inGain = DBToAmp(p.inDB);
    const double outGain = DBToAmp(p.outDB);

    mNaiveClipper.mInGain = inGain;
    mNaiveClipper.mOutGain = outGain;
    mNaiveClipper.mMode = p.mode;
    mNaiveClipper.mCeiling = DBToAmp(p.ceilingDB);
    mNaiveClipper.mShape = p.shape;

    mClipper.mInGain = inGain;
    mClipper.mOutGain = outGain;
    mClipper.mMode = p.mode;
    mClipper.SetThreshold(p.ceilingDB, (1. - p.shape) * kMaxKneeDB);

    Waveshapers::Waveshaper& shaper = p.type == 0
      ? static_cast<Waveshapers::Waveshaper&>(mNaiveClipper)
      : static_cast<Waveshapers::Waveshaper&>(mClipper);

    // Like the original (0x180548eb0): set the factor and reset to the host
    // block size on the audio thread, only when a reset is pending. The
    // original never resizes for larger blocks and overruns its buffers; the
    // port resets in that case instead.
    if (mResetPending.exchange(false) || nFrames > mAllocatedBlockSize)
    {
      mAllocatedBlockSize = std::max(mHostBlockSize, nFrames);
      mOverSampler.SetOverSampling(OversamplingFactor(p.oversampling));
      mOverSampler.Reset(mAllocatedBlockSize);
    }

    mOverSampler.ProcessBlock(ins, outs, nFrames, 2, 2, [&shaper](double** in, double** out, int n) {
      for (int s = 0; s < n; s++)
      {
        double l = in[0][s];
        double r = in[1][s];
        shaper.Process(l, r);
        out[0][s] = l;
        out[1][s] = r;
      }
    });
  }

private:
  /** Soft-knee width in dB at SHAPE 0 (Clamp member at offset 0x870). */
  static constexpr double kMaxKneeDB = 24.;

  /** Same conversion as the original: exp(dB * ln(10) / 20). */
  static double DBToAmp(double dB) { return std::exp(dB * 0.1151292546497); }

  static iplug::EFactor OversamplingFactor(int index)
  {
    switch (index)
    {
      case 1: return iplug::k2x;
      case 2: return iplug::k4x;
      case 3: return iplug::k8x;
      case 4: return iplug::k16x;
      default: return iplug::kNone;
    }
  }

  iplug::OverSampler<double> mOverSampler { iplug::kNone, true, 2, 2 };
  std::atomic<bool> mResetPending { true };
  int mHostBlockSize = DEFAULT_BLOCK_SIZE;
  int mAllocatedBlockSize = 0;
  Waveshapers::NaiveClipper mNaiveClipper;
  Waveshapers::Clipper mClipper;
};
