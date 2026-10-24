#pragma once

// Shared style, colour maths and drawing helpers for the Clamp editor. The
// numbers follow the original Clamp 2.2.0 editor (see docs/REVERSE_ENGINEERING.md).

#include "IControl.h"

#include <vector>

namespace clampui
{
using namespace iplug;
using namespace igraphics;

/** Font IDs of the original. The files are Inter, because the original fonts are not ours to ship. */
constexpr const char* kTextFont = "TextFont";
constexpr const char* kLabelFont = "LabelFont";
constexpr const char* kTitleFont = "TitleFont";

/** Editor size in logical pixels. */
constexpr float kEditorWidth = 400.f;
constexpr float kEditorHeight = 320.f;

const IColor kColorBackground(255, 7, 7, 7);
const IColor kColorGlass(255, 17, 17, 17);
const IColor kColorFrame(255, 50, 50, 50);
const IColor kColorFooterText(255, 127, 127, 127);
const IColor kColorLabel(255, 180, 180, 180);
const IColor kColorAboutText(255, 200, 200, 200);
const IColor kColorTransparent(0, 0, 0, 0);

/** The accent palette of ColorMenu. Index 4 (red) is the default. */
constexpr int kNumAccentColors = 8;
const IColor kAccentColors[kNumAccentColors] = {
  IColor(255, 140, 148, 212),
  IColor(255, 160, 140, 212),
  IColor(255, 197, 134, 217),
  IColor(255, 217, 124, 152),
  IColor(255, 217, 100, 100),
  IColor(255, 212, 137, 110),
  IColor(255, 123, 167, 109),
  IColor(255, 160, 160, 160),
};

/** The IText objects of the original (static data at 0x1806fd560 to 0x1806fd7f0). */
IText AboutBodyText();
IText LabelText();
IText ValueText();
IText TitleText();
IText AboutTitleText();
IText FooterText();

/** The global IVStyle that every control copies (0x180709b20). ColorMenu writes the accent into kX1. */
IVStyle ClampStyle();

/** Per-channel A, R, G, B: a + (int)((b - a) * f). */
IColor LerpColor(const IColor& a, const IColor& b, float f);

/** R, G, B plus k, clamped to 0..255. Alpha stays. */
IColor AddColor(const IColor& c, int k);

/** Moves A, R, G and B towards 255 by the fraction f. */
IColor ToWhite(const IColor& c, float f);

/** The colour with alpha a. */
IColor WithAlpha(const IColor& c, int a);

/** The control's value as text, with the unit, e.g. "-12.0 dB". */
WDL_String DisplayWithLabel(const IControl& control);

/** Shows the footer tooltip for caller, like the original helper 0x18052d6f0. */
void ShowFooterTooltip(IControl* pCaller);

/** Shows the value bubble above (x, y) for caller, like the original hook 0x18052d600. */
void ShowValueBubble(IControl* pCaller, float x, float y);

struct Vertex
{
  float x;
  float y;
};

/** A convex polygon, used where NanoVG cannot clip a gradient band. */
using Outline = std::vector<Vertex>;

/** A rounded rectangle as a polygon, with corner radii in the PathRoundRect order. */
Outline RoundRectPolygon(const IRECT& r, float tl, float tr, float bl, float br);

/** A circle as a polygon. */
Outline CirclePolygon(float cx, float cy, float radius);

/** The part of subject inside the convex polygon clip. */
Outline ClipPolygon(const Outline& subject, const Outline& clip);

/** The part of subject where dot(p - origin, normal) <= 0. */
Outline ClipHalfPlane(const Outline& subject, Vertex origin, Vertex normal);

/** Adds the polygon to the current path. */
void PathPolygon(IGraphics& g, const Outline& polygon);

/** Fills the current path with a linear gradient of any number of stops.
 * NanoVG draws two-stop gradients only, so a vertical or horizontal gradient is
 * drawn band by band, with one scissor per pair of stops. */
void PathFillLinear(IGraphics& g, const IPattern& pattern, bool preserve = false, const IBlend* pBlend = nullptr);

/** Strokes the current path with a linear gradient of any number of stops (see PathFillLinear). */
void PathStrokeLinear(IGraphics& g, const IPattern& pattern, float thickness, const IStrokeOptions& options = IStrokeOptions(), const IBlend* pBlend = nullptr);

/** Strokes the current path and adds the colour to what is below, with the colour's alpha
 * times weight. This is Skia's plus blend, which the original gets from EBlend::Add.
 * The pattern may be solid or a linear gradient (see PathFillLinear). */
void PathStrokeAdd(IGraphics& g, const IPattern& pattern, float thickness, const IStrokeOptions& options, float weight);

/** Fills the current path with a radial gradient of any number of opaque stops.
 * The outer pair of stops fills the path, and each inner pair fills a disc on
 * top of it. When pClip is set, the discs are clipped to it. */
void PathFillRadial(IGraphics& g, float cx, float cy, float r, std::vector<IColorStop> stops, const Outline* pClip = nullptr);

/** A two-stop radial gradient that also honours the offsets: c0 at o0 * r and c1 at o1 * r. */
IPattern RadialGradient(float cx, float cy, float r, const IColor& c0, float o0, const IColor& c1, float o1);
} // namespace clampui
