#include "ClampUI.h"

#include "Clamp.h"
#include "ClampControls.h"
#include "ClampSettings.h"
#include "IControls.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <string>

using namespace clampui;

namespace
{
constexpr const char* kAuthorURL = "https://outobugi.com";

#pragma mark - Panels

/** The editor background. Like IPanelControl, but it ignores the disabled blend, so that bypass leaves no trails. */
class BackgroundPanel final : public IControl
{
public:
  BackgroundPanel(const IRECT& bounds, const IColor& color)
  : IControl(bounds)
  , mColor(color)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override { g.FillRect(mColor, mRECT); }

private:
  IColor mColor;
};

/** Panel base (BasePanel with UIBase). It ignores the mouse and the disabled blend. */
class BasePanel : public IControl
{
public:
  /** cornerMode: 0 all corners round, 1 top, 2 bottom, 3 left, 4 right, else square. */
  BasePanel(const IRECT& bounds, const IColor& color, int cornerMode)
  : IControl(bounds)
  , mColor(color)
  , mCornerMode(cornerMode)
  {
    mIgnoreMouse = true;
  }

protected:
  /** The panel rect is padded by 1.5. A square edge gets that back, so neighbouring panels meet. */
  static constexpr float kInset = 1.5f;

  /** UIBase::MakePath (0x18052fd30) with extend 0. It also clips to the path bounds. */
  void MakePath(IGraphics& g, const IRECT& r, float radius) const
  {
    switch (mCornerMode)
    {
      case 0:
        g.PathRoundRect(r, radius);
        break;
      case 1:
        g.PathRoundRect(PathBounds(r), radius, radius, 0.f, 0.f);
        g.PathClipRegion(PathBounds(r));
        break;
      case 2:
        g.PathRoundRect(PathBounds(r), 0.f, 0.f, radius, radius);
        g.PathClipRegion(PathBounds(r));
        break;
      case 3:
        g.PathRoundRect(PathBounds(r), radius, 0.f, radius, 0.f);
        g.PathClipRegion(PathBounds(r));
        break;
      case 4:
        g.PathRoundRect(PathBounds(r), 0.f, radius, 0.f, radius);
        g.PathClipRegion(PathBounds(r));
        break;
      default:
        g.PathMoveTo(r.L - kInset, r.T);
        g.PathLineTo(r.R + kInset, r.T);
        g.PathLineTo(r.R + kInset, r.B);
        g.PathLineTo(r.L - kInset, r.B);
        g.PathClose();
        break;
    }
  }

  /** The same shape as MakePath, as a polygon. */
  Outline MakePolygon(const IRECT& r, float radius) const
  {
    switch (mCornerMode)
    {
      case 0: return RoundRectPolygon(r, radius, radius, radius, radius);
      case 1: return RoundRectPolygon(PathBounds(r), radius, radius, 0.f, 0.f);
      case 2: return RoundRectPolygon(PathBounds(r), 0.f, 0.f, radius, radius);
      case 3: return RoundRectPolygon(PathBounds(r), radius, 0.f, radius, 0.f);
      case 4: return RoundRectPolygon(PathBounds(r), 0.f, radius, 0.f, radius);
      default: return RoundRectPolygon(IRECT(r.L - kInset, r.T, r.R + kInset, r.B), 0.f, 0.f, 0.f, 0.f);
    }
  }

  IColor mColor;
  int mCornerMode;

private:
  IRECT PathBounds(const IRECT& r) const
  {
    switch (mCornerMode)
    {
      case 1: return IRECT(r.L, r.T, r.R, r.B + kInset);
      case 2: return IRECT(r.L, r.T - kInset, r.R, r.B);
      case 3: return IRECT(r.L, r.T, r.R + kInset, r.B);
      case 4: return IRECT(r.L - kInset, r.T, r.R, r.B);
      default: return r;
    }
  }
};

/** Dark glass panel (GlassPanel): a solid fill, lighter edges, and a faint tilted sheen. */
class GlassPanel final : public BasePanel
{
public:
  GlassPanel(const IRECT& bounds, const IColor& color, int cornerMode)
  : BasePanel(bounds, color, cornerMode)
  {
  }

