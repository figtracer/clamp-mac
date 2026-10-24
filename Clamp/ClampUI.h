#pragma once

#include "IPlug_include_in_plug_hdr.h"

struct ClampSettings;

#if IPLUG_EDITOR
/** Attaches the Clamp editor controls, in the order of the original layout (0x180546e20). */
void AttachClampUI(iplug::igraphics::IGraphics* pGraphics);

/** Applies Settings.ini values when the editor opens: the scale, the accent colour and the tooltips. */
void ApplyClampSettings(iplug::igraphics::IGraphics* pGraphics, const ClampSettings& settings);

/** The current editor values to save in Settings.ini when the editor closes. */
ClampSettings CollectClampSettings(iplug::igraphics::IGraphics* pGraphics);
#endif
