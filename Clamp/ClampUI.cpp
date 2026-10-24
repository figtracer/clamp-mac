#include "ClampUI.h"

#include "Clamp.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

void ClampDisplaySender::Reset(double sampleRate)
{
  mWindowSize = std::max(1, static_cast<int>(sampleRate * kDisplayPointSeconds));
  mNumInputPeaks = 0;
  mCount = 0;
  mInputPeak = 0.f;
  mOutputPeak = 0.f;
}

void ClampDisplaySender::ProcessInput(sample** inputs, int nFrames, double gain)
{
  int count = mCount;
  float peak = mInputPeak;
  mNumInputPeaks = 0;

  for (int s = 0; s < nFrames; s++)
  {
    const double v = std::max(std::abs(inputs[0][s]), std::abs(inputs[1][s])) * gain;
    peak = std::max(peak, static_cast<float>(v));

    if (++count == mWindowSize)
    {
      if (mNumInputPeaks < kMaxPointsPerBlock)
        mInputPeaks[mNumInputPeaks++] = peak;
      peak = 0.f;
      count = 0;
    }
  }

  mInputPeak = peak;
}

void ClampDisplaySender::ProcessOutput(sample** outputs, int nFrames, double gain, int ctrlTag)
{
  int point = 0;

  for (int s = 0; s < nFrames; s++)
  {
    const double v = std::max(std::abs(outputs[0][s]), std::abs(outputs[1][s])) * gain;
    mOutputPeak = std::max(mOutputPeak, static_cast<float>(v));

    if (++mCount == mWindowSize)
    {
      if (point < mNumInputPeaks)
        PushData({ctrlTag, {mInputPeaks[point], mOutputPeak}});
      point++;
      mOutputPeak = 0.f;
      mCount = 0;
    }
  }
}

#if IPLUG_EDITOR
namespace
{
const char* kFontRegular = "Inter-Regular";
const char* kFontSemiBold = "Inter-SemiBold";
const char* kFontBold = "Inter-Bold";

const IColor kBackground(255, 7, 7, 7);
const IColor kAccent(255, 205, 108, 104);
const IColor kAccentBright(255, 236, 150, 145);
const IColor kTextBright(255, 182, 182, 182);
const IColor kTextLabel(255, 176, 176, 176);
const IColor kTextMid(255, 128, 128, 128);
const IColor kTextDim(255, 76, 76, 76);
const IColor kLogo(255, 205, 205, 205);

/** Card corner radius. */
constexpr float kCardRadius = 5.f;
/** Distance from the top of the level display to its 0 dB line. */
constexpr float kDisplayZeroOffset = 23.6f;
/** Vertical scale of the level display: one grid step of 6 dB is 25 px. */
constexpr float kDisplayPixelsPerDB = 25.f / 6.f;
/** Width and height of the ceiling tag in the level display. */
constexpr float kTagWidth = 46.f;
constexpr float kTagHeight = 16.f;

IStrokeOptions RoundStroke()
{
  IStrokeOptions options;
  options.mCapOption = ELineCap::Round;
  options.mJoinOption = ELineJoin::Round;
  return options;
}

IColor Grey(int v, int alpha = 255)
{
  return IColor(alpha, v, v, v);
}

/** Draws a "<" or ">" chevron centred on (x, y). */
void DrawChevron(IGraphics& g, float x, float y, bool pointsLeft, const IColor& color)
{
  const float dx = pointsLeft ? 2.3f : -2.3f;
  g.PathMoveTo(x + dx, y - 4.3f);
  g.PathLineTo(x - dx, y);
  g.PathLineTo(x + dx, y + 4.3f);
  g.PathStroke(color, 1.3f, RoundStroke());
}

/** Draws the triangle of dots in one corner of the controls card. (x, y) is the corner dot. */
void DrawCornerDots(IGraphics& g, float x, float y, float dirX, float dirY)
{
  constexpr float spacing = 8.4f;
  for (int j = 0; j < 3; j++)
    for (int i = 0; i + j < 3; i++)
      g.FillCircle(Grey(15), x + dirX * spacing * i, y + dirY * spacing * j, 1.1f);
}

/** Static artwork: background, header title and mark, cards, and fixed labels. */
class ClampBackground final : public IControl
{
public:
  ClampBackground(const IRECT& bounds, const IRECT& displayCard, const IRECT& controlsCard, float stripTop)
  : IControl(bounds)
  , mDisplayCard(displayCard)
  , mControlsCard(controlsCard)
  , mStripTop(stripTop)
  {
    mIgnoreMouse = true;
  }

