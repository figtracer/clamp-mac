#include "ClampControls.h"

#include "ISender.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace clampui
{
namespace
{
/** The light-theme test of BigKnob and Slider: min + max of the kBG channels. Always false with Clamp's grey 50. */
bool IsLight(const IColor& bg)
{
  const int lo = std::min({bg.R, bg.G, bg.B});
  const int hi = std::max({bg.R, bg.G, bg.B});
  return lo + hi >= 200;
}

IStrokeOptions RoundCap(bool preserve = false)
{
  IStrokeOptions options;
  options.mCapOption = ELineCap::Round;
  options.mPreserve = preserve;
  return options;
}

IFillOptions Preserve()
{
  IFillOptions options;
  options.mPreserve = true;
  return options;
}

/** The accent (kX1), or kX2 while the control is disabled. */
IColor Accent(const IControl& control, const IVectorBase& vector)
{
  return vector.GetColor(control.IsDisabled() ? kX2 : kX1);
}
} // namespace

#pragma mark - Hover helpers

void ShowFooterTooltip(IControl* pCaller)
{
  IGraphics* pGraphics = pCaller->GetUI();
  for (int i = 0; IBubbleControl* pBubble = pGraphics->GetBubbleControl(i); i++)
  {
    if (auto* pTooltip = dynamic_cast<TooltipDisplay*>(pBubble))
    {
      pTooltip->ShowTooltip(pCaller);
      return;
    }
  }
}

void ShowValueBubble(IControl* pCaller, float x, float y)
{
  IGraphics* pGraphics = pCaller->GetUI();
  for (int i = 0; IBubbleControl* pBubble = pGraphics->GetBubbleControl(i); i++)
  {
    if (auto* pValue = dynamic_cast<ValueDisplay*>(pBubble))
    {
      pValue->Show(pCaller, x, y);
      return;
    }
  }
}

#pragma mark - BigKnob

BigKnob::BigKnob(float cx, float cy, float size, int paramIdx, const IVStyle& style)
: IKnobControlBase(IRECT(cx - size / 2.f, cy - size / 2.f, cx + size / 2.f, cy + size / 2.f + 20.f), paramIdx, EDirection::Vertical, 4.)
, IVectorBase(style)
{
  AttachIControl(this, "");
  mHideCursorOnDrag = false;
}

void BigKnob::OnResize()
{
  SetTargetRECT(mRECT);
  mKnobRect = IRECT(mRECT.L, mRECT.T, mRECT.R, mRECT.B - 20.f);
  mLabelRect = IRECT(mRECT.L, mRECT.B - 20.f, mRECT.R, mRECT.B);
  SetDirty(false);
}

bool BigKnob::IsHit(float x, float y) const
{
  const float r = std::min(mKnobRect.W(), mKnobRect.H()) / 2.f;
  const float dx = x - mKnobRect.MW();
  const float dy = y - mKnobRect.MH();
  return dx * dx + dy * dy <= r * r;
}

IRECT BigKnob::GetKnobDragBounds()
{
  // IVKnobControl: a widget taller than wide drags over W x W/2, so 280 px cover the range at gearing 4.
  if (mRECT.W() > mRECT.H())
    return mRECT.GetCentredInside(mRECT.H() / 2.f, mRECT.H());
  return mRECT.GetCentredInside(mRECT.W(), mRECT.W() / 2.f);
}

void BigKnob::OnMouseOver(float x, float y, const IMouseMod& mod)
{
  IKnobControlBase::OnMouseOver(x, y, mod);
  ShowFooterTooltip(this);
  ShowValueBubble(this, mKnobRect.MW(), mKnobRect.T);
}

void BigKnob::Draw(IGraphics& g)
{
  const IColor bg = GetColor(kBG);
  const IColor sh = GetColor(kSH);
  const IColor fg = GetColor(kFG);
  const IColor accent = Accent(*this, *this);
  const bool light = IsLight(bg);

  const IColor c1 = LerpColor(bg, sh, light ? 0.10f : 0.25f);
  const IColor c2 = LerpColor(bg, AddColor(bg, 63), light ? 0.75f : 0.50f);
  const IColor c3 = AddColor(bg, 63);

  const float cx = mKnobRect.MW();
  const float cy = mKnobRect.MH();
  const float r1 = std::min(mKnobRect.W(), mKnobRect.H()) / 2.f * 0.9f;
  const float r2 = r1 * 0.9f;
  const float k = std::sqrt(r2) * 0.45f;
  const float d = r2 - 3.f * k;
  const float off = r1 * 0.01f * 10.f;
  const float side = std::max(1.f, d);
  const IRECT box(cx - side / 2.f, cy - side / 2.f, cx + side / 2.f, cy + side / 2.f);

  const float angle = static_cast<float>(GetValue()) * 270.f - 135.f;
  constexpr float kAnchor = 135.f;

  // Drop shadow and top rim light.
  g.PathCircle(cx, cy + off, r1);
  PathFillRadial(g, cx, cy + off, r1, {{WithAlpha(sh, 84), 0.f}, {WithAlpha(sh, 0), 0.9f}, {WithAlpha(sh, 0), 1.f}});
  g.PathCircle(cx, cy - off, r1);
  PathFillRadial(g, cx, cy - off, r1, {{WithAlpha(c2, 127), 0.f}, {WithAlpha(c2, 0), 0.9f}, {WithAlpha(c2, 0), 1.f}});

  // Outer body and face.
  g.PathCircle(cx, cy, r1);
  PathFillLinear(g, IPattern::CreateLinearGradient(box, EDirection::Vertical, {{AddColor(c1, -6), 0.f}, {bg, 1.f}}));
  g.PathCircle(cx, cy, r2);
  PathFillRadial(g, cx, cy, r2, {{bg, 0.f}, {c1, 1.f - (r1 - r2) / r2}, {sh, 1.f}});

  // Value arc from the value to the 0 dB end, with an additive glow.
  if (std::min(angle, kAnchor) != std::max(angle, kAnchor))
  {
    g.PathArc(cx, cy, r2, std::min(angle, kAnchor), std::max(angle, kAnchor));
    const IColor glow = WithAlpha(AddColor(accent, static_cast<int>((light ? 0.f : -0.45f) * 255.f)), 20);
    for (int i = 1; i <= 5; i++)
    {
      const float width = i / 5.f * (5.f * k - k / 2.f) + k / 2.f;
      if (light)
        g.PathStroke(IPattern(glow), width, RoundCap(true));
      else
        PathStrokeAdd(g, IPattern(glow), width, RoundCap(true), 1.f);
    }
    g.PathStroke(IPattern(ToWhite(accent, 0.33f)), k / 2.f, RoundCap());
  }

  // The label shows the value while the pointer is over the knob.
  WDL_String label;
  if (GetMouseIsOver())
    label = DisplayWithLabel(*this);
  else if (GetParam())
    label.Set(GetParam()->GetName());
  g.DrawText(GetStyle().labelText.WithFGColor(fg), label.Get(), mLabelRect);

  // Cap shadow, cap and rim.
  g.PathCircle(cx, cy + 0.8f * d, d);
  g.PathFill(RadialGradient(cx, cy + 0.65f * d, d, sh, 0.f, WithAlpha(sh, 0), 1.f));
  g.PathCircle(cx, cy, 0.95f * d);
  PathFillLinear(g, IPattern::CreateLinearGradient(box, EDirection::Vertical, {{bg, 0.f}, {c1, 1.f}}), true);
  PathStrokeLinear(g, IPattern::CreateLinearGradient(box, EDirection::Vertical, {{c2, 0.f}, {c1, 1.f}}), 1.25f);

  // Pointer.
  const float rad = (angle - 90.f) / 180.f * static_cast<float>(PI);
  g.PathMoveTo(cx + 0.68f * d * std::cos(rad), cy + 0.68f * d * std::sin(rad));
  g.PathLineTo(cx + d * std::cos(rad), cy + d * std::sin(rad));
  g.PathStroke(IPattern(WithAlpha(fg, 168)), 2.f, RoundCap());

  // Sheen.
  g.PathCircle(cx, cy, r1);
  g.PathFill(RadialGradient(cx, cy - 0.9f * d, d, WithAlpha(c3, 25), 0.f, WithAlpha(c3, 0), 1.f));
}

#pragma mark - Slider

Slider::Slider(const IRECT& bounds, int paramIdx, const IVStyle& style)
: ISliderControlBase(bounds, paramIdx, EDirection::Vertical, 4., 30.f)
, IVectorBase(style)
{
  AttachIControl(this, "");
  mHideCursorOnDrag = false;
}

void Slider::OnResize()
{
  // A column as wide as the handle, centred in the layout rect.
  mRECT = mRECT.GetCentredInside(mHandleSize, mRECT.H());
  ISliderControlBase::OnResize();
  const float s = std::min(mRECT.W(), mRECT.H());
  mTrackBounds = IRECT(mRECT.L, mRECT.T + s / 3.f, mRECT.R, mRECT.B - s / 3.f);
}

bool Slider::IsHit(float x, float y) const
{
  const float cx = mTrackBounds.MW();
  return IRECT(cx - 15.f, mTrackBounds.T, cx + 15.f, mTrackBounds.B).Contains(x, y);
}

void Slider::OnMouseOver(float x, float y, const IMouseMod& mod)
{
  ISliderControlBase::OnMouseOver(x, y, mod);
  ShowFooterTooltip(this);
  ShowValueBubble(this, mRECT.MW(), mRECT.T);
}

void Slider::Draw(IGraphics& g)
{
  const IColor bg = GetColor(kBG);
  const IColor sh = GetColor(kSH);
  const IColor fg = GetColor(kFG);
  const IColor accent = Accent(*this, *this);
  const bool light = IsLight(bg);

  const IColor s1 = LerpColor(bg, sh, light ? 0.10f : 0.25f);
  const IColor s2 = LerpColor(bg, sh, light ? 0.20f : 0.35f);
  const IColor s3 = LerpColor(bg, AddColor(bg, 63), light ? 0.75f : 0.50f);

  const IRECT& track = mTrackBounds;
  const float cx = track.MW();
  const float w = mHandleSize * 0.25f;
  const float v = static_cast<float>(GetValue());
  const float hy = track.B - track.H() * v;
  const float ay = track.MH();

  // Track slot.
  const IRECT slot(cx - w / 2.f, track.T - w / 2.f, cx + w / 2.f, track.B + w / 2.f);
  const float t = (w / 2.5f) / slot.H();
  g.PathRoundRect(slot, w / 2.f);
  PathFillLinear(g, IPattern::CreateLinearGradient(mRECT, EDirection::Vertical, {{sh, 0.f}, {s2, 1.f}}), true);
  PathStrokeLinear(g, IPattern::CreateLinearGradient(slot, EDirection::Vertical, {{s2, 0.f}, {s1, t}, {s1, 1.f - t}, {s3, 1.f}}), 1.f);

  // Fill line from the centre to the handle, with an additive glow.
  const float glowMin = std::sqrt(w) * 0.5f;
  g.PathMoveTo(cx, hy);
  g.PathLineTo(cx, ay);
  const IColor glow = WithAlpha(AddColor(accent, light ? 0 : -114), 25);
  for (int i = 1; i <= 4; i++)
  {
    const float width = i * 0.25f * (w - glowMin) + glowMin;
    if (light)
      g.PathStroke(IPattern(glow), width, RoundCap(true));
    else
      PathStrokeAdd(g, IPattern(glow), width, RoundCap(true), 1.f);
  }
  g.PathStroke(IPattern(ToWhite(accent, 0.25f)), glowMin, RoundCap());

  // Handle shadow: five rounded rects that grow from the centre.
  const IRECT handle(cx - mHandleSize / 2.f, hy - mHandleSize / 5.f, cx + mHandleSize / 2.f, hy + mHandleSize / 5.f);
  const float u = std::min(handle.W(), handle.H()) * 0.25f;
  const IRECT shadow = handle.GetTranslated(0.f, 1.75f * u);
  const IRECT inner(cx - 0.33f * handle.W() / 2.f, shadow.MH() - 0.33f * handle.H() / 2.f, cx + 0.33f * handle.W() / 2.f, shadow.MH() + 0.33f * handle.H() / 2.f);
  for (int i = 0; i < 5; i++)
  {
    const float f = (i + 1) / 5.f;
    const IRECT r(inner.L + f * (shadow.L - inner.L), inner.T + f * (shadow.T - inner.T), inner.R + f * (shadow.R - inner.R), inner.B + f * (shadow.B - inner.B));
    g.FillRoundRect(WithAlpha(sh, 25), r, std::min(r.W(), r.H()) / 2.f);
  }

  // Handle body, inset and grip line.
  g.PathRoundRect(handle, 3.f);
  PathFillLinear(g, IPattern::CreateLinearGradient(handle, EDirection::Vertical, {{s3, 0.f}, {bg, 0.3f}, {s1, 0.6f}, {AddColor(s1, -25), 1.f}}));
  g.PathRoundRect(IRECT(cx - 12.f, hy - 4.8f, cx + 12.f, hy + 4.8f), 3.f);
  PathFillLinear(g, IPattern::CreateLinearGradient(handle, EDirection::Vertical, {{bg, 0.f}, {s1, 1.f}}));
  g.PathMoveTo(cx - w, hy);
  g.PathLineTo(cx + w, hy);
  g.PathStroke(IPattern(fg), 1.5f, RoundCap());
}

#pragma mark - SimpleValueBox

SimpleValueBox::SimpleValueBox(const IRECT& bounds, int paramIdx, const IVStyle& style)
: IKnobControlBase(bounds, paramIdx, EDirection::Vertical, 4.)
, IVectorBase(style)
{
  AttachIControl(this, "");
  mHideCursorOnDrag = false;
}

IRECT SimpleValueBox::GetKnobDragBounds()
{
  // IVKnobControl: a widget wider than tall drags over H/2 x H, so 144 px cover the range at gearing 4.
  if (mRECT.W() > mRECT.H())
    return mRECT.GetCentredInside(mRECT.H() / 2.f, mRECT.H());
  return mRECT.GetCentredInside(mRECT.W(), mRECT.W() / 2.f);
}

void SimpleValueBox::OnMouseOver(float x, float y, const IMouseMod& mod)
{
  IKnobControlBase::OnMouseOver(x, y, mod);
  ShowFooterTooltip(this);
}

void SimpleValueBox::Draw(IGraphics& g)
{
  const IRECT labelRect(mRECT.L, mRECT.T, mRECT.L + mRECT.W() / 3.f, mRECT.B);
  const IRECT valueRect(mRECT.L + mRECT.W() / 3.f, mRECT.T, mRECT.R - 10.f, mRECT.B);
  const IText& valueText = GetStyle().valueText;

  if (const IParam* pParam = GetParam())
    g.DrawText(valueText.WithSize(9.f).WithAlign(EAlign::Near), pParam->GetName(), labelRect);
  g.DrawText(valueText.WithFGColor(Accent(*this, *this)).WithAlign(EAlign::Center), DisplayWithLabel(*this).Get(), valueRect);

  // Up and down marks. They light up while the pointer is over the control.
  const IColor color = WithAlpha(GetColor(kHL), GetMouseIsOver() ? 255 : 127);
  const float my = mRECT.MH();
  const float h = 8.f / 3.f;
  const IRECT up(mRECT.R - 6.f, my - 4.f, mRECT.R, my - 4.f + h);
  const IRECT down(mRECT.R - 6.f, my + 4.f - h, mRECT.R, my + 4.f);
  g.PathMoveTo(up.L, up.B);
  g.PathLineTo(up.MW(), up.T);
  g.PathLineTo(up.R, up.B);
  g.PathStroke(IPattern(color), 1.f);
  g.PathMoveTo(down.L, down.T);
  g.PathLineTo(down.MW(), down.B);
  g.PathLineTo(down.R, down.T);
  g.PathStroke(IPattern(color), 1.f);
}

#pragma mark - ShapeDisplay

ShapeDisplay::ShapeDisplay(const IRECT& bounds, int paramIdx, const IVStyle& style, int type, bool invert)
: IControl(bounds, paramIdx)
, IVectorBase(style)
, mType(type)
, mInvert(invert)
{
  AttachIControl(this, "");
  mIgnoreMouse = true;
}

void ShapeDisplay::Draw(IGraphics& g)
{
  const float s = std::min(mRECT.W(), mRECT.H()) / 2.f;
  const float cx = mRECT.MW();
  const float cy = mRECT.MH();
  const float x0 = cx - 0.9f * s;
  const float x1 = cx + 0.9f * s;
  const float y0 = cy - 0.45f * s;
  const float y1 = cy + 0.45f * s;

  // Dim grey at one end of SHAPE, the accent at the other.
  const float t = mInvert ? 1.f - static_cast<float>(GetValue()) : static_cast<float>(GetValue());
  const IColor color = LerpColor(IColor(127, 127, 127, 127), Accent(*this, *this), t);

  if (mType == 0)
  {
    g.PathMoveTo(x0, y1);
    g.PathCubicBezierTo(x0, y0, x1, y0, x1, y0);
    g.PathStroke(IPattern(color), 1.25f);
  }
  else
  {
    const float xm = (x0 + x1) / 2.f;
    g.DrawLine(color, x0, y1, xm, y0, nullptr, 1.25f);
    g.DrawLine(color, xm, y0, x1, y0, nullptr, 1.25f);
  }
}

#pragma mark - SimpleSpinBox

SimpleSpinBox::SimpleSpinBox(const IRECT& bounds, int paramIdx, const IVStyle& style)
: IControl(bounds, paramIdx)
, IVectorBase(style)
{
  AttachIControl(this, "");

  // A user change slides the carousel for 400 ms (0x18055c180).
  SetActionFunction([this](IControl*) {
    SetAnimation([this](IControl*) { OnAnimationTick(); }, 400);
  });
}

void SimpleSpinBox::OnInit()
{
  if (const IParam* pParam = GetParam())
    mNumItems = static_cast<int>(pParam->GetMax() - pParam->GetMin()) + 1;
}

void SimpleSpinBox::OnAnimationTick()
{
  // A per-frame recurrence, as in the original (0x18055c1f0): the step grows with the elapsed time.
  const float progress = static_cast<float>(GetAnimationProgress());
  mPosition = (static_cast<float>(GetValue()) - mPosition) * progress + mPosition;
  mAnimating = true;
  if (progress > 1.f)
  {
    OnEndAnimation();
    mAnimating = false;
  }
}

void SimpleSpinBox::SetDirty(bool triggerAction, int valIdx)
{
  IControl::SetDirty(triggerAction, valIdx);

  // A change from the host or a preset moves the carousel at once, unless it is sliding.
  if (!triggerAction && !mAnimating)
    mPosition = static_cast<float>(GetValue());
}

void SimpleSpinBox::OnMouseDown(float x, float y, const IMouseMod& mod)
{
  const double step = 1. / (mNumItems - 1);
  const double value = mHoverDirection < 0 ? GetValue() - step : GetValue() + step;
  SetValue(std::clamp(value, 0., 1.));
  SetDirty(true);
}

void SimpleSpinBox::OnMouseUp(float x, float y, const IMouseMod& mod)
{
  SetDirty(false);
}

void SimpleSpinBox::OnMouseOver(float x, float y, const IMouseMod& mod)
{
  const float mid = (mRECT.L + mRECT.R) * 0.5f;
  mHoverDirection = 0;
  if (x < mid)
    mHoverDirection = -1;
  if (x > mid)
    mHoverDirection = 1;
  SetDirty(false);
  ShowFooterTooltip(this);
  IControl::OnMouseOver(x, y, mod);
}

void SimpleSpinBox::OnMouseOut()
{
  mHoverDirection = 0;
  IControl::OnMouseOut();
  SetDirty(false);
}

void SimpleSpinBox::DrawItem(IGraphics& g, int index, const IRECT& r, float alpha, const IColor& color)
{
  const char* str = GetParam() ? GetParam()->GetDisplayText(index) : "";
  if (!CStringHasContents(str))
    return;

  // Long texts shrink to fit, down to half size.
  const IText& base = GetStyle().valueText;
  IRECT measured;
  const float width = g.MeasureText(base, str, measured);
  const float scale = width > 0.f ? std::clamp(r.W() / width, 0.5f, 1.f) : 1.f;
  const int a = static_cast<int>(std::clamp(alpha, 0.f, 1.f) * 255.f);
  g.DrawText(base.WithSize(base.mSize * scale).WithFGColor(WithAlpha(color, a)), str, r);
}

void SimpleSpinBox::Draw(IGraphics& g)
{
  const float v = static_cast<float>(GetValue());
  if (mPosition < 0.f)
    mPosition = v;

  const IRECT& r = mRECT;
  const float my = (r.T + r.B) / 2.f;
  const IRECT leftMark(r.L + 2.f, my - 4.f, r.L + 6.f, my + 4.f);
  const IRECT textArea(r.L + 4.f, r.T, r.R - 4.f, r.B);
  const IRECT rightMark(r.R - 6.f, my - 4.f, r.R - 2.f, my + 4.f);
  const IColor hl = GetColor(kHL);

  // A dot at either end of the list, else a chevron. The hovered side is brighter.
  const IColor leftColor = WithAlpha(hl, mHoverDirection == -1 ? hl.A : 127);
  if (v <= 0.0001f)
  {
    g.FillCircle(leftColor, leftMark.MW(), leftMark.MH(), 1.f);
  }
  else
  {
    g.PathMoveTo(leftMark.R, leftMark.T);
    g.PathLineTo(leftMark.L, my);
    g.PathLineTo(leftMark.R, leftMark.B);
    g.PathStroke(IPattern(leftColor), 1.f);
  }

  const IColor rightColor = WithAlpha(hl, mHoverDirection == 1 ? hl.A : 127);
  if (v >= 1.f)
  {
    g.FillCircle(rightColor, rightMark.MW(), rightMark.MH(), 1.f);
  }
  else
  {
    g.PathMoveTo(rightMark.L, rightMark.T);
    g.PathLineTo(rightMark.R, my);
    g.PathLineTo(rightMark.L, rightMark.B);
    g.PathStroke(IPattern(rightColor), 1.f);
  }

  // Carousel: items side by side, faded by their distance from the centre.
  const float w = textArea.W();
  const float start = textArea.L - (mNumItems - 1) * w * mPosition;
  const IColor color = Accent(*this, *this);
  for (int i = 0; i < mNumItems; i++)
  {
    const IRECT item(start + i * w, textArea.T, start + (i + 1) * w, textArea.B);
    const IRECT inner = item.GetHPadded(-4.f);
    const float fade = std::clamp(std::fabs(textArea.MW() - inner.MW()) / (inner.W() / 2.f), 0.f, 1.f);
    DrawItem(g, i, inner, 1.f - fade, color);
  }
}

#pragma mark - DropMenu

DropMenu::DropMenu(const IRECT& bounds, int paramIdx, const IVStyle& style)
: IControl(bounds, paramIdx)
, IVectorBase(style)
{
  AttachIControl(this, "");
}

void DropMenu::InitRows(int numItems)
{
  mNumItems = std::max(1, numItems);
  const float rowHeight = std::min(mRECT.W(), mRECT.H());
  mRECT.B = mRECT.T + rowHeight * mNumItems;
}

IRECT DropMenu::GetRow(int index) const
{
  const float rowHeight = (mRECT.B - mRECT.T) / mNumItems;
  return IRECT(mRECT.L, mRECT.T + index * rowHeight, mRECT.R, mRECT.T + (index + 1) * rowHeight);
}

int DropMenu::RowAt(float x, float y) const
{
  for (int i = 0; i < mNumItems; i++)
  {
    if (GetRow(i).Contains(x, y))
      return i;
  }
  return -1;
}

void DropMenu::OnResize()
{
  SetTargetRECT(GetRow(0));
  mExpanded = false;
}

void DropMenu::Draw(IGraphics& g)
{
  g.PathRoundRect(mTargetRECT, 5.f);
  IFillOptions fill;
  fill.mPreserve = mExpanded;
  g.PathFill(IPattern(kColorBackground), fill);

  if (!mExpanded)
  {
    DrawItem(g, mSelected, GetRow(0));
    return;
  }

  g.PathStroke(IPattern(GetColor(kFG)), 1.f);
  for (int i = 0; i < mNumItems; i++)
    DrawItem(g, i, GetRow(i));
}

void DropMenu::OnMouseOver(float x, float y, const IMouseMod& mod)
{
  // The list opens on hover and covers all rows until the pointer leaves it.
  mExpanded = true;
  mTargetRECT = mRECT;
  const int row = RowAt(x, y);
  mHovered = row >= 0 ? row : mNumItems - 1;
  mMouseIsOver = false;
  SetDirty(false);
}

void DropMenu::OnMouseOut()
{
  mExpanded = false;
  mHovered = -1;
  mTargetRECT = GetRow(0);
  mMouseIsOver = false;
  SetDirty(false);
}

void DropMenu::OnMouseDown(float x, float y, const IMouseMod& mod)
{
  if (mPressed)
    return;

  if (mExpanded)
  {
    const int row = RowAt(x, y);
    if (row >= 0)
    {
      mSelected = row;
      OnSelect();
    }
  }
  mPressed = true;
}

void DropMenu::OnMouseUp(float x, float y, const IMouseMod& mod)
{
  mPressed = false;
}

#pragma mark - ParamMenu

ParamMenu::ParamMenu(const IRECT& bounds, int paramIdx, const IVStyle& style)
: DropMenu(bounds, paramIdx, style)
{
}

void ParamMenu::OnInit()
{
  if (const IParam* pParam = GetParam())
  {
    mSelected = static_cast<int>(pParam->Value());
    InitRows(static_cast<int>(pParam->GetMax()) + 1);
  }
}

void ParamMenu::SetDirty(bool triggerAction, int valIdx)
{
  // Like the original, only a change from outside the menu re-reads the shown item.
  if (!triggerAction && GetParam())
    mSelected = static_cast<int>(GetParam()->Value());
  IControl::SetDirty(triggerAction, valIdx);
}

void ParamMenu::DrawItem(IGraphics& g, int index, const IRECT& r)
{
  float alpha = 0.5f;
  if (index == mHovered)
    alpha = 0.8f;
  if (index == mSelected)
    alpha = 1.f;
  if (IsDisabled())
    alpha = 0.25f;

  const char* str = GetParam() ? GetParam()->GetDisplayText(index) : "";
  if (!CStringHasContents(str))
    return;

  const IText& text = GetStyle().labelText;
  g.DrawText(text.WithFGColor(WithAlpha(text.mFGColor, static_cast<int>(std::clamp(alpha, 0.f, 1.f) * 255.f))), str, r);
}

void ParamMenu::OnSelect()
{
  SetValue(mSelected / static_cast<double>(mNumItems - 1));
  SetDirty(true);
}

#pragma mark - ColorMenu

ColorMenu::ColorMenu(const IRECT& bounds, const IVStyle& style)
: DropMenu(bounds, kNoParameter, style)
{
  mSelected = 4;
}

void ColorMenu::OnInit()
{
  InitRows(kNumAccentColors);
}

void ColorMenu::SetSelected(int index)
{
  mSelected = std::clamp(index, 0, kNumAccentColors - 1);
  OnSelect();
}

void ColorMenu::DrawItem(IGraphics& g, int index, const IRECT& r)
{
  const IColor& color = kAccentColors[index];
  const float m = std::min(r.W(), r.H());
  if (index == mSelected)
    g.DrawCircle(color, r.MW(), r.MH(), m * 0.25f, nullptr, 1.f);
  g.FillCircle(color, r.MW(), r.MH(), m / 6.f);
}

void ColorMenu::OnSelect()
{
  // Apply (0x18055c0f0): write the colour into kX1 of every vector control.
  const IColor color = kAccentColors[mSelected];
  GetUI()->ForAllControlsFunc([color](IControl* pControl) {
    if (auto* pVector = dynamic_cast<IVectorBase*>(pControl))
      pVector->SetColor(kX1, color);
  });
}

#pragma mark - ValueDisplay

ValueDisplay::ValueDisplay(const IText& text)
: IBubbleControl(text, COLOR_BLACK, COLOR_BLACK, 5.f)
{
}

void ValueDisplay::Show(IControl* pCaller, float x, float y)
{
  if (IsHidden())
  {
    Hide(false);
    mState = kExpanding;
    SetDirty(true);
  }

  if (pCaller != mCaller)
  {
    // The original uses {x - 50, y - 40, x + 50, y}. The port widens the
    // redraw area so that a long value is not clipped.
    // The port also redraws the old place, as IBubbleControl::ShowBubble does.
    SetTargetAndDrawRECTs(IRECT(x - 80.f, y - 40.f, x + 80.f, y));
    mCaller = pCaller;
    mX = x;
    mY = y;
    GetUI()->SetAllControlsDirty();
  }
}

void ValueDisplay::Draw(IGraphics& g)
{
  if (!mCaller || !mCaller->GetParam())
    return;

  // Stay fully shown while the control is hovered.
  if (mCaller->GetMouseIsOver())
    mState = kExpanded;

  WDL_String str;
  str.SetFormatted(128, "%s: %s", mCaller->GetParam()->GetName(), DisplayWithLabel(*mCaller).Get());

  IRECT box(mX - 50.f, mY - 35.f, mX + 50.f, mY - 5.f);
  g.MeasureText(mText, str.Get(), box);
  const float width = static_cast<float>(GetUI()->Width());
  if (box.L < 20.f)
    box.Translate(20.f - box.L, 0.f);
  if (box.R > width - 20.f)
    box.Translate(width - 20.f - box.R, 0.f);

  g.FillRoundRect(COLOR_BLACK, IRECT(box.L - 4.f, box.T - 2.f, box.R + 4.f, box.B + 2.f), 5.f, &mBlend);
  g.FillTriangle(COLOR_BLACK, mX - 8.f, mY - 20.f, mX + 8.f, mY - 20.f, mX, mY - 4.f, &mBlend);
  g.DrawText(mText.WithFGColor(COLOR_WHITE), str.Get(), box, &mBlend);
}

#pragma mark - TooltipDisplay

TooltipDisplay::TooltipDisplay(const IText& text, const IRECT& footerRect)
: IBubbleControl(text, COLOR_BLACK, COLOR_BLACK, 5.f)
, mFooterRect(footerRect)
{
}

void TooltipDisplay::OnResize()
{
  mRECT = mFooterRect;
  mTargetRECT = mFooterRect;
}

void TooltipDisplay::SetDisabled(bool disable)
{
  mCaller = nullptr;
  mDisabled = disable;
  mBlend.mWeight = disable ? GRAYED_ALPHA : 1.f;
  SetDirty(false);
}

void TooltipDisplay::ShowTooltip(IControl* pCaller)
{
  if (mState == kCollapsed)
  {
    Hide(false);
    SetDirty(true);
  }

  if (pCaller != mCaller)
    SetTargetAndDrawRECTs(mFooterRect);

  mCaller = pCaller;
  mStr.Set("");

  // A control without a tooltip clears the strip.
  const char* tooltip = pCaller->GetTooltip();
  if (!CStringHasContents(tooltip) || !pCaller->GetParam())
  {
    mCaller = nullptr;
    return;
  }

  mStr.SetFormatted(256, "%s : %s", pCaller->GetParam()->GetName(), tooltip);
}

void TooltipDisplay::Draw(IGraphics& g)
{
  if (IsDisabled() || !mCaller)
    return;

  // Stay while the control is hovered. The strip has no fade.
  if (mCaller->GetMouseIsOver())
    mState = kExpanded;

  g.FillRect(kColorBackground, mFooterRect);
  g.DrawText(mText, mStr.Get(), mFooterRect);
}

#pragma mark - DynamicsDisplay

DynamicsDisplay::DynamicsDisplay(const IRECT& bounds, const IVStyle& style, float minDB, float maxDB)
: IControl(IRECT(bounds.L + 1.5f, bounds.T - 10.f, bounds.R - 1.5f, bounds.B + 10.f))
, IVectorBase(style)
, mMinDB(minDB)
, mMaxDB(maxDB)
{
  AttachIControl(this, "");

  // Grid labels every 6 dB, built only for a display taller than 100 px.
  mCompact = bounds.H() <= 100.f;
  if (!mCompact)
  {
    const int n = static_cast<int>(std::fabs(maxDB) + std::fabs(minDB)) + 1;
    for (int i = 0; i < n; i++)
    {
      const int v = static_cast<int>((maxDB - minDB) * (static_cast<float>(i) / static_cast<float>(n)) + minDB);
      if (v % 6 == 0)
        mGrid.push_back(v);
    }
  }
}

void DynamicsDisplay::OnResize()
{
  SetTargetRECT(mRECT);
  mPlot = IRECT(mRECT.L, mRECT.T + 10.f, mRECT.R, mRECT.B - 10.f);
  SetDirty(false);
}

void DynamicsDisplay::OnMsgFromDelegate(int msgTag, int dataSize, const void* pData)
{
  if (IsDisabled() || msgTag != ISender<>::kUpdateMessage)
    return;

  float post = 0.f;
  float pre = 0.f;
  if (dataSize >= static_cast<int>(sizeof(ISenderData<2, float>)))
  {
    const auto* pPacket = static_cast<const ISenderData<2, float>*>(pData);
    post = pPacket->vals[0];
    pre = pPacket->vals[1];
  }

  // dB scaled to 0..1 over the range. Silence gives -inf, which draws at the bottom.
  const double range = std::fabs(mMaxDB) + std::fabs(mMinDB);
  const double nPost = (std::log(static_cast<double>(post)) * 8.685889638065037 + std::fabs(mMinDB)) / range;
  const double nPre = (std::log(static_cast<double>(pre)) * 8.685889638065037 + std::fabs(mMinDB)) / range;
  mPost[mPostWrite] = static_cast<float>(nPost);
  mPostWrite = (mPostWrite + 1) % kNumPoints;
  mPre[mPreWrite] = static_cast<float>(nPre);
  mPreWrite = (mPreWrite + 1) % kNumPoints;
  SetDirty(false);
}

void DynamicsDisplay::PathTrace(IGraphics& g, const std::array<float, kNumPoints>& ring, int writeIndex) const
{
  // Oldest point at the left, newest at the right, 1 px outside the plot on each side.
  const float x0 = mPlot.L - 1.f;
  const float xR = mPlot.R + 1.f;
  const float top = mPlot.T + 1.f;
  const float bottom = mPlot.B - 1.f;
  const float h = bottom - top;

  g.PathMoveTo(x0, bottom);
  for (int i = 0; i < kNumPoints; i++)
  {
    const float v = std::fmax(ring[(writeIndex + i) % kNumPoints], 0.f);
    g.PathLineTo(x0 + static_cast<float>(i) / (kNumPoints - 1) * (xR - x0), bottom - v * h);
  }
  g.PathLineTo(xR, bottom - 1.f);
}

void DynamicsDisplay::Draw(IGraphics& g)
{
  IColor post = mCompact ? GetColor(kX1) : GetColor(kFG);
  IColor pre = GetColor(kX3);
  if (IsDisabled())
  {
    post = GetColor(kX2);
    pre = GetColor(kX2);
  }

  // Grid labels, fading towards the bottom.
  const IText& valueText = GetStyle().valueText;
  const float range = std::fabs(mMaxDB - mMinDB);
  for (int v : mGrid)
  {
    const float r = (v + std::fabs(mMinDB)) / range;
    const float y = mPlot.B - (mPlot.B - mPlot.T) * r;
    const int alpha = static_cast<int>(std::clamp(r * 0.66f, 0.f, 1.f) * 255.f);
    char label[32];
    std::snprintf(label, sizeof(label), "%i dB", v);
    g.DrawText(valueText.WithSize(10.f).WithFGColor(WithAlpha(valueText.mFGColor, alpha)), label, IRECT(mPlot.L, y - 5.f, mPlot.R, y + 5.f));
  }

  auto vertical = [this](const IColor& c) {
    return IPattern::CreateLinearGradient(mPlot, EDirection::Vertical, {{WithAlpha(c, 76), 0.f}, {WithAlpha(c, 0), 1.f}});
  };

  g.PathClipRegion(mPlot);

  // Input trace: a fill only. Its outline is transparent in the original.
  PathTrace(g, mPre, mPreWrite);
  PathFillLinear(g, vertical(pre));

  // Output trace: a fill and an outline that fades out at both ends.
  PathTrace(g, mPost, mPostWrite);
  PathFillLinear(g, vertical(post), true);
  PathStrokeLinear(g, IPattern::CreateLinearGradient(mPlot, EDirection::Horizontal, {{WithAlpha(post, 0), 0.f}, {WithAlpha(post, 204), 0.5f}, {WithAlpha(post, 0), 1.f}}), 1.f);

  g.PathClipRegion();
}

#pragma mark - ThresholdSlider

ThresholdSlider::ThresholdSlider(const IRECT& bounds, int paramIdx, const IVStyle& style)
: ISliderControlBase(bounds, paramIdx, EDirection::Vertical, 4., 8.f)
, IVectorBase(style)
{
  AttachIControl(this, "");
  mHandleSize = 20.f;
  mHideCursorOnDrag = false;
}

void ThresholdSlider::OnResize()
{
  ISliderControlBase::OnResize();
  // The line scale matches the display labels: 0 dB at the top, -48 dB at the bottom.
  mWidgetBounds = IRECT(mRECT.L, mRECT.T + mPillHeight / 2.f, mRECT.R, mRECT.B - mPillHeight / 2.f);
  mTrackBounds = mWidgetBounds;
}

float ThresholdSlider::LineY() const
{
  return mWidgetBounds.B - mWidgetBounds.H() * static_cast<float>(GetValue());
}

IRECT ThresholdSlider::GetPillRect() const
{
  const float cx = (mWidgetBounds.L + mWidgetBounds.R) / 2.f;
  const float bottom = std::fmin(mWidgetBounds.B, LineY() + mPillHeight / 2.f);
  return IRECT(cx - 25.f, bottom - mPillHeight, cx + 25.f, bottom);
}

bool ThresholdSlider::IsHit(float x, float y) const
{
  return GetPillRect().Contains(x, y);
}

void ThresholdSlider::OnMouseOver(float x, float y, const IMouseMod& mod)
{
  ISliderControlBase::OnMouseOver(x, y, mod);
  ShowFooterTooltip(this);
}

void ThresholdSlider::Draw(IGraphics& g)
{
  const IColor accent = Accent(*this, *this);
  const IRECT tag = GetPillRect().GetPadded(-2.f);
  const float y = LineY();

  IStrokeOptions butt;
  butt.mMiterLimit = 10.f;

  // The line, on both sides of the tag, fading out towards the panel edges.
  g.PathMoveTo(mWidgetBounds.L, y);
  g.PathLineTo(tag.L, y);
  g.PathMoveTo(mWidgetBounds.R, y);
  g.PathLineTo(tag.R, y);
  PathStrokeLinear(g, IPattern::CreateLinearGradient(mRECT, EDirection::Horizontal, {{WithAlpha(accent, 0), 0.f}, {WithAlpha(accent, 229), 0.5f}, {WithAlpha(accent, 0), 1.f}}), 1.33f, butt);

  // The tag: a dark fill of the accent and an accent outline.
  g.PathRoundRect(tag, std::min(tag.W(), tag.H()) * 0.25f);
  g.PathFill(IPattern(IColor(168, accent.R / 4, accent.G / 4, accent.B / 4)), Preserve());
  g.PathStroke(IPattern(accent), 1.33f, butt);

  const WDL_String value = DisplayWithLabel(*this);
  if (value.GetLength())
    g.DrawText(GetStyle().valueText.WithFGColor(accent), value.Get(), tag);
}

#pragma mark - SVGButton

SVGButton::SVGButton(const IRECT& bounds, int paramIdx, DrawIconFunc drawIcon, IActionFunction actionFunc)
: IControl(bounds, paramIdx, actionFunc)
, mDrawIcon(std::move(drawIcon))
{
  // ISwitchControlBase treats a double click as a second click.
  mDblAsSingleClick = true;
}

void SVGButton::Draw(IGraphics& g)
{
  mDrawIcon(g, mRECT);

  // A param-bound button that is on gets a half-dark overlay.
  if (GetParamIdx() > kNoParameter && GetValue() > 0.5)
    g.FillRect(IColor(127, 7, 7, 7), mRECT);
}

void SVGButton::OnMouseDown(float x, float y, const IMouseMod& mod)
{
  SetValue(GetValue() == 0. ? 1. : 0.);
  SetDirty(true);
}

void SVGButton::OnMouseUp(float x, float y, const IMouseMod& mod)
{
  SetDirty(false);
}
} // namespace clampui
