#include "Clamp.h"
#include "IPlug_include_in_plug_src.h"

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
  // Like the original, always process two channels. iPlug2 backs an
  // unconnected channel with a zeroed scratch buffer.
  sample* ins[2] = {inputs[0], inputs[1]};
  sample* outs[2] = {outputs[0], outputs[1]};

  // The display shows levels at the clipper: after IN gain and before OUT gain.
  const bool bypassed = GetParam(kBypass)->Bool();
  mDisplaySender.ProcessInput(ins, nFrames, bypassed ? 1. : DBToAmp(GetParam(kInputGain)->Value()));

  ClampDSP::Params p;
  p.bypass = bypassed;
  p.oversampling = GetParam(kOversampling)->Int();
  p.inDB = GetParam(kInputGain)->Value();
  p.outDB = GetParam(kOutputGain)->Value();
  p.ceilingDB = GetParam(kCeiling)->Value();
  p.shape = GetParam(kShape)->Value();
  p.type = GetParam(kType)->Int();
  p.mode = GetParam(kMode)->Int();
  mDSP.Process(ins, outs, nFrames, p);

  mDisplaySender.ProcessOutput(outs, nFrames, bypassed ? 1. : DBToAmp(-GetParam(kOutputGain)->Value()), kCtrlTagDisplay);
}

void Clamp::OnReset()
{
  mDSP.SetBlockSize(GetBlockSize());
  mDisplaySender.Reset(GetSampleRate());
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