  void Draw(IGraphics& g) override
  {
    g.FillRect(kBackground, mRECT);
    DrawMark(g, IRECT(10.f, 12.f, 29.f, 29.f));
    g.DrawText(IText(17.5f, kTextBright, kFontBold, EAlign::Near), "CLAMP", IRECT(41.f, 10.f, 160.f, 31.f));

    // Level display card: flat, darker towards the bottom.
    const IRECT& d = mDisplayCard;
    g.FillRoundRect(Grey(26), d, kCardRadius);
    g.PathRoundRect(IRECT(d.L, d.B - 80.f, d.R, d.B), 0.f, 0.f, kCardRadius, kCardRadius);
    g.PathFill(IPattern::CreateLinearGradient(d.MW(), d.B - 80.f, d.MW(), d.B - 15.f, {{Grey(17, 0), 0.f}, {Grey(17), 1.f}}));
    DrawTopEdge(g, d);

    // Controls card: lit from the top left, with a dark strip at the bottom.
    const IRECT& c = mControlsCard;
    g.PathRoundRect(IRECT(c.L, c.T, c.R, mStripTop), kCardRadius, kCardRadius, 0.f, 0.f);
    g.PathFill(IPattern::CreateLinearGradient(c.L + 65.f, c.T, c.L + 165.f, mStripTop, {{Grey(57), 0.f}, {Grey(37), 1.f}}));
    g.FillRoundRect(Grey(16), IRECT(c.L, mStripTop, c.R, c.B), 0.f, 0.f, kCardRadius, kCardRadius);
    DrawTopEdge(g, c);

    DrawCornerDots(g, c.L + 14.6f, c.T + 13.2f, 1.f, 1.f);
    DrawCornerDots(g, c.R - 14.7f, c.T + 13.2f, -1.f, 1.f);
    DrawCornerDots(g, c.L + 14.6f, c.T + 207.8f, 1.f, -1.f);
    DrawCornerDots(g, c.R - 14.7f, c.T + 207.8f, -1.f, -1.f);

    g.DrawText(IText(10.5f, kTextMid, kFontSemiBold), "CEILING", IRECT(c.L, 222.f, c.R, 239.f));
    g.DrawText(IText(10.5f, kTextMid, kFontRegular, EAlign::Far), PLUG_NAME " " PLUG_VERSION_STR, IRECT(200.f, 301.f, 394.f, 318.f));
  }

private:
  /** Draws the header mark: a sine wave held between two clamp jaws. */
  static void DrawMark(IGraphics& g, const IRECT& r)
  {
    constexpr int kSteps = 32;
    const float limit = r.H() * 0.27f;
    for (int i = 0; i <= kSteps; i++)
    {
      const float t = static_cast<float>(i) / kSteps;
      const float y = std::clamp(-static_cast<float>(std::sin(t * 2. * PI)) * r.H() * 0.5f, -limit, limit);
      if (i == 0)
        g.PathMoveTo(r.L + 1.f, r.MH() + y);
      else
        g.PathLineTo(r.L + 1.f + t * (r.W() - 2.f), r.MH() + y);
    }
    g.PathStroke(kLogo, 2.2f, RoundStroke());
    g.PathLine(r.L, r.T + 0.8f, r.R, r.T + 0.8f);
    g.PathLine(r.L, r.B - 0.8f, r.R, r.B - 0.8f);
    g.PathStroke(kLogo, 1.6f, RoundStroke());
  }

  /** Draws a faint highlight along the top edge of a card. */
  static void DrawTopEdge(IGraphics& g, const IRECT& r)
  {
    g.PathRoundRect(r.GetPadded(-0.5f), kCardRadius);
    g.PathStroke(IPattern::CreateLinearGradient(r.MW(), r.T, r.MW(), r.T + 8.f, {{IColor(28, 255, 255, 255), 0.f}, {IColor(0, 255, 255, 255), 1.f}}), 1.f);
  }

  IRECT mDisplayCard;
  IRECT mControlsCard;
  float mStripTop;
};

/** Scrolling level history with a dB grid and a draggable ceiling tag. */
class ClampLevelDisplay final : public IControl
{
public:
  ClampLevelDisplay(const IRECT& bounds, int paramIdx)
  : IControl(bounds, paramIdx)
  {
  }

