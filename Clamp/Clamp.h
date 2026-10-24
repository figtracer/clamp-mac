#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "ClampDSP.h"
#include "IControls.h"
#include "ClampUI.h"

const int kNumPresets = 1;

enum EParams
{
  kBypass = 0,
  kOversampling,
  kInputGain,
  kOutputGain,
  kCeiling,
  kShape,
  kType,
  kMode,
  kNumParams
};

enum ECtrlTags
{
  kCtrlTagDisplay = 0,
};

using namespace iplug;
using namespace igraphics;

class Clamp final : public Plugin
{
public:
  Clamp(const InstanceInfo& info);

#if IPLUG_DSP
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
  void OnReset() override;
  void OnIdle() override;
  void OnParamChange(int paramIdx) override;

private:
  ClampDSP mDSP;
  ClampDisplaySender mDisplaySender;
#endif
};
