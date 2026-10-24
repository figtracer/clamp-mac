#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "Oversampler.h"
#include "IControls.h"
#include "Waveshapers.h"
#include "ClampUI.h"

#include <vector>

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

private:
  OverSampler<sample> mOverSampler { kNone, true, 2, 2 };
  EFactor mOversamplingFactor = kNone;
  int mOversamplingBlockSize = 0;
  Waveshapers::NaiveClipper mNaiveClipper;
  Waveshapers::Clipper mClipper;
  std::vector<sample> mMonoScratch;
  ClampDisplaySender mDisplaySender;
#endif
};
