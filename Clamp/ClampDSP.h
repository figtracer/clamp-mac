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

/** The original's level meter (plugin + 0xbe8, update 0x180549360): a peak
 * envelope with instant attack and a one-pole release, averaged over 2.5 ms
 * windows of the oversampled signal. It sends one (post, pre) pair per window. */
class ClampMeter
{
public:
  /** Window length and release for the oversampled rate, as 0x180548eb0 sets them. */
  void SetRate(double overSampledRate)
  {
    mWindow = std::max(1, static_cast<int>(overSampledRate * 0.0025));
    mRelease = static_cast<float>((1. / (overSampledRate / 1000.)) * 0.015);
  }

  /** Adds one oversampled sample. At the start of each window, calls send(post, pre) with the window means. */
  template <typename SendFunc>
  void Add(double post, double pre, SendFunc& send)
  {
    if (mCount == 0)
    {
      const float n = static_cast<float>(mWindow);
      send(mSum[0] / n, mSum[1] / n);
      mSum[0] = 0.f;
      mSum[1] = 0.f;
    }

    mCount = (mCount + 1) % mWindow;
    Follow(0, post);
    Follow(1, pre);
  }

private:
  void Follow(int c, double value)
  {
    float x = std::fabs(static_cast<float>(value));
    if (x < mEnvelope[c])
      x = (x - mEnvelope[c]) * mRelease + mEnvelope[c];
    mEnvelope[c] = x;
    mSum[c] += x;
  }

  /** Constructor values of the original (5 ms at 48 kHz). The first block replaces them. */
  int mWindow = 240;
  float mRelease = 5.f / 48.f;
  int mCount = 0;
  float mEnvelope[2] = {0.f, 0.f};
  float mSum[2] = {0.f, 0.f};
};

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

  /** Host sample rate, for the meter timing. */
  void SetSampleRate(double sampleRate) { mSampleRate = sampleRate; }

  /** Processes two channels without metering. */
  void Process(double** ins, double** outs, int nFrames, const Params& p)
  {
    auto ignore = [](float, float) {};
    Process(ins, outs, nFrames, p, ignore);
  }

  /** Processes two channels. Mirrors the original ProcessBlock (0x180548b40).
   * Like the original, it meters each oversampled sample after the shaper and
   * calls sendMeter(post, pre) once per 2.5 ms window. Bypass sends nothing. */
  template <typename SendMeterFunc>
  void Process(double** ins, double** outs, int nFrames, const Params& p, SendMeterFunc& sendMeter)
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
      mOverSamplingRate = 1 << std::clamp(p.oversampling, 0, 4);
    }

    const double meterRate = mOverSamplingRate * mSampleRate;
    if (meterRate != mMeterRate)
    {
      mMeterRate = meterRate;
      mMeter.SetRate(meterRate);
    }

    ClampMeter& meter = mMeter;
    mOverSampler.ProcessBlock(ins, outs, nFrames, 2, 2, [&shaper, &meter, &sendMeter](double** in, double** out, int n) {
      for (int s = 0; s < n; s++)
      {
        double l = in[0][s];
        double r = in[1][s];
        shaper.Process(l, r);
        out[0][s] = l;
        out[1][s] = r;
        meter.Add(std::fmax(shaper.mPostL, shaper.mPostR), std::fmax(shaper.mPreL, shaper.mPreR), sendMeter);
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
  double mSampleRate = 48000.;
  int mOverSamplingRate = 1;
  double mMeterRate = 0.;
  ClampMeter mMeter;
};