  void Draw(IGraphics& g) override
  {
    const float zeroY = DBToY(0.);
    DrawHistory(g, mInput.data(), zeroY, Grey(255, 18), Grey(255, 4), Grey(255, 22));
    DrawHistory(g, mOutput.data(), zeroY, Grey(255, 22), Grey(255, 0), Grey(120, 210));

    for (int dB = 0; dB >= -42; dB -= 6)
    {
      char label[16];
      std::snprintf(label, sizeof(label), "%d dB", dB);
      const IColor color = Grey(static_cast<int>(127.f + 2.2f * dB));
      const float y = DBToY(dB);
      g.DrawText(IText(9.f, color, kFontSemiBold), label, IRECT(mRECT.L, y - 6.f, mRECT.R, y + 6.f));
    }

    const double ceilingDB = GetParam()->FromNormalized(GetValue());
    const float y = DBToY(ceilingDB);
    g.DrawHorizontalLine(kAccent.WithOpacity(0.3f), y, mRECT.L, mRECT.R);

    const IRECT tag = GetTagRect();
    g.FillRoundRect(IColor(255, 46, 31, 30), tag, 4.f);
    g.DrawRoundRect(kAccent, tag.GetPadded(-0.75f), 3.5f, nullptr, 1.5f);
    char value[16];
    std::snprintf(value, sizeof(value), "%.1f dB", ceilingDB);
    g.DrawText(IText(10.f, kAccent, kFontRegular), value, tag);
  }

  void OnMsgFromDelegate(int msgTag, int dataSize, const void* pData) override
  {
    if (mFrozen || msgTag != ISender<2, 256, float>::kUpdateMessage || dataSize != sizeof(ISenderData<2, float>))
      return;

    const auto* pPoint = static_cast<const ISenderData<2, float>*>(pData);
    mInput[mWritePos] = pPoint->vals[0];
    mOutput[mWritePos] = pPoint->vals[1];
    mWritePos = (mWritePos + 1) % kNumPoints;
    SetDirty(false);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    mDragging = HitsCeiling(x, y);
    mDragOffset = y - DBToY(GetParam()->FromNormalized(GetValue()));
  }

  void OnMouseDrag(float x, float y, float dX, float dY, const IMouseMod& mod) override
  {
    if (mDragging)
      SetCeilingDB(YToDB(y - mDragOffset));
  }

  void OnMouseUp(float x, float y, const IMouseMod& mod) override
  {
    mDragging = false;
  }

  void OnMouseDblClick(float x, float y, const IMouseMod& mod) override
  {
    if (HitsCeiling(x, y))
      SetValueToDefault();
  }

  void OnMouseWheel(float x, float y, const IMouseMod& mod, float d) override
  {
    SetCeilingDB(GetParam()->FromNormalized(GetValue()) + (d > 0.f ? 0.5 : -0.5));
  }

  void OnMouseOver(float x, float y, const IMouseMod& mod) override
  {
    GetUI()->SetMouseCursor(HitsCeiling(x, y) ? ECursor::SIZENS : ECursor::ARROW);
    IControl::OnMouseOver(x, y, mod);
  }

  void OnMouseOut() override
  {
    GetUI()->SetMouseCursor(ECursor::ARROW);
    IControl::OnMouseOut();
  }

  /** Stops or restarts the scrolling. */
  void SetFrozen(bool frozen) { mFrozen = frozen; }
  bool IsFrozen() const { return mFrozen; }

private:
  static constexpr int kNumPoints = 256;

  float DBToY(double dB) const
  {
    return mRECT.T + kDisplayZeroOffset - static_cast<float>(dB) * kDisplayPixelsPerDB;
  }

  double YToDB(float y) const
  {
    return (mRECT.T + kDisplayZeroOffset - y) / kDisplayPixelsPerDB;
  }

  IRECT GetTagRect() const
  {
    const float y = DBToY(GetParam()->FromNormalized(GetValue()));
    return IRECT(mRECT.MW() - kTagWidth / 2.f, y - kTagHeight / 2.f, mRECT.MW() + kTagWidth / 2.f, y + kTagHeight / 2.f);
  }

  bool HitsCeiling(float x, float y) const
  {
    return GetTagRect().Contains(x, y) || std::abs(y - GetTagRect().MH()) <= 4.f;
  }

