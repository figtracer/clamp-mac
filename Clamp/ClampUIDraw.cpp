#include "ClampUIBase.h"

#include <algorithm>
#include <cmath>

#ifdef IGRAPHICS_NANOVG
  #include "IGraphicsNanoVG.h"
#endif

namespace clampui
{
IText AboutBodyText()
{
  return IText(12.f, kColorAboutText, kTextFont, EAlign::Center, EVAlign::Middle);
}

IText LabelText()
{
  return IText(12.f, kColorLabel, kLabelFont, EAlign::Center, EVAlign::Middle);
}

IText ValueText()
{
  return IText(11.f, kColorLabel, kLabelFont, EAlign::Center, EVAlign::Middle);
}

IText TitleText()
{
  return IText(20.f, kColorLabel, kTitleFont, EAlign::Near, EVAlign::Middle);
}

IText AboutTitleText()
{
  return IText(16.f, kColorLabel, kTitleFont, EAlign::Center, EVAlign::Middle);
}

IText FooterText()
{
  return LabelText().WithFGColor(kColorFooterText);
}

IVStyle ClampStyle()
{
  IVStyle style;
  style.hideCursor = true;
  style.showLabel = false;
  style.showValue = false;
  style.drawFrame = false;
  style.drawShadows = false;
  style.emboss = false;
  style.roundness = 0.f;
  style.frameThickness = 1.f;
  style.shadowOffset = 3.f;
  style.widgetFrac = 1.f;
  style.angle = 0.f;
  style.labelOrientation = EOrientation::North;
  style.labelText = LabelText();
  style.valueText = ValueText();
  style.colorSpec = IVColorSpec {
    IColor(255, 50, 50, 50),    // kBG
    IColor(255, 130, 130, 130), // kFG
    kColorTransparent,          // kPR
    IColor(255, 128, 130, 130), // kFR
    IColor(255, 135, 135, 135), // kHL
    IColor(255, 5, 5, 5),       // kSH
    IColor(255, 160, 160, 160), // kX1, replaced by the accent
    IColor(255, 80, 80, 80),    // kX2, used when disabled
    IColor(255, 100, 100, 100), // kX3
  };
  return style;
}

IColor LerpColor(const IColor& a, const IColor& b, float f)
{
  auto lerp = [f](int x, int y) { return x + static_cast<int>(static_cast<float>(y - x) * f); };
  return IColor(lerp(a.A, b.A), lerp(a.R, b.R), lerp(a.G, b.G), lerp(a.B, b.B));
}

IColor AddColor(const IColor& c, int k)
{
  auto add = [k](int x) { return std::clamp(x + k, 0, 255); };
  return IColor(c.A, add(c.R), add(c.G), add(c.B));
}

IColor ToWhite(const IColor& c, float f)
{
  auto white = [f](int x) { return x - static_cast<int>(static_cast<float>(255 - x) * -f); };
  return IColor(white(c.A), white(c.R), white(c.G), white(c.B));
}

IColor WithAlpha(const IColor& c, int a)
{
  return IColor(a, c.R, c.G, c.B);
}

WDL_String DisplayWithLabel(const IControl& control)
{
  WDL_String str;
  if (const IParam* pParam = control.GetParam())
  {
    pParam->GetDisplay(control.GetValue(), true, str);
    const char* label = pParam->GetLabel();
    if (CStringHasContents(label))
    {
      str.Append(" ");
      str.Append(label);
    }
  }
  return str;
}

Outline RoundRectPolygon(const IRECT& r, float tl, float tr, float bl, float br)
{
  constexpr int kSteps = 8;
  Outline polygon;
  auto corner = [&](float cx, float cy, float radius, float startDeg) {
    if (radius <= 0.f)
    {
      // A square corner: the arc centre is the corner itself.
      polygon.push_back({cx, cy});
      return;
    }
    for (int i = 0; i <= kSteps; i++)
    {
      const float a = DegToRad(startDeg + 90.f * i / kSteps);
      polygon.push_back({cx + radius * std::cos(a), cy + radius * std::sin(a)});
    }
  };
  // Clockwise on screen, starting at the top-left corner.
  corner(r.L + tl, r.T + tl, tl, 180.f);
  corner(r.R - tr, r.T + tr, tr, 270.f);
  corner(r.R - br, r.B - br, br, 0.f);
  corner(r.L + bl, r.B - bl, bl, 90.f);
  return polygon;
}

Outline CirclePolygon(float cx, float cy, float radius)
{
  constexpr int kSteps = 128;
  Outline polygon;
  for (int i = 0; i < kSteps; i++)
  {
    const float a = 2.f * static_cast<float>(PI) * i / kSteps;
    polygon.push_back({cx + radius * std::cos(a), cy + radius * std::sin(a)});
  }
  return polygon;
}

Outline ClipHalfPlane(const Outline& subject, Vertex origin, Vertex normal)
{
  Outline result;
  const size_t n = subject.size();
  auto side = [&](const Vertex& p) { return (p.x - origin.x) * normal.x + (p.y - origin.y) * normal.y; };
  for (size_t i = 0; i < n; i++)
  {
    const Vertex& a = subject[i];
    const Vertex& b = subject[(i + 1) % n];
    const float da = side(a);
    const float db = side(b);
    if (da <= 0.f)
      result.push_back(a);
    if ((da <= 0.f) != (db <= 0.f))
    {
      const float t = da / (da - db);
      result.push_back({a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t});
    }
  }
  return result;
}

Outline ClipPolygon(const Outline& subject, const Outline& clip)
{
  // Sutherland-Hodgman: clip against the half-plane of each edge of the convex clip polygon.
  float area = 0.f;
  for (size_t i = 0; i < clip.size(); i++)
  {
    const Vertex& a = clip[i];
    const Vertex& b = clip[(i + 1) % clip.size()];
    area += a.x * b.y - b.x * a.y;
  }
  const float orientation = area >= 0.f ? 1.f : -1.f;

  Outline result = subject;
  for (size_t i = 0; i < clip.size() && !result.empty(); i++)
  {
    const Vertex& a = clip[i];
    const Vertex& b = clip[(i + 1) % clip.size()];
    if (a.x == b.x && a.y == b.y)
      continue;
    // The outward normal of edge a -> b.
    const Vertex normal = {(b.y - a.y) * orientation, (a.x - b.x) * orientation};
    result = ClipHalfPlane(result, a, normal);
  }
  return result;
}

void PathPolygon(IGraphics& g, const Outline& polygon)
{
  if (polygon.size() < 3)
    return;
  g.PathMoveTo(polygon[0].x, polygon[0].y);
  for (size_t i = 1; i < polygon.size(); i++)
    g.PathLineTo(polygon[i].x, polygon[i].y);
  g.PathClose();
}

IPattern RadialGradient(float cx, float cy, float r, const IColor& c0, float o0, const IColor& c1, float o1)
{
  // NanoVG reads the inner radius from the first offset and the outer radius from the pattern radius.
  const float outer = std::max(r * o1, 1e-3f);
  return IPattern::CreateRadialGradient(cx, cy, outer, {{c0, r * o0 / outer}, {c1, 1.f}});
}

namespace
{
/** The stops of pattern. Where alpha and colour both change between two stops,
 * it adds stops between them, so that NanoVG's premultiplied blend follows the
 * unpremultiplied blend of Skia, which the original uses. */
std::vector<IColorStop> UnpremultipliedStops(const IPattern& pattern)
{
  constexpr int kSteps = 4;
  std::vector<IColorStop> stops;
  for (int k = 0; k < pattern.NStops(); k++)
  {
    const IColorStop& b = pattern.GetStop(k);
    if (k > 0)
    {
      const IColorStop& a = pattern.GetStop(k - 1);
      const bool rgbChanges = a.mColor.R != b.mColor.R || a.mColor.G != b.mColor.G || a.mColor.B != b.mColor.B;
      if (a.mColor.A != b.mColor.A && rgbChanges && b.mOffset > a.mOffset)
      {
        for (int i = 1; i < kSteps; i++)
        {
          const float f = static_cast<float>(i) / kSteps;
          auto mix = [f](int x, int y) { return static_cast<int>(std::lround(x + (y - x) * f)); };
          const IColor c(mix(a.mColor.A, b.mColor.A), mix(a.mColor.R, b.mColor.R), mix(a.mColor.G, b.mColor.G), mix(a.mColor.B, b.mColor.B));
          stops.emplace_back(c, a.mOffset + (b.mOffset - a.mOffset) * f);
        }
      }
    }
    stops.emplace_back(b.mColor, b.mOffset);
  }
  return stops;
}

/** Calls draw(twoStopPattern) once per pair of stops, with a scissor around that pair's band.
 * draw must keep the path. Pairs of transparent stops are skipped. */
template <typename DrawFunc>
void DrawLinearBands(IGraphics& g, const IPattern& pattern, DrawFunc&& draw)
{
  const std::vector<IColorStop> stops = UnpremultipliedStops(pattern);
  const int n = static_cast<int>(stops.size());
  if (n == 0)
    return;

  if (n == 1)
  {
    draw(IPattern(stops[0].mColor));
    return;
  }

  // The gradient runs from s (offset 0) to e (offset 1), as NanoVGPaint computes it.
  IMatrix inverse = IMatrix(pattern.mTransform).Invert();
  double sx, sy, ex, ey;
  inverse.TransformPoint(sx, sy, 0., 0.);
  inverse.TransformPoint(ex, ey, 0., 1.);
  const double dx = ex - sx;
  const double dy = ey - sy;
  const bool vertical = std::fabs(dx) <= 1e-6 * std::fabs(dy);
  const bool horizontal = std::fabs(dy) <= 1e-6 * std::fabs(dx);

#ifdef IGRAPHICS_NANOVG
  NVGcontext* pVG = static_cast<NVGcontext*>(g.GetDrawContext());
  if (!vertical && !horizontal)
  {
    // NanoVG cannot clip a rotated band without widening the region clip, so use the end stops.
    const IColorStop& first = stops.front();
    const IColorStop& last = stops.back();
    draw(IPattern::CreateLinearGradient(static_cast<float>(sx + dx * first.mOffset), static_cast<float>(sy + dy * first.mOffset),
                                        static_cast<float>(sx + dx * last.mOffset), static_cast<float>(sy + dy * last.mOffset),
                                        {{first.mColor, 0.f}, {last.mColor, 1.f}}));
    return;
  }

  constexpr float kFar = 1e5f;
  for (int k = 0; k < n - 1; k++)
  {
    const IColorStop& a = stops[k];
    const IColorStop& b = stops[k + 1];
    if ((a.mColor.A == 0 && b.mColor.A == 0) || b.mOffset <= a.mOffset)
      continue;

    const float x0 = static_cast<float>(sx + dx * a.mOffset);
    const float y0 = static_cast<float>(sy + dy * a.mOffset);
    const float x1 = static_cast<float>(sx + dx * b.mOffset);
    const float y1 = static_cast<float>(sy + dy * b.mOffset);

    // The band runs from this pair's first offset to its second. The first
    // and last bands also cover the clamped ends.
    const bool openStart = k == 0;
    const bool openEnd = k == n - 2;
    const float p0 = vertical ? y0 : x0;
    const float p1 = vertical ? y1 : x1;
    float lo, hi;
    if (p0 <= p1)
    {
      lo = openStart ? -kFar : p0;
      hi = openEnd ? kFar : p1;
    }
    else
    {
      lo = openEnd ? -kFar : p1;
      hi = openStart ? kFar : p0;
    }

    nvgSave(pVG);
    if (vertical)
      nvgIntersectScissor(pVG, -kFar, lo, 2.f * kFar, hi - lo);
    else
      nvgIntersectScissor(pVG, lo, -kFar, hi - lo, 2.f * kFar);
    draw(IPattern::CreateLinearGradient(x0, y0, x1, y1, {{a.mColor, 0.f}, {b.mColor, 1.f}}));
    nvgRestore(pVG);
  }
#else
  (void) vertical;
  (void) horizontal;
  draw(pattern);
#endif
}
} // namespace

void PathFillLinear(IGraphics& g, const IPattern& pattern, bool preserve, const IBlend* pBlend)
{
  IFillOptions options;
  options.mPreserve = true;
  DrawLinearBands(g, pattern, [&](const IPattern& band) { g.PathFill(band, options, pBlend); });
  if (!preserve)
    g.PathClear();
}

void PathStrokeLinear(IGraphics& g, const IPattern& pattern, float thickness, const IStrokeOptions& options, const IBlend* pBlend)
{
  IStrokeOptions bandOptions = options;
  bandOptions.mPreserve = true;
  DrawLinearBands(g, pattern, [&](const IPattern& band) { g.PathStroke(band, thickness, bandOptions, pBlend); });
  if (!options.mPreserve)
    g.PathClear();
}

void PathStrokeAdd(IGraphics& g, const IPattern& pattern, float thickness, const IStrokeOptions& options, float weight)
{
  const IBlend blend(EBlend::Add, weight);
#ifdef IGRAPHICS_NANOVG
  // iPlug2 maps EBlend::Add to (SRC_ALPHA, DST_ALPHA) in NanoVG. NanoVG colours
  // are premultiplied, so that squares the alpha. (ONE, ONE) adds like Skia's plus.
  NVGcontext* pVG = static_cast<NVGcontext*>(g.GetDrawContext());
  DrawLinearBands(g, pattern, [&](const IPattern& band) {
    nvgStrokePaint(pVG, NanoVGPaint(pVG, band, &blend));
    nvgStrokeWidth(pVG, thickness);
    switch (options.mCapOption)
    {
      case ELineCap::Butt: nvgLineCap(pVG, NVG_BUTT); break;
      case ELineCap::Round: nvgLineCap(pVG, NVG_ROUND); break;
      case ELineCap::Square: nvgLineCap(pVG, NVG_SQUARE); break;
    }
    switch (options.mJoinOption)
    {
      case ELineJoin::Miter: nvgLineJoin(pVG, NVG_MITER); break;
      case ELineJoin::Round: nvgLineJoin(pVG, NVG_ROUND); break;
      case ELineJoin::Bevel: nvgLineJoin(pVG, NVG_BEVEL); break;
    }
    nvgMiterLimit(pVG, options.mMiterLimit);
    nvgGlobalCompositeBlendFunc(pVG, NVG_ONE, NVG_ONE);
    nvgStroke(pVG);
    nvgGlobalCompositeOperation(pVG, NVG_SOURCE_OVER);
  });
  if (!options.mPreserve)
    g.PathClear();
#else
  g.PathStroke(pattern, thickness, options, &blend);
#endif
}

void PathFillRadial(IGraphics& g, float cx, float cy, float r, std::vector<IColorStop> stops, const Outline* pClip)
{
  // An outer run of one colour is the same as ending the gradient where the run starts.
  while (stops.size() > 1 && stops[stops.size() - 1].mColor == stops[stops.size() - 2].mColor)
    stops.pop_back();

  const size_t n = stops.size();
  if (n == 0)
  {
    g.PathClear();
    return;
  }
  if (n == 1)
  {
    g.PathFill(IPattern(stops[0].mColor));
    return;
  }

  const IColorStop& a = stops[n - 2];
  const IColorStop& b = stops[n - 1];
  g.PathFill(RadialGradient(cx, cy, r, a.mColor, a.mOffset, b.mColor, b.mOffset));

  for (size_t k = n - 2; k-- > 0;)
  {
    const IColorStop& inner = stops[k];
    const IColorStop& outer = stops[k + 1];
    const float radius = r * outer.mOffset;
    if (pClip)
      PathPolygon(g, ClipPolygon(CirclePolygon(cx, cy, radius), *pClip));
    else
      g.PathCircle(cx, cy, radius);
    g.PathFill(RadialGradient(cx, cy, r, inner.mColor, inner.mOffset, outer.mColor, outer.mOffset));
  }
}
} // namespace clampui
