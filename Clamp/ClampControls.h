#pragma once

// The interactive controls of the Clamp editor. Each class has the name of
// the original class and follows its geometry, drawing and mouse handling.

#include "ClampUIBase.h"
#include "IBubbleControl.h"

#include <array>
#include <functional>

namespace clampui
{
/** CEILING knob (BigKnob, base KnobBase). The arc runs from the value to the 0 dB end. */
class BigKnob final : public IKnobControlBase, public IVectorBase
{
public:
  /** The knob is size wide around (cx, cy). The label strip adds 20 px below it. */
  BigKnob(float cx, float cy, float size, int paramIdx, const IVStyle& style);

  void Draw(IGraphics& g) override;
  void OnResize() override;
  bool IsHit(float x, float y) const override;
  void OnMouseOver(float x, float y, const IMouseMod& mod) override;

protected:
  IRECT GetKnobDragBounds() override;

private:
  IRECT mKnobRect;
  IRECT mLabelRect;
};

/** IN and OUT fader (Slider). The value follows the pointer, and the fill grows from the centre. */
class Slider final : public ISliderControlBase, public IVectorBase
{
public:
  Slider(const IRECT& bounds, int paramIdx, const IVStyle& style);

  void Draw(IGraphics& g) override;
  void OnResize() override;
  bool IsHit(float x, float y) const override;
  void OnMouseOver(float x, float y, const IMouseMod& mod) override;
};

/** SHAPE label, value and up/down marks (SimpleValueBox). Vertical drag changes the value. */
class SimpleValueBox final : public IKnobControlBase, public IVectorBase
{
public:
  SimpleValueBox(const IRECT& bounds, int paramIdx, const IVStyle& style);

  void Draw(IGraphics& g) override;
  void OnMouseOver(float x, float y, const IMouseMod& mod) override;

protected:
  IRECT GetKnobDragBounds() override;
};

/** Soft (type 0) or hard (type 1) curve icon next to SHAPE (ShapeDisplay). It fades with the SHAPE value. */
class ShapeDisplay final : public IControl, public IVectorBase
{
public:
  ShapeDisplay(const IRECT& bounds, int paramIdx, const IVStyle& style, int type, bool invert);

  void Draw(IGraphics& g) override;

private:
  int mType;
  bool mInvert;
};

/** MODE and TYPE selector (SimpleSpinBox): a carousel of the enum texts with a 400 ms slide. */
class SimpleSpinBox final : public IControl, public IVectorBase
{
public:
  SimpleSpinBox(const IRECT& bounds, int paramIdx, const IVStyle& style);

  void Draw(IGraphics& g) override;
  void OnInit() override;
  void OnMouseDown(float x, float y, const IMouseMod& mod) override;
  void OnMouseUp(float x, float y, const IMouseMod& mod) override;
  void OnMouseOver(float x, float y, const IMouseMod& mod) override;
  void OnMouseOut() override;
  void SetDirty(bool triggerAction = true, int valIdx = kNoValIdx) override;

private:
  void DrawItem(IGraphics& g, int index, const IRECT& r, float alpha, const IColor& color);
  void OnAnimationTick();

  int mNumItems = 2;
  /** Animated carousel position, in normalized units. Negative until the first draw. */
  float mPosition = -1.f;
  bool mAnimating = false;
  /** Hovered half: -1 left, 1 right, 0 none. */
  int mHoverDirection = 0;
};

/** In-place list that opens on hover (DropMenu). Subclasses set the items. */
class DropMenu : public IControl, public IVectorBase
{
public:
  DropMenu(const IRECT& bounds, int paramIdx, const IVStyle& style);

  void Draw(IGraphics& g) override;
  void OnResize() override;
  void OnMouseOver(float x, float y, const IMouseMod& mod) override;
  void OnMouseOut() override;
  void OnMouseDown(float x, float y, const IMouseMod& mod) override;
  void OnMouseUp(float x, float y, const IMouseMod& mod) override;

  int GetSelected() const { return mSelected; }

protected:
  /** Extends the control downwards to numItems rows as tall as the control is wide. */
  void InitRows(int numItems);
  IRECT GetRow(int index) const;
  int RowAt(float x, float y) const;

  virtual void DrawItem(IGraphics& g, int index, const IRECT& r) = 0;
  virtual void OnSelect() = 0;