  void SetCeilingDB(double dB)
  {
    SetValue(GetParam()->ConstrainNormalized(GetParam()->ToNormalized(dB)));
    SetDirty(true);
  }

  /** Fills the area under one level history, newest point at the right edge, and strokes its outline. */
  void DrawHistory(IGraphics& g, const float* pPoints, float zeroY, const IColor& fillTop, const IColor& fillBottom, const IColor& lineColor)
  {
    const int nPoints = std::min(kNumPoints, static_cast<int>(mRECT.W()) + 1);
    auto pointY = [&](int k) {
      const float amp = pPoints[(mWritePos - 1 - k + kNumPoints) % kNumPoints];
      const float y = DBToY(AmpToDB(std::max(amp, 1e-6f)));
      return std::clamp(y, mRECT.T, mRECT.B);
    };

    g.PathMoveTo(mRECT.R, mRECT.B);
    for (int k = 0; k < nPoints; k++)
      g.PathLineTo(mRECT.R - k, pointY(k));
    g.PathLineTo(mRECT.R - (nPoints - 1), mRECT.B);
    g.PathClose();
    g.PathFill(IPattern::CreateLinearGradient(mRECT.MW(), zeroY, mRECT.MW(), mRECT.B, {{fillTop, 0.f}, {fillBottom, 1.f}}));

    for (int k = 0; k < nPoints; k++)
    {
      if (k == 0)
        g.PathMoveTo(mRECT.R, pointY(k));
      else
        g.PathLineTo(mRECT.R - k, pointY(k));
    }
    g.PathStroke(lineColor, 1.f, RoundStroke());
  }

  std::array<float, kNumPoints> mInput {};
  std::array<float, kNumPoints> mOutput {};
  int mWritePos = 0;
  bool mFrozen = false;
  bool mDragging = false;
  float mDragOffset = 0.f;
};

/** Large CEILING knob with a red arc from the value to the top of the range. */
class ClampCeilingKnob final : public IKnobControlBase
{
public:
  ClampCeilingKnob(const IRECT& bounds, int paramIdx)
  : IKnobControlBase(bounds, paramIdx, EDirection::Vertical, 2.)
  {
  }

  void Draw(IGraphics& g) override
  {
    const float cx = mRECT.MW();
    const float cy = mRECT.MH();
    const float s = mRECT.W() / 132.f;
    const float angle = -135.f + static_cast<float>(GetValue()) * 270.f;

    // Soft shadow around the well.
    g.PathCircle(cx, cy + 3.f * s, 70.f * s);
    g.PathFill(IPattern::CreateRadialGradient(cx, cy + 3.f * s, 70.f * s, {{IColor(110, 0, 0, 0), 60.f / 70.f}, {IColor(0, 0, 0, 0), 1.f}}));

    // Well with a lit top rim.
    g.PathCircle(cx, cy, 63.5f * s);
    g.PathFill(IPattern::CreateLinearGradient(cx, cy - 63.5f * s, cx, cy + 63.5f * s, {{Grey(68), 0.f}, {Grey(26), 1.f}}));
    g.PathCircle(cx, cy, 62.3f * s);
    g.PathFill(IPattern::CreateLinearGradient(cx, cy - 62.f * s, cx, cy + 62.f * s, {{Grey(40), 0.f}, {Grey(33), 1.f}}));

    // Knob body, dark at the edge and at the bottom.
    g.PathCircle(cx, cy, 56.5f * s);
    g.PathFill(IPattern::CreateRadialGradient(cx, cy, 56.5f * s, {{Grey(47), 44.f / 56.5f}, {Grey(13), 1.f}}));
    g.PathCircle(cx, cy, 56.5f * s);
    g.PathFill(IPattern::CreateLinearGradient(cx, cy - 20.f * s, cx, cy + 56.f * s, {{IColor(0, 0, 0, 0), 0.f}, {IColor(150, 0, 0, 0), 1.f}}));

    // Face with a thin lit rim.
    g.PathCircle(cx, cy, 44.3f * s);
    g.PathFill(IPattern::CreateLinearGradient(cx, cy - 44.f * s, cx, cy + 44.f * s, {{Grey(58), 0.f}, {Grey(38), 1.f}}));
    g.PathCircle(cx, cy, 44.3f * s);
    g.PathStroke(IPattern::CreateLinearGradient(cx, cy - 44.f * s, cx, cy + 20.f * s, {{Grey(96), 0.f}, {Grey(96, 0), 1.f}}), 1.f);

    // Value arc with a glow.
    if (angle < 134.5f)
    {
      const float r = 56.5f * s;
      g.DrawArc(kAccent.WithOpacity(0.06f), cx, cy, r, angle, 135.f, nullptr, 10.f * s);
      g.DrawArc(kAccent.WithOpacity(0.12f), cx, cy, r, angle, 135.f, nullptr, 6.f * s);
      g.DrawArc(kAccent.WithOpacity(0.35f), cx, cy, r, angle, 135.f, nullptr, 3.5f * s);
      g.PathArc(cx, cy, r, angle, 135.f);
      g.PathStroke(kAccentBright, 2.f * s, RoundStroke());
    }

    g.PathRadialLine(cx, cy, angle, 31.f * s, 47.f * s);
    g.PathStroke(Grey(112), 1.7f * s, RoundStroke());
  }