  void Draw(IGraphics& g) override
  {
    const IRECT r = mRECT.GetPadded(-kInset);
    const float h = r.H();
    const IColor clear = WithAlpha(mColor, 0);

    MakePath(g, r, 5.f);
    g.PathFill(IPattern(mColor));

    MakePath(g, r, 5.f);
    if (mCornerMode == 2)
      PathFillLinear(g, IPattern::CreateLinearGradient(r, EDirection::Vertical, {{IColor(51, 0, 0, 0), 0.f}, {clear, 15.f / h}, {clear, 1.f - 5.f / h}, {AddColor(mColor, 12), 1.f}}));
    else if (mCornerMode == 1)
      PathFillLinear(g, IPattern::CreateLinearGradient(r, EDirection::Vertical, {{AddColor(mColor, 38), 0.f}, {clear, 5.f / h}, {clear, 1.f}}));
    else
      PathFillLinear(g, IPattern::CreateLinearGradient(r, EDirection::Vertical, {{AddColor(mColor, 38), 0.f}, {clear, 5.f / h}, {clear, 1.f - 5.f / h}, {AddColor(mColor, 12), 1.f}}));

    DrawSheen(g, r);
  }

private:
  /** The sheen gradient is rotated by 6 degrees: white 10 to 5 over its first half, then a hard edge.
   * NanoVG cannot clip a rotated band, so the lit part is cut out of the panel shape as a polygon. */
  void DrawSheen(IGraphics& g, const IRECT& r)
  {
    IPattern pattern = IPattern::CreateLinearGradient(r, EDirection::Vertical);
    pattern.mTransform.Rotate(6.f).Translate(-r.H() * 0.5f, 0.f);

    IMatrix inverse = IMatrix(pattern.mTransform).Invert();
    double sx, sy, ex, ey;
    inverse.TransformPoint(sx, sy, 0., 0.);
    inverse.TransformPoint(ex, ey, 0., 1.);
    const float dx = static_cast<float>(ex - sx);
    const float dy = static_cast<float>(ey - sy);

    // The original fades from 5 to 0 between 0.49 and 0.5. Cut halfway, where the anti-aliased edge does the fade.
    const Vertex edge {static_cast<float>(sx) + 0.495f * dx, static_cast<float>(sy) + 0.495f * dy};
    PathPolygon(g, ClipHalfPlane(MakePolygon(r, 5.f), edge, {dx, dy}));
    g.PathFill(IPattern::CreateLinearGradient(static_cast<float>(sx), static_cast<float>(sy), static_cast<float>(sx) + 0.49f * dx, static_cast<float>(sy) + 0.49f * dy,
                                              {{IColor(10, 255, 255, 255), 0.f}, {IColor(5, 255, 255, 255), 1.f}}));
  }
};

/** Lit metal panel (FramePanel) with indented dots in the corners. */
class FramePanel final : public BasePanel
{
public:
  /** gradientRect places the radial light. dotsMode: 1 bottom corners, 2 top corners, 4 all four. */
  FramePanel(const IRECT& bounds, const IColor& color, int cornerMode, const IRECT& gradientRect, int dotsMode)
  : BasePanel(bounds, color, cornerMode)
  , mGradientRect(gradientRect)
  , mDotsMode(dotsMode)
  {
  }

  void Draw(IGraphics& g) override
  {
    const IRECT r = mRECT.GetPadded(-kInset);
    const float h = r.H();

    // Radial light from above the top centre.
    const Outline shape = MakePolygon(r, 5.f);
    MakePath(g, r, 5.f);
    PathFillRadial(g, mGradientRect.MW(), mGradientRect.MH() - mGradientRect.H() / 1.666f, mGradientRect.W() / 1.5f,
                   {{AddColor(mColor, 12), 0.f}, {mColor, 0.33f}, {AddColor(mColor, -12), 1.f}}, &shape);

    // Bevel: a 3 px highlight at the top and a 3 px shade at the bottom.
    const IColor light = mCornerMode == 2 ? mColor : AddColor(mColor, 25);
    const IColor dark = (mCornerMode == 1 || mCornerMode == 2) ? AddColor(mColor, -12) : AddColor(mColor, -24);
    MakePath(g, r, 5.f);
    PathFillLinear(g, IPattern::CreateLinearGradient(r, EDirection::Vertical, {{light, 0.f}, {WithAlpha(light, 0), 3.f / h}, {WithAlpha(dark, 0), 1.f - 3.f / h}, {dark, 1.f}}));

    DrawDots(g, r);
  }

private:
  /** One indent (IndentDot): a dark hole with a light lower rim. */
  static void DrawDot(IGraphics& g, float x, float y)
  {
    constexpr float kRadius = 1.8f;
    const IRECT r(x - kRadius / 2.f, y - kRadius / 2.f, x + kRadius / 2.f, y + kRadius / 2.f);
    g.PathCircle(x, y, kRadius);
    PathFillLinear(g, IPattern::CreateLinearGradient(r, EDirection::Vertical, {{IColor(204, 0, 0, 0), 0.f}, {IColor(127, 0, 0, 0), 1.f}}), true);
    // The original passes the panel fields 8 and 0.1 as the IBlend of this
    // stroke (0x180530c0c), so the rim is added at 10 %.
    PathStrokeAdd(g, IPattern::CreateLinearGradient(r, EDirection::Vertical, {{kColorTransparent, 0.f}, {IColor(255, 255, 255, 255), 1.f}}), 1.f, IStrokeOptions(), 0.1f);
  }