  int mNumItems = 1;
  int mSelected = 0;
  int mHovered = -1;
  bool mExpanded = false;

private:
  bool mPressed = false;
};

/** OVERSAMPLING list in the header (ParamMenu). */
class ParamMenu final : public DropMenu
{
public:
  ParamMenu(const IRECT& bounds, int paramIdx, const IVStyle& style);

  void OnInit() override;
  void SetDirty(bool triggerAction = true, int valIdx = kNoValIdx) override;

protected:
  void DrawItem(IGraphics& g, int index, const IRECT& r) override;
  void OnSelect() override;
};

/** Accent colour list in the header (ColorMenu). A choice writes the colour into kX1 of every control. */
class ColorMenu final : public DropMenu
{
public:
  ColorMenu(const IRECT& bounds, const IVStyle& style);

  void OnInit() override;

  /** Selects a palette entry and applies it, as OnUIOpen does with the saved colour. */
  void SetSelected(int index);

protected:
  void DrawItem(IGraphics& g, int index, const IRECT& r) override;
  void OnSelect() override;
};

/** Value bubble over the CEILING knob and the faders (ValueDisplay). Bubble control 0. */
class ValueDisplay final : public IBubbleControl
{
public:
  explicit ValueDisplay(const IText& text);

  /** Shows the bubble for pCaller, with its tail at (x, y). */
  void Show(IControl* pCaller, float x, float y);

  void Draw(IGraphics& g) override;

private:
  float mX = 0.f;
  float mY = 0.f;
};

/** Footer strip that shows "NAME : tooltip" for the hovered control (TooltipDisplay). Bubble control 1. */
class TooltipDisplay final : public IBubbleControl
{
public:
  TooltipDisplay(const IText& text, const IRECT& footerRect);

  void ShowTooltip(IControl* pCaller);

  void Draw(IGraphics& g) override;
  void OnResize() override;
  void SetDisabled(bool disable) override;

private:
  IRECT mFooterRect;
};

/** Scrolling level history (DynamicsDisplay). It shows the input after IN gain and the output, in dB. */
class DynamicsDisplay final : public IControl, public IVectorBase
{
public:
  DynamicsDisplay(const IRECT& bounds, const IVStyle& style, float minDB, float maxDB);

  void Draw(IGraphics& g) override;
  void OnResize() override;
  void OnMsgFromDelegate(int msgTag, int dataSize, const void* pData) override;

private:
  static constexpr int kNumPoints = 1024;

  void PathTrace(IGraphics& g, const std::array<float, kNumPoints>& ring, int writeIndex) const;

  IRECT mPlot;
  float mMinDB;
  float mMaxDB;
  bool mCompact = true;
  std::vector<int> mGrid;
  std::array<float, kNumPoints> mPost {};
  std::array<float, kNumPoints> mPre {};
  int mPostWrite = 0;
  int mPreWrite = 0;
};

/** CEILING line and value tag over the level display (ThresholdSlider). */
class ThresholdSlider final : public ISliderControlBase, public IVectorBase
{
public:
  ThresholdSlider(const IRECT& bounds, int paramIdx, const IVStyle& style);

  void Draw(IGraphics& g) override;
  void OnResize() override;
  bool IsHit(float x, float y) const override;
  void OnMouseOver(float x, float y, const IMouseMod& mod) override;

private:
  float LineY() const;
  IRECT GetPillRect() const;

  IRECT mWidgetBounds;
  float mPillHeight = 20.f;
};

/** Header icon button (SVGButton). The original draws SVG files; the port draws its own path icons.
 * A param-bound button is dimmed while its value is above 0.5. It never greys out. */
class SVGButton final : public IControl
{
public:
  using DrawIconFunc = std::function<void(IGraphics&, const IRECT&)>;

  SVGButton(const IRECT& bounds, int paramIdx, DrawIconFunc drawIcon, IActionFunction actionFunc);

  void Draw(IGraphics& g) override;
  void OnMouseDown(float x, float y, const IMouseMod& mod) override;
  void OnMouseUp(float x, float y, const IMouseMod& mod) override;
  void SetDisabled(bool disable) override {}

private:
  DrawIconFunc mDrawIcon;
};
} // namespace clampui