  void OnMouseDblClick(float x, float y, const IMouseMod& mod) override
  {
    SetValueToDefault();
  }
};

/** Vertical gain fader. The red bar runs from 0 dB to the handle. */
class ClampFader final : public IKnobControlBase
{
public:
  ClampFader(const IRECT& bounds, int paramIdx)
  : IKnobControlBase(bounds, paramIdx, EDirection::Vertical, 1.)
  {
  }

  void Draw(IGraphics& g) override
  {
    const float cx = mRECT.MW();
    const IRECT track(cx - 3.5f, mRECT.T, cx + 3.5f, mRECT.B);
    g.FillRoundRect(Grey(11), track, 3.5f);
    g.PathRoundRect(track.GetPadded(0.5f), 4.f);
    g.PathStroke(IPattern::CreateLinearGradient(cx, mRECT.MH(), cx, mRECT.B, {{Grey(70, 0), 0.f}, {Grey(70), 1.f}}), 1.f);

    const IRECT travel = GetKnobDragBounds();
    const float handleY = travel.B - static_cast<float>(GetValue()) * travel.H();
    const float zeroY = travel.B - static_cast<float>(GetParam()->ToNormalized(0.)) * travel.H();
    if (handleY < zeroY - kHandleHalfHeight)
      g.DrawLine(kAccentBright, cx, handleY + kHandleHalfHeight, cx, zeroY, nullptr, 1.5f);
    else if (handleY > zeroY + kHandleHalfHeight)
      g.DrawLine(kAccentBright, cx, zeroY, cx, handleY - kHandleHalfHeight, nullptr, 1.5f);

    const IRECT handle(mRECT.L, handleY - kHandleHalfHeight, mRECT.R, handleY + kHandleHalfHeight);
    g.DrawFastDropShadow(handle, handle.GetPadded(7.f), 2.f, 3.5f, 5.f);
    g.PathRoundRect(handle, 3.5f);
    g.PathFill(IPattern::CreateLinearGradient(cx, handle.T, cx, handle.B, {{Grey(64), 0.f}, {Grey(35), 1.f}}));
    g.PathLine(cx - 8.5f, handleY, cx + 8.5f, handleY);
    g.PathStroke(Grey(124), 1.6f, RoundStroke());
  }

  void OnMouseDblClick(float x, float y, const IMouseMod& mod) override
  {
    SetValueToDefault();
  }

protected:
  /** The handle centre travels the track minus half a handle at each end, and follows the mouse 1:1. */
  IRECT GetKnobDragBounds() override
  {
    return mRECT.GetVPadded(-kHandleHalfHeight);
  }

private:
  static constexpr float kHandleHalfHeight = 6.f;
};

/** SHAPE strip: soft and hard curve icons, the label, and the value, which changes with vertical drag. */
class ClampShapeControl final : public IKnobControlBase
{
public:
  ClampShapeControl(const IRECT& bounds, int paramIdx)
  : IKnobControlBase(bounds, paramIdx, EDirection::Vertical, 1.)
  {
  }

