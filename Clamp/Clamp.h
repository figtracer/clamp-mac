#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "Oversampler.h"
#include "IControls.h"
#include "Waveshapers.h"

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
  kCtrlTagInMeter = 0,
  kCtrlTagOutMeter,
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
  OverSampler<sample> mOverSampler { kNone, true, 2, 2 };
  Waveshapers::NaiveClipper mNaiveClipper;
  Waveshapers::Clipper mClipper;
  std::vector<sample> mMonoScratch;
  IPeakAvgSender<2> mInMeterSender;
  IPeakAvgSender<2> mOutMeterSender;
#endif
};
