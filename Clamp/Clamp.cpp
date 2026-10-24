#include "Clamp.h"
#include "IPlug_include_in_plug_src.h"

#if IPLUG_EDITOR
  #include "ClampSettings.h"
  #include "ClampUI.h"
#endif

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
    // Like the original, open at scale 1. OnUIOpen applies the scale from Settings.ini.
    return MakeGraphics(*this, PLUG_WIDTH, PLUG_HEIGHT, PLUG_FPS, 1.f);
  };

  mLayoutFunc = [&](IGraphics* pGraphics) {
    AttachClampUI(pGraphics);
  };
#endif
}

#if IPLUG_EDITOR
void Clamp::OnUIOpen()
{
  Plugin::OnUIOpen();

  // Like the original (0x180549230): read Settings.ini, then apply the scale, the accent colour and tooltips.
  ClampSettings settings;
  ReadClampSettings(settings);
  ApplyClampSettings(GetUI(), settings);
}

void Clamp::OnUIClose()
{
  // Like the original (0x180549150): Settings.ini is written only when the editor closes.
  if (GetUI())
    WriteClampSettings(CollectClampSettings(GetUI()));

  Plugin::OnUIClose();
}
#endif

#if IPLUG_DSP
void Clamp::ProcessBlock(sample** inputs, sample** outputs, int nFrames)
{
  // Like the original, always process two channels. iPlug2 backs an
  // unconnected channel with a zeroed scratch buffer.
  sample* ins[2] = {inputs[0], inputs[1]};
  sample* outs[2] = {outputs[0], outputs[1]};

  ClampDSP::Params p;
  p.bypass = GetParam(kBypass)->Bool();
  p.oversampling = GetParam(kOversampling)->Int();
  p.inDB = GetParam(kInputGain)->Value();
  p.outDB = GetParam(kOutputGain)->Value();
  p.ceilingDB = GetParam(kCeiling)->Value();
  p.shape = GetParam(kShape)->Value();
  p.type = GetParam(kType)->Int();
  p.mode = GetParam(kMode)->Int();

  // The DSP meters each oversampled sample, as the original does. Bypass sends nothing, so the display stops.
  auto sendMeter = [this](float post, float pre) {
    mDisplaySender.PushData({kCtrlTagDisplay, {post, pre}});
  };
  mDSP.Process(ins, outs, nFrames, p, sendMeter);
}

void Clamp::OnReset()
{
  mDSP.SetBlockSize(GetBlockSize());
  mDSP.SetSampleRate(GetSampleRate());
}

void Clamp::OnParamChange(int paramIdx)
{
  if (paramIdx == kOversampling)
    mDSP.RequestOversamplingReset();
}

void Clamp::OnIdle()
{
  mDisplaySender.TransmitData(*this);
}
#endif