  void Draw(IGraphics& g) override
  {
    const float v = static_cast<float>(GetValue());
    const float x = mRECT.L;
    const float y = mRECT.MH();

    g.PathMoveTo(x + 4.5f, y + 3.f);
    g.PathQuadraticBezierTo(x + 6.5f, y - 3.7f, x + 17.4f, y - 4.f);
    g.PathStroke(IColor::LinearInterpolateBetween(kAccent, kTextDim, v), 1.6f, RoundStroke());

    g.PathMoveTo(x + 120.7f, y + 3.3f);
    g.PathLineTo(x + 127.4f, y - 3.7f);
    g.PathLineTo(x + 133.2f, y - 3.7f);
    g.PathStroke(IColor::LinearInterpolateBetween(kTextDim, kAccent, v), 1.4f, RoundStroke());

    g.DrawText(IText(8.5f, kTextLabel, kFontBold, EAlign::Near), "SHAPE", IRECT(x + 33.7f, y - 7.f, x + 66.f, y + 7.f));
    char value[16];
    std::snprintf(value, sizeof(value), "%.2f", GetParam()->FromNormalized(GetValue()));
    g.DrawText(IText(10.5f, kAccent, kFontRegular, EAlign::Near), value, IRECT(x + 66.3f, y - 7.f, x + 95.f, y + 7.f));

    const float cx = x + 101.4f;
    g.PathMoveTo(cx - 3.3f, y - 1.5f);
    g.PathLineTo(cx, y - 4.4f);
    g.PathLineTo(cx + 3.3f, y - 1.5f);
    g.PathMoveTo(cx - 3.3f, y + 1.9f);
    g.PathLineTo(cx, y + 4.8f);
    g.PathLineTo(cx + 3.3f, y + 1.9f);
    g.PathStroke(kTextDim, 1.2f, RoundStroke());
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    if (x < mRECT.L + 24.f)
      SetShape(0.);
    else if (x > mRECT.L + 114.f)
      SetShape(1.);
    else
      IKnobControlBase::OnMouseDown(x, y, mod);
  }

  void OnMouseDblClick(float x, float y, const IMouseMod& mod) override
  {
    if (x >= mRECT.L + 24.f && x <= mRECT.L + 114.f)
      SetValueToDefault();
  }

protected:
  /** A 150 px drag covers the full range. */
  IRECT GetKnobDragBounds() override
  {
    return mRECT.GetCentredInside(mRECT.W(), 150.f);
  }

private:
  void SetShape(double value)
  {
    SetValue(value);
    SetDirty(true);
  }
};

/** Enum selector drawn as text. With arrows, it shows "<" and ">" (or a dot at either end of the list). */
class ClampEnumControl final : public IControl
{
public:
  ClampEnumControl(const IRECT& bounds, int paramIdx, const IText& text, bool showArrows)
  : IControl(bounds, paramIdx)
  , mShowArrows(showArrows)
  {
    mText = text;
    mDblAsSingleClick = true;
  }

  void Draw(IGraphics& g) override
  {
    WDL_String display;
    GetParam()->GetDisplay(display);
    g.DrawText(mText, display.Get(), mRECT);

    if (!mShowArrows)
      return;

    const int index = GetParam()->Int();
    const float y = mRECT.MH();
    if (index > 0)
      DrawChevron(g, mRECT.L + 6.f, y, true, kTextDim);
    else
      g.FillCircle(kTextDim, mRECT.L + 6.f, y, 1.f);

    if (index < NumOptions() - 1)
      DrawChevron(g, mRECT.R - 6.f, y, false, kTextDim);
    else
      g.FillCircle(kTextDim, mRECT.R - 6.f, y, 1.f);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    if (mod.R)
      PromptUserInput(mRECT);
    else if (mShowArrows && x < mRECT.L + mRECT.W() * 0.25f)
      Step(-1, false);
    else if (mShowArrows && x > mRECT.R - mRECT.W() * 0.25f)
      Step(1, false);
    else
      Step(1, true);
  }

  void OnMouseWheel(float x, float y, const IMouseMod& mod, float d) override
  {
    Step(d > 0.f ? 1 : -1, false);
  }

private:
  int NumOptions() const { return GetParam()->NDisplayTexts(); }

  void Step(int delta, bool wrap)
  {
    const int n = NumOptions();
    int index = GetParam()->Int() + delta;
    index = wrap ? (index + n) % n : std::clamp(index, 0, n - 1);
    SetValue(GetParam()->ToNormalized(index));
    SetDirty(true);
  }

  bool mShowArrows;
};

/** Power icon. It is lit while the plugin is active, and dim while BYPASS is on. */
class ClampBypassButton final : public IControl
{
public:
  ClampBypassButton(const IRECT& bounds, int paramIdx)
  : IControl(bounds, paramIdx)
  {
    mDblAsSingleClick = true;
  }

