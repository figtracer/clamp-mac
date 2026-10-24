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
    pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
    pGraphics->AttachPanelBackground(IColor(255, 30, 32, 36));
    pGraphics->LoadFont("Roboto-Regular", ROBOTO_FN);

    const IVStyle style = DEFAULT_STYLE
      .WithColor(kBG, IColor(255, 30, 32, 36))
      .WithColor(kFG, IColor(255, 52, 56, 64))
      .WithColor(kPR, IColor(255, 240, 160, 60))
      .WithColor(kX1, IColor(255, 240, 160, 60))
      .WithLabelText(IText(13.f, COLOR_LIGHT_GRAY, "Roboto-Regular"))
      .WithValueText(IText(12.f, COLOR_WHITE, "Roboto-Regular"))
      .WithDrawShadows(false);

    const IRECT b = pGraphics->GetBounds().GetPadded(-10.f);
    const IRECT top = b.GetFromTop(28.f);
    pGraphics->AttachControl(new ITextControl(top.GetFromLeft(120.f), "CLAMP", IText(20.f, COLOR_WHITE, "Roboto-Regular", EAlign::Near)));
    pGraphics->AttachControl(new IVToggleControl(top.GetFromRight(80.f), kBypass, "", style, "ACTIVE", "BYPASS"));

    const IRECT body = b.GetReducedFromTop(36.f);
    const IRECT meters = body.GetFromRight(60.f);
    const IRECT controls = body.GetReducedFromRight(70.f);

    const IRECT knobs = controls.GetFromTop(120.f);
    const char* knobLabels[] = {"IN", "CEILING", "SHAPE", "OUT"};
    const int knobParams[] = {kInputGain, kCeiling, kShape, kOutputGain};
    for (int i = 0; i < 4; i++)
      pGraphics->AttachControl(new IVKnobControl(knobs.GetGridCell(i, 1, 4), knobParams[i], knobLabels[i], style));

    const IRECT switches = controls.GetReducedFromTop(130.f);
    pGraphics->AttachControl(new IVTabSwitchControl(switches.GetGridCell(0, 3, 1).GetPadded(-2.f), kType, {}, "TYPE", style));
    pGraphics->AttachControl(new IVTabSwitchControl(switches.GetGridCell(1, 3, 1).GetPadded(-2.f), kMode, {}, "MODE", style));
    pGraphics->AttachControl(new IVTabSwitchControl(switches.GetGridCell(2, 3, 1).GetPadded(-2.f), kOversampling, {}, "OVERSAMPLING", style));

    pGraphics->AttachControl(new IVPeakAvgMeterControl<2>(meters.GetGridCell(0, 1, 2), "IN", style.WithShowValue(false)), kCtrlTagInMeter);
    pGraphics->AttachControl(new IVPeakAvgMeterControl<2>(meters.GetGridCell(1, 1, 2), "OUT", style.WithShowValue(false)), kCtrlTagOutMeter);
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

  mInMeterSender.ProcessBlock(ins, nFrames, kCtrlTagInMeter);

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

  mOutMeterSender.ProcessBlock(outs, nFrames, kCtrlTagOutMeter);
}

void Clamp::OnReset()
{
  mMonoScratch.assign(GetBlockSize(), 0.);
  mOverSampler.Reset(GetBlockSize());
  mInMeterSender.Reset(GetSampleRate());
  mOutMeterSender.Reset(GetSampleRate());
}

void Clamp::OnIdle()
{
  mInMeterSender.TransmitData(*this);
  mOutMeterSender.TransmitData(*this);
}

void Clamp::OnParamChange(int paramIdx)
{
  if (paramIdx == kOversampling)
    mOverSampler.SetOverSampling(OversamplingFactor(GetParam(kOversampling)->Int()));
}
#endif
