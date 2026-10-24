#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "ClampDSP.h"
#include "ISender.h"

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

/** Control tags, as in the original layout. */
enum ECtrlTags
{
  kCtrlTagDisplay = 0,
  kCtrlTagHeader,
  kCtrlTagFooter,
};

using namespace iplug;
using namespace igraphics;

class Clamp final : public Plugin
{
public:
  Clamp(const InstanceInfo& info);

#if IPLUG_EDITOR
  void OnUIOpen() override;
  void OnUIClose() override;
#endif

#if IPLUG_DSP
  void ProcessBlock(sample** inputs, sample** outputs, int nFrames) override;
  void OnReset() override;
  void OnIdle() override;
  void OnParamChange(int paramIdx) override;

private:
  ClampDSP mDSP;
  /** Level display packets: vals[0] is the output level, vals[1] the input level after IN gain.
   * Like the original, the queue holds 64 packets and drops new ones when full. */
  ISender<2, 64, float> mDisplaySender;
#endif
};