  void Draw(IGraphics& g) override
  {
    const IColor color = GetValue() > 0.5 ? kTextDim : kTextBright;
    const float cx = mRECT.MW();
    const float cy = mRECT.MH() + 0.8f;
    g.PathArc(cx, cy, 6.f, 38.f, 322.f);
    g.PathStroke(color, 2.f, RoundStroke());
    g.PathLine(cx, cy - 7.f, cx, cy - 1.f);
    g.PathStroke(color, 2.f, RoundStroke());
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    SetValue(GetValue() > 0.5 ? 0. : 1.);
    SetDirty(true);
  }
};

/** Record-style button. It pauses and resumes the level display. */
class ClampFreezeButton final : public IControl
{
public:
  ClampFreezeButton(const IRECT& bounds, ClampLevelDisplay* pDisplay)
  : IControl(bounds)
  , mDisplay(pDisplay)
  {
    mDblAsSingleClick = true;
  }

  void Draw(IGraphics& g) override
  {
    const IColor color = mDisplay->IsFrozen() ? kTextDim : kAccent;
    g.DrawCircle(color, mRECT.MW(), mRECT.MH(), 6.4f, nullptr, 1.5f);
    g.FillCircle(color, mRECT.MW(), mRECT.MH(), 4.4f);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    mDisplay->SetFrozen(!mDisplay->IsFrozen());
    SetDirty(false);
  }

private:
  ClampLevelDisplay* mDisplay;
};

/** Overlay with short help text. A click on it closes it. */
class ClampInfoOverlay final : public IControl
{
public:
  explicit ClampInfoOverlay(const IRECT& bounds)
  : IControl(bounds)
  {
  }

  void Draw(IGraphics& g) override
  {
    static const char* kLines[] = {
      "CEILING  Level where clipping starts. Drag the knob or the red tag.",
      "IN / OUT  Gain before and after the clipper.",
      "SHAPE  0 is the softest curve, 1 is a hard clip. Drag the number.",
      "TYPE  A blends tanh and hard clip. B uses a soft-knee curve.",
      "MODE  Clip L and R, one side, or mid/side components.",
      "OFF / 2X ... 16X  Oversampling. Click to step, right-click for a list.",
      "Red button  Pause or resume the level display.",
      "Power  Bypass.",
    };

    g.FillRoundRect(IColor(245, 14, 14, 14), mRECT, kCardRadius);
    g.DrawRoundRect(Grey(48), mRECT.GetPadded(-0.5f), kCardRadius);
    g.DrawText(IText(12.f, kTextBright, kFontBold, EAlign::Near), "About Clamp Port", IRECT(mRECT.L + 16.f, mRECT.T + 12.f, mRECT.R, mRECT.T + 30.f));

    const IText text(9.5f, kTextMid, kFontRegular, EAlign::Near);
    float y = mRECT.T + 40.f;
    for (const char* pLine : kLines)
    {
      g.DrawText(text, pLine, IRECT(mRECT.L + 16.f, y, mRECT.R - 16.f, y + 14.f));
      y += 20.f;
    }

    g.DrawText(IText(9.5f, kAccent, kFontRegular, EAlign::Near), "Unofficial macOS port. The original Clamp is by Outobugi.", IRECT(mRECT.L + 16.f, mRECT.B - 30.f, mRECT.R - 16.f, mRECT.B - 16.f));
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    Hide(true);
  }
};

/** "?" button. It shows the info overlay. */
class ClampHelpButton final : public IControl
{
public:
  ClampHelpButton(const IRECT& bounds, IControl* pOverlay)
  : IControl(bounds)
  , mOverlay(pOverlay)
  {
    mDblAsSingleClick = true;
    mText = IText(19.5f, kTextBright, kFontBold);
  }

  void Draw(IGraphics& g) override
  {
    g.DrawText(mText, "?", mRECT);
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    mOverlay->Hide(!mOverlay->IsHidden());
  }

private:
  IControl* mOverlay;
};

/** Footer toggle for hover tooltips. */
class ClampTooltipsToggle final : public IControl
{
public:
  explicit ClampTooltipsToggle(const IRECT& bounds)
  : IControl(bounds)
  {
    mDblAsSingleClick = true;
  }