  /** A right triangle of 6 dots (0x180531370). Negative w or h mirrors it. */
  static void DrawCluster(IGraphics& g, float y0, float x0, float w, float h)
  {
    const float m = std::min(h, 0.f);
    DrawDot(g, x0, y0 - m);
    DrawDot(g, x0, y0 + 0.33f * h - m);
    DrawDot(g, x0, y0 + 0.66f * h - m);
    DrawDot(g, x0 + 0.33f * w, y0 + 0.33f * h - m);
    DrawDot(g, x0 + 0.33f * w, y0 + 0.66f * h - m);
    DrawDot(g, x0 + 0.66f * w, y0 + 0.66f * h - m);
  }

  void DrawDots(IGraphics& g, const IRECT& r)
  {
    if (mDotsMode == 1 || mDotsMode == 4)
    {
      DrawCluster(g, r.B - 30.f, r.L + 15.f, 25.f, 25.f);
      DrawCluster(g, r.B - 30.f, r.R - 15.f, -25.f, 25.f);
    }
    if (mDotsMode == 2 || mDotsMode == 4)
    {
      DrawCluster(g, r.T + 5.f, r.L + 15.f, 25.f, -25.f);
      DrawCluster(g, r.T + 5.f, r.R - 15.f, -25.f, -25.f);
    }
  }

  IRECT mGradientRect;
  int mDotsMode;
};

#pragma mark - Header and footer

/** Header (IContainerBase in the original). It ignores the mouse and draws the about overlay. */
class Header final : public IControl
{
public:
  explicit Header(const IRECT& bounds)
  : IControl(bounds)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    if (!mShowAbout)
      return;

    g.FillRect(IColor(191, 7, 7, 7), IRECT(mRECT.L, mRECT.T + 36.f, mRECT.R, mRECT.B));

    // The original's rows: the title block, then three blocks of (320 - 50 - 116) / 3.
    const IRECT title(mRECT.L + 50.f, mRECT.T + 86.f, mRECT.R - 50.f, mRECT.T + 116.f);
    const float step = (mRECT.B - 50.f - title.B) / 3.f;
    const IText body = AboutBodyText();
    auto row = [&](float top, float bottom) { return IRECT(title.L, top, title.R, bottom); };

    g.DrawText(AboutTitleText().WithVAlign(EVAlign::Top), PLUG_NAME, title);
    g.DrawText(body.WithVAlign(EVAlign::Bottom), PLUG_VERSION_STR, title);
    g.DrawText(body, PLUG_COPYRIGHT_STR, row(title.B, title.B + step));
    g.DrawText(body, "Made with iPlug2 Framework", row(title.B + step, title.B + 2.f * step));
    g.DrawText(body.WithVAlign(EVAlign::Bottom), "VST is a registered trademark of", row(title.B + 2.f * step, title.B + 2.5f * step));
    g.DrawText(body.WithVAlign(EVAlign::Top), "Steinberg Media Technologies GmbH", row(title.B + 2.5f * step, title.B + 3.f * step));
  }

  /** The header buttons never grey out, and neither does the header. */
  void SetDisabled(bool disable) override {}

  void ToggleAbout()
  {
    mShowAbout = !mShowAbout;
    SetDirty(false);
  }

  ColorMenu* mColorMenu = nullptr;

private:
  bool mShowAbout = false;
};

