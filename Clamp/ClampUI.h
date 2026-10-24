#pragma once

#include "IPlug_include_in_plug_hdr.h"
#include "ISender.h"

#include <array>

/** Seconds of audio in one point of the level display. */
constexpr double kDisplayPointSeconds = 0.02;

/** Sends one pair of peaks per display point: the level into the clipper and the level out of it.
 * The input scan and the output scan are separate, because the host can process in place. */
class ClampDisplaySender final : public iplug::ISender<2, 256, float>
{
public:
  void Reset(double sampleRate);

  /** Scans the block input, scaled by gain. Call this before the block is processed. */
  void ProcessInput(iplug::sample** inputs, int nFrames, double gain);

  /** Scans the block output, scaled by gain, and queues each finished point for ctrlTag. */
  void ProcessOutput(iplug::sample** outputs, int nFrames, double gain, int ctrlTag);

private:
  static constexpr int kMaxPointsPerBlock = 64;

  std::array<float, kMaxPointsPerBlock> mInputPeaks {};
  int mNumInputPeaks = 0;
  int mWindowSize = 1;
  int mCount = 0;
  float mInputPeak = 0.f;
  float mOutputPeak = 0.f;
};

#if IPLUG_EDITOR
/** Attaches the Clamp editor controls. The level display listens on displayCtrlTag. */
void AttachClampUI(iplug::igraphics::IGraphics* pGraphics, int displayCtrlTag);
#endif