  void Draw(IGraphics& g) override
  {
    const float cx = mRECT.L + 5.2f;
    const float cy = mRECT.MH();
    g.DrawCircle(kTextMid, cx, cy, 4.1f, nullptr, 1.2f);
    if (GetUI()->TooltipsEnabled())
      g.FillCircle(kTextMid, cx, cy, 2.f);
    g.DrawText(IText(10.5f, kTextMid, kFontRegular, EAlign::Near), "Tooltips", mRECT.GetReducedFromLeft(13.3f));
  }

  void OnMouseDown(float x, float y, const IMouseMod& mod) override
  {
    GetUI()->EnableTooltips(!GetUI()->TooltipsEnabled());
    SetDirty(false);
  }
};
} // namespace

void AttachClampUI(IGraphics* pGraphics, int displayCtrlTag)
{
  pGraphics->AttachCornerResizer(EUIResizerMode::Scale, false);
  pGraphics->LoadFont(kFontRegular, INTER_REGULAR_FN);
  pGraphics->LoadFont(kFontSemiBold, INTER_SEMIBOLD_FN);
  pGraphics->LoadFont(kFontBold, INTER_BOLD_FN);
  pGraphics->AttachBubbleControl(new IBubbleControl(IText(10.f, kTextBright, kFontSemiBold), Grey(22), Grey(60), 3.f));

  const IRECT bounds = pGraphics->GetBounds();
  const IRECT displayCard(3.f, 39.5f, 132.f, 297.f);
  const IRECT controlsCard(135.4f, 39.5f, 396.4f, 297.f);
  const float stripTop = 262.6f;
  pGraphics->AttachControl(new ClampBackground(bounds, displayCard, controlsCard, stripTop));

  auto* pDisplay = new ClampLevelDisplay(displayCard.GetReducedFromBottom(29.f), kCeiling);
  pGraphics->AttachControl(pDisplay, displayCtrlTag)->SetTooltip("Level history: input to the clipper (light) and output (dark). Drag the red tag to set CEILING.");
  pGraphics->AttachControl(new ClampEnumControl(IRECT(19.5f, 272.f, 115.7f, 291.f), kMode, IText(10.f, kAccent, kFontSemiBold), true))->SetTooltip("MODE: channels to clip");

  pGraphics->AttachControl(new ClampEnumControl(IRECT(266.5f, 11.f, 296.5f, 30.f), kOversampling, IText(11.5f, kTextBright, kFontSemiBold), false))->SetTooltip("Oversampling");
  pGraphics->AttachControl(new ClampFreezeButton(IRECT(305.f, 11.f, 323.f, 29.f), pDisplay))->SetTooltip("Pause or resume the level display");
  pGraphics->AttachControl(new ClampBypassButton(IRECT(371.f, 10.f, 389.f, 30.f), kBypass))->SetTooltip("BYPASS");

  auto* pFader = pGraphics->AttachControl(new ClampFader(IRECT(148.6f, 94.f, 178.6f, 206.6f), kInputGain));
  pFader->SetActionFunction(ShowBubbleVerticalActionFunc);
  pFader->SetTooltip("IN: gain before the clipper");
  pFader = pGraphics->AttachControl(new ClampFader(IRECT(352.8f, 94.f, 382.8f, 206.6f), kOutputGain));
  pFader->SetActionFunction(ShowBubbleVerticalActionFunc);
  pFader->SetTooltip("OUT: gain after the clipper");

  pGraphics->AttachControl(new ClampCeilingKnob(IRECT(200.1f, 84.1f, 332.1f, 216.1f), kCeiling))->SetTooltip("CEILING: level where clipping starts");
  pGraphics->AttachControl(new ClampShapeControl(IRECT(150.f, 271.f, 288.f, 292.f), kShape))->SetTooltip("SHAPE: 0 is soft, 1 is hard. Drag the number, or click a curve icon");
  pGraphics->AttachControl(new ClampEnumControl(IRECT(292.f, 272.f, 380.f, 291.f), kType, IText(10.f, kAccent, kFontSemiBold), true))->SetTooltip("TYPE: clipper algorithm");

  pGraphics->AttachControl(new ClampTooltipsToggle(IRECT(4.f, 302.f, 60.f, 317.f)));

  auto* pOverlay = new ClampInfoOverlay(IRECT(3.f, 39.5f, 396.4f, 297.f));
  pGraphics->AttachControl(new ClampHelpButton(IRECT(339.f, 9.f, 355.f, 31.f), pOverlay))->SetTooltip("About");
  pGraphics->AttachControl(pOverlay)->Hide(true);
}
#endif