/** Footer: the Tooltips switch and the version. It owns the tooltip strip. */
class Footer final : public IControl
{
public:
  explicit Footer(const IRECT& bounds)
  : IControl(bounds)
  {
    mText = FooterText();
    mDblAsSingleClick = true;
  }

  void OnResize() override
  {
    mRECT = IRECT(mRECT.L, mRECT.B - 21.5f, mRECT.R, mRECT.B);
    mTargetRECT = mRECT;
    const float x0 = mRECT.L + 5.f;
    const float half = ((mRECT.R - 5.f) - x0) / 2.f;
    mCircleRect = IRECT(x0, mRECT.T, x0 + 8.f, mRECT.B);
    mLabelRect = IRECT(x0 + 12.f, mRECT.T, x0 + half, mRECT.B);
    mVersionRect = IRECT(x0 + half, mRECT.T, mRECT.R - 5.f, mRECT.B);
  }

  void OnAttached() override
  {
    mTooltipDisplay = new TooltipDisplay(mText.WithAlign(EAlign::Center), mRECT);
    GetUI()->AttachBubbleControl(mTooltipDisplay);
  }

  void Draw(IGraphics& g) override
  {
    g.DrawText(mText.WithAlign(EAlign::Far), PLUG_NAME " " PLUG_VERSION_STR, mVersionRect);
    g.DrawText(mText.WithAlign(EAlign::Near), "Tooltips", mLabelRect);
    const float r = std::min(mCircleRect.W(), mCircleRect.H()) / 2.f;
    g.DrawCircle(kColorFooterText, mCircleRect.MW(), mCircleRect.MH(), r, nullptr, 1.f);
    if (mTooltipDisplay && !mTooltipDisplay->IsDisabled())
      g.FillCircle(kColorFooterText, mCircleRect.MW(), mCircleRect.MH(), 2.f);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    if (mTooltipDisplay && (mCircleRect.Contains(x, y) || mLabelRect.Contains(x, y)))
      mTooltipDisplay->SetDisabled(!mTooltipDisplay->IsDisabled());
    SetDirty(true);
  }

  TooltipDisplay* GetTooltipDisplay() const { return mTooltipDisplay; }

private:
  TooltipDisplay* mTooltipDisplay = nullptr;
  IRECT mCircleRect;
  IRECT mLabelRect;
  IRECT mVersionRect;
};

#pragma mark - Icons

// The port's own icons, drawn as paths. They follow the size, place and grey
// of the original artwork, not its shapes.

IStrokeOptions RoundStroke()
{
  IStrokeOptions options;
  options.mCapOption = ELineCap::Round;
  options.mJoinOption = ELineJoin::Round;
  return options;
}

/** A neutral mark: a wave held flat between two jaws. Grey 208, like the original logo. */
void DrawLogoIcon(IGraphics& g, const IRECT& r)
{
  const IColor color(255, 208, 208, 208);
  const float cx = r.MW();
  const float cy = r.MH();
  constexpr float kThickness = 2.f;

  g.PathMoveTo(cx - 8.5f, cy - 7.5f);
  g.PathLineTo(cx + 8.5f, cy - 7.5f);
  g.PathMoveTo(cx - 8.5f, cy + 7.5f);
  g.PathLineTo(cx + 8.5f, cy + 7.5f);
  g.PathStroke(IPattern(color), kThickness, RoundStroke());

  constexpr int kSteps = 32;
  for (int i = 0; i <= kSteps; i++)
  {
    const float t = static_cast<float>(i) / kSteps;
    const float y = std::clamp(-std::sin(t * 2.f * static_cast<float>(PI)) * 6.f, -3.5f, 3.5f);
    if (i == 0)
      g.PathMoveTo(cx - 7.f, cy + y);
    else
      g.PathLineTo(cx - 7.f + 14.f * t, cy + y);
  }
  g.PathStroke(IPattern(color), kThickness, RoundStroke());
}

/** Power symbol: a ring open at the top with a bar through the gap. Grey 185. */
void DrawPowerIcon(IGraphics& g, const IRECT& r)
{
  const IColor color(255, 185, 185, 185);
  const float cx = r.MW();
  const float cy = r.MH();
  constexpr float kThickness = 2.3f;

  g.PathArc(cx, cy + 0.25f, 6.35f, 35.f, 325.f);
  g.PathStroke(IPattern(color), kThickness, RoundStroke());
  g.PathMoveTo(cx, cy - 6.6f);
  g.PathLineTo(cx, cy - 0.3f);
  g.PathStroke(IPattern(color), kThickness, RoundStroke());
}

