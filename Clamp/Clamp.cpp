#include "Clamp.h"
#include "IPlug_include_in_plug_src.h"

namespace
{
/** Soft-knee width in dB at SHAPE 0 (Clamp member at offset 0x870). */
constexpr double kMaxKneeDB = 24.;

EFactor OversamplingFactor(int index)
{
  switch (index)
  {
    case 1: return k2x;
    case 2: return k4x;
    case 3: return k8x;
    case 4: return k16x;
    default: return kNone;
  }
}
} // namespace

Clamp::Clamp(const InstanceInfo& info)
: iplug::Plugin(info, MakeConfig(kNumParams, kNumPresets))
{
  GetParam(kBypass)->InitBool("BYPASS", false, "", 0, "", "OFF", "ON");
  GetParam(kOversampling)->InitEnum("OVERSAMPLING", 0, {"OFF", "2X", "4X", "8X", "16X"});
  GetParam(kInputGain)->InitDouble("IN", 0., -24., 24., 0.5, "dB", 0, "", IParam::ShapeLinear(), IParam::kUnitDB);
  GetParam(kOutputGain)->InitDouble("OUT", 0., -24., 24., 0.5, "dB", 0, "", IParam::ShapeLinear(), IParam::kUnitDB);
  GetParam(kCeiling)->InitDouble("CEILING", 0., -48., 0., 0.5, "dB", 0, "", IParam::ShapeLinear(), IParam::kUnitDB);
  GetParam(kShape)->InitDouble("SHAPE", 1., 0., 1., 0.01);
  GetParam(kType)->InitEnum("TYPE", 0, {"TYPE A", "TYPE B"});
  GetParam(kMode)->InitEnum("MODE", 0, {"LR", "L", "R", "MS", "M", "S"});

#if IPLUG_EDITOR
  mMakeGraphicsFunc = [&]() {
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, GetScaleForScreen(PLUG_WIDTH, PLUG_HEIGHT));
  };

  mLayoutFunc = [&](IGraphics* pGraphics) {
    AttachClampUI(pGraphics, kCtrlTagDisplay);
  };
#endif
}

#if IPLUG_DSP
void Clamp::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  const bool mono = NInChansConnected() < 2 || NOutChansConnected() < 2;
  sample* ins[2] = {inputs[0], mono ? inputs[0] : inputs[1]};
  sample* outs[2] = {outputs[0], mono ? mMonoScratch.data() : outputs[1]};
  if (mono && static_cast<int>(mMonoScratch.size()) < nFrames)
  {
    mMonoScratch.resize(nFrames);
    outs[1] = mMonoScratch.data();
  }

  // The display shows levels at the clipper: after IN gain and before OUT gain.
  const bool bypassed = GetParam(kBypass)->Bool();
  mDisplaySender.ProcessInput(ins, nFrames, bypassed ? 1. : DBToAmp(GetParam(kInputGain)->Value()));

  if (GetParam(kBypass)->Bool())
  {
    for (int c = 0; c < 2; c++)
      std::copy(ins[c], ins[c] + nFrames, outs[c]);
  }
  else
  {
    const double inGain = DBToAmp(GetParam(kInputGain)->Value());
    const double outGain = DBToAmp(GetParam(kOutputGain)->Value());
    const double ceilingDB = GetParam(kCeiling)->Value();
    const double shape = GetParam(kShape)->Value();
    const int type = GetParam(kType)->Int();
    const int mode = GetParam(kMode)->Int();

    mNaiveClipper.mInGain = inGain;
    mNaiveClipper.mOutGain = outGain;
    mNaiveClipper.mMode = mode;
    mNaiveClipper.mCeiling = DBToAmp(ceilingDB);
    mNaiveClipper.mShape = shape;

    mClipper.mInGain = inGain;
    mClipper.mOutGain = outGain;
    mClipper.mMode = mode;
    mClipper.SetThreshold(ceilingDB, (1. - shape) * kMaxKneeDB);

    Waveshapers::Waveshaper& shaper = type == 0
      ? static_cast<Waveshapers::Waveshaper&>(mNaiveClipper)
      : static_cast<Waveshapers::Waveshaper&>(mClipper);

    // Apply oversampling changes on the audio thread, as the original does.
    // SetOverSampling() resets to a 512-sample block, so size for the host.
    const EFactor factor = OversamplingFactor(GetParam(kOversampling)->Int());
    if (factor != mOversamplingFactor || nFrames > mOversamplingBlockSize)
    {
      mOversamplingFactor = factor;
      mOversamplingBlockSize = std::max(GetBlockSize(), nFrames);
      mOverSampler.SetOverSampling(factor);
      mOverSampler.Reset(mOversamplingBlockSize);
    }

    mOverSampler.ProcessBlock(ins, outs, nFrames, 2, 2, [&shaper](sample** in, sample** out, int n) {
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

  mDisplaySender.ProcessOutput(outs, nFrames, bypassed ? 1. : DBToAmp(-GetParam(kOutputGain)->Value()), kCtrlTagDisplay);
}

void Clamp::OnReset()
{
  mMonoScratch.assign(GetBlockSize(), 0.);
  mOversamplingBlockSize = GetBlockSize();
  mOverSampler.Reset(mOversamplingBlockSize);
  mDisplaySender.Reset(GetSampleRate());
}

void Clamp::OnIdle()
{
  mDisplaySender.TransmitData(*this);
}
#endif