/** Question mark. Grey 186. */
void DrawInfoIcon(IGraphics& g, const IRECT& r)
{
  const IColor color(255, 186, 186, 186);
  const float cx = r.MW();
  const float cy = r.MH();

  g.PathArc(cx, cy - 2.8f, 3.55f, -75.f, 125.f);
  g.PathLineTo(cx, cy + 1.1f);
  g.PathLineTo(cx, cy + 2.4f);
  g.PathStroke(IPattern(color), 2.5f, RoundStroke());
  g.FillCircle(color, cx, cy + 6.1f, 1.6f);
}

#pragma mark - Bypass

/** The power button's routine (0x180560e10): grey out every control bound to a parameter, and controls 0 to 2. */
void ApplyBypass(IGraphics* pGraphics, bool bypassed)
{
  const int nParams = pGraphics->GetDelegate()->NParams();
  for (int i = 0; i < nParams; i++)
    pGraphics->ForControlWithParam(i, [bypassed](IControl* pControl) { pControl->SetDisabled(bypassed); });

  for (int i = 0; i < 3 && i < pGraphics->NControls(); i++)
    pGraphics->GetControl(i)->SetDisabled(bypassed);
}
} // namespace

void AttachClampUI(IGraphics* pGraphics)
{
  pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
  pGraphics->AttachControl(new BackgroundPanel(pGraphics->GetBounds(), kColorBackground));
  pGraphics->LoadFont(kTextFont, INTER_REGULAR_FN);
  pGraphics->LoadFont(kLabelFont, INTER_SEMIBOLD_FN);
  pGraphics->LoadFont(kTitleFont, INTER_BOLD_FN);
  pGraphics->EnableMouseOver(true);

  const IVStyle style = ClampStyle();
  const IRECT bounds = pGraphics->GetBounds();

  // The body below the 36 px header and above the 20 px footer, split into a left third and the rest.
  const IRECT b = bounds.GetReducedFromTop(36.f).GetReducedFromBottom(20.f).GetPadded(-1.5f);
  const float splitX = b.R - b.W() / 1.5f;
  const float barT = b.B - 36.f;

  pGraphics->AttachControl(new FramePanel(IRECT(splitX, b.T, b.R, barT), kColorFrame, 1, b, 4));
  pGraphics->AttachControl(new GlassPanel(IRECT(splitX, barT, b.R, b.B), kColorGlass, 2));
  pGraphics->AttachControl(new GlassPanel(IRECT(b.L, b.T, splitX, b.B), kColorGlass, 0));

  // Bottom bar of the right panel: SHAPE, its curve icons, and TYPE.
  pGraphics->AttachControl(new SimpleValueBox(IRECT(splitX + 50.f, barT, splitX + 120.8f, b.B), kShape, style));
  pGraphics->AttachControl(new ShapeDisplay(IRECT(splitX + 20.f, barT, splitX + 35.f, b.B).GetScaledAboutCentre(0.9f), kShape, style, 0, true));
  pGraphics->AttachControl(new ShapeDisplay(IRECT(splitX + 135.8f, barT, splitX + 150.8f, b.B).GetScaledAboutCentre(0.9f), kShape, style, 1, false));
  pGraphics->AttachControl(new SimpleSpinBox(IRECT(splitX + 160.8f, barT, b.R - 20.f, b.B), kType, style))->SetTooltip("Clipper algorithm.");

  // Faders and the CEILING knob.
  pGraphics->AttachControl(new Slider(IRECT(splitX, 87.5f, splitX + 60.f, 212.5f), kInputGain, style))->SetTooltip("Sets the input gain of chosen channel(s)");
  pGraphics->AttachControl(new Slider(IRECT(b.R - 60.f, 87.5f, b.R, 212.5f), kOutputGain, style))->SetTooltip("Sets the output gain of chosen channel(s)");
  pGraphics->AttachControl(new BigKnob((splitX + b.R) / 2.f, 150.f, 140.f, kCeiling, style));

  // Level display, the ceiling line over it, and MODE.
  pGraphics->AttachControl(new DynamicsDisplay(IRECT(b.L, 62.5f, splitX, barT), style, -48.f, 0.f), kCtrlTagDisplay);
  pGraphics->AttachControl(new ThresholdSlider(IRECT(b.L, 52.5f, splitX, barT + 10.f), kCeiling, style));
  pGraphics->AttachControl(new SimpleSpinBox(IRECT(b.L + 20.f, barT, splitX - 20.f, b.B), kMode, style));

  pGraphics->AttachBubbleControl(new ValueDisplay(style.labelText));

  // Header and its children (Header::OnAttached 0x180540da0): the logo, the
  // title, and four 33 px cells from the right: power, info, accent colour
  // and oversampling. Each button is its cell scaled by 0.8.
  auto* pHeader = new Header(bounds);
  pGraphics->AttachControl(pHeader, kCtrlTagHeader);

  const IRECT inner(bounds.L + 3.f, bounds.T + 3.f, bounds.R - 3.f, bounds.T + 36.f);
  const float cell = inner.H();
  const float cellsL = inner.R - 4.f * cell;
  auto button = [&](int i) { return IRECT(cellsL + i * cell, inner.T, cellsL + (i + 1) * cell, inner.B).GetScaledAboutCentre(0.8f); };

  pGraphics->AttachControl(new SVGButton(IRECT(inner.L, inner.T, inner.L + cell, inner.B).GetScaledAboutCentre(0.75f), kNoParameter, DrawLogoIcon, [](IControl* pCaller) {
    // The original opens the author's site. The port credits the author the same way.
    pCaller->GetUI()->OpenURL(kAuthorURL);
  }));

  std::string title = PLUG_NAME;
  std::transform(title.begin(), title.end(), title.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
  pGraphics->AttachControl(new ITextControl(IRECT(inner.L + cell + 5.f, inner.T, inner.R, inner.B), title.c_str(), TitleText(), kColorTransparent));

  pGraphics->AttachControl(new SVGButton(button(3), kBypass, DrawPowerIcon, [](IControl* pCaller) {
    ApplyBypass(pCaller->GetUI(), pCaller->GetValue() >= 0.5);
  }));
  pGraphics->AttachControl(new SVGButton(button(2), kNoParameter, DrawInfoIcon, [pHeader](IControl*) {
    pHeader->ToggleAbout();
  }));
  pHeader->mColorMenu = new ColorMenu(button(1), style);
  pGraphics->AttachControl(pHeader->mColorMenu);
  pGraphics->AttachControl(new ParamMenu(button(0), kOversampling, style));

  // Like Header::OnAttached, grey out the controls if the editor opens bypassed.
  if (pGraphics->GetDelegate()->GetParam(kBypass)->Value() >= 0.5)
    ApplyBypass(pGraphics, true);

  pGraphics->AttachControl(new Footer(bounds), kCtrlTagFooter);
}

void ApplyClampSettings(IGraphics* pGraphics, const ClampSettings& settings)
{
  if (!pGraphics)
    return;

  pGraphics->Resize(PLUG_WIDTH, PLUG_HEIGHT, settings.scale);

  if (auto* pHeader = dynamic_cast<Header*>(pGraphics->GetControlWithTag(kCtrlTagHeader)); pHeader && pHeader->mColorMenu)
    pHeader->mColorMenu->SetSelected(settings.color);

  if (auto* pFooter = dynamic_cast<Footer*>(pGraphics->GetControlWithTag(kCtrlTagFooter)); pFooter && pFooter->GetTooltipDisplay())
    pFooter->GetTooltipDisplay()->SetDisabled(settings.tooltips == 0);
}

ClampSettings CollectClampSettings(IGraphics* pGraphics)
{
  ClampSettings settings;
  settings.scale = pGraphics->GetDrawScale();

  if (auto* pHeader = dynamic_cast<Header*>(pGraphics->GetControlWithTag(kCtrlTagHeader)); pHeader && pHeader->mColorMenu)
    settings.color = pHeader->mColorMenu->GetSelected();

  if (auto* pFooter = dynamic_cast<Footer*>(pGraphics->GetControlWithTag(kCtrlTagFooter)); pFooter && pFooter->GetTooltipDisplay())
    settings.tooltips = pFooter->GetTooltipDisplay()->IsDisabled() ? 0 : 1;

  return settings;
}
