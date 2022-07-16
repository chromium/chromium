/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Gauge UI component, using browser vector graphics.
 * @see ../demos/gauge.html
 */

goog.module('goog.ui.Gauge');
goog.module.declareLegacyNamespace();

const AbstractGraphics = goog.requireType('goog.graphics.AbstractGraphics');
const Animation = goog.require('goog.fx.Animation');
const AnimationEvent = goog.requireType('goog.fx.AnimationEvent');
const Component = goog.require('goog.ui.Component');
const DomHelper = goog.requireType('goog.dom.DomHelper');
const Font = goog.require('goog.graphics.Font');
const GaugeTheme = goog.require('goog.ui.GaugeTheme');
const GroupElement = goog.requireType('goog.graphics.GroupElement');
const Path = goog.require('goog.graphics.Path');
const SolidFill = goog.require('goog.graphics.SolidFill');
const TagName = goog.require('goog.dom.TagName');
const Transition = goog.require('goog.fx.Transition');
const aria = goog.require('goog.a11y.aria');
const asserts = goog.require('goog.asserts');
const easing = goog.require('goog.fx.easing');
const googEvents = goog.require('goog.events');
const googGraphics = goog.require('goog.graphics');
const googMath = goog.require('goog.math');

/**
 * The radius of the entire gauge from the canvas size.
 * @type {number}
 */
const FACTOR_RADIUS_FROM_SIZE = 0.45;

/**
 * The ratio of internal gauge radius from entire radius.
 * The remaining area is the border around the gauge.
 * @type {number}
 */
const FACTOR_MAIN_AREA = 0.9;

/**
 * The ratio of the colored background area for value ranges.
 * The colored area width is computed as
 * InternalRadius * (1 - FACTOR_COLOR_RADIUS)
 * @type {number}
 */
const FACTOR_COLOR_RADIUS = 0.75;

/**
 * The ratio of the major ticks length start position, from the radius.
 * The major ticks length width is computed as
 * InternalRadius * (1 - FACTOR_MAJOR_TICKS)
 * @type {number}
 */
const FACTOR_MAJOR_TICKS = 0.8;

/**
 * The ratio of the minor ticks length start position, from the radius.
 * The minor ticks length width is computed as
 * InternalRadius * (1 - FACTOR_MINOR_TICKS)
 * @type {number}
 */
const FACTOR_MINOR_TICKS = 0.9;

/**
 * The length of the needle front (value facing) from the internal radius.
 * The needle front is the part of the needle that points to the value.
 * @type {number}
 */
const FACTOR_NEEDLE_FRONT = 0.95;

/**
 * The length of the needle back relative to the internal radius.
 * The needle back is the part of the needle that points away from the value.
 * @type {number}
 */
const FACTOR_NEEDLE_BACK = 0.3;

/**
 * The width of the needle front at the hinge.
 * This is the width of the curve control point, the actual width is
 * computed by the curve itself.
 * @type {number}
 */
const FACTOR_NEEDLE_WIDTH = 0.07;

/**
 * The width (radius) of the needle hinge from the gauge radius.
 * @type {number}
 */
const FACTOR_NEEDLE_HINGE = 0.15;

/**
 * The title font size (height) for titles relative to the internal radius.
 * @type {number}
 */
const FACTOR_TITLE_FONT_SIZE = 0.16;

/**
 * The offset of the title from the center, relative to the internal radius.
 * @type {number}
 */
const FACTOR_TITLE_OFFSET = 0.35;

/**
 * The formatted value font size (height) relative to the internal radius.
 * @type {number}
 */
const FACTOR_VALUE_FONT_SIZE = 0.18;

/**
 * The title font size (height) for tick labels relative to the internal radius.
 * @type {number}
 */
const FACTOR_TICK_LABEL_FONT_SIZE = 0.14;

/**
 * The offset of the formatted value down from the center, relative to the
 * internal radius.
 * @type {number}
 */
const FACTOR_VALUE_OFFSET = 0.75;

/**
 * The font name for title text.
 * @type {string}
 */
const TITLE_FONT_NAME = 'arial';

/**
 * Time in miliseconds for animating a move of the value pointer.
 * @type {number}
 */
const NEEDLE_MOVE_TIME = 400;

/**
 * Tolerance factor for how much values can exceed the range (being too
 * low or too high). The value is presented as a position (percentage).
 * @type {number}
 */
const MAX_EXCEED_POSITION_POSITION = 0.02;

/**
 * Information on how to decorate a range in the gauge.
 */
class GaugeColoredRange {
  /**
   * @param {number} fromValue The range start (minimal) value.
   * @param {number} toValue The range end (maximal) value.
   * @param {string} backgroundColor Color to fill the range background with.
   */
  constructor(fromValue, toValue, backgroundColor) {
    /**
     * The range start (minimal) value.
     * @type {number}
     */
    this.fromValue = fromValue;

    /**
     * The range end (maximal) value.
     * @type {number}
     */
    this.toValue = toValue;

    /**
     * Color to fill the range background with.
     * @type {string}
     */
    this.backgroundColor = backgroundColor;
  }
}

/**
 * A UI component that displays a gauge.
 * A gauge displayes a current value within a round axis that represents a
 * given range.
 * The gauge is built from an external border, and internal border inside it,
 * ticks and labels inside the internal border, and a needle that points to
 * the current value.
 * @final
 */
class Gauge extends Component {
  /**
   * @param {number} width The width in pixels.
   * @param {number} height The height in pixels.
   * @param {!DomHelper=} opt_domHelper The DOM helper object for the
   *     document we want to render in.
   */
  constructor(width, height, opt_domHelper) {
    super(opt_domHelper);

    /**
     * The width in pixels of this component.
     * @type {number}
     * @private
     */
    this.width_ = width;

    /**
     * The height in pixels of this component.
     * @type {number}
     * @private
     */
    this.height_ = height;

    /**
     * The underlying graphics.
     * @type {!AbstractGraphics}
     * @private
     */
    this.graphics_ =
        googGraphics.createGraphics(width, height, null, null, opt_domHelper);

    /**
     * Colors to paint the background of certain ranges (optional).
     * @type {!Array<!GaugeColoredRange>}
     * @private
     */
    this.rangeColors_ = [];

    /**
     * The minimal value that can be displayed.
     * @private
     * @type {number}
     */
    this.minValue_ = 0;

    /**
     * The maximal value that can be displayed.
     * @private
     * @type {number}
     */
    this.maxValue_ = 100;

    /**
     * The number of major tick sections.
     * @private
     * @type {number}
     */
    this.majorTicks_ = 5;

    /**
     * The number of minor tick sections in each major tick section.
     * @private
     * @type {number}
     */
    this.minorTicks_ = 2;

    /**
     * The current value that needs to be displayed in the gauge.
     * @private
     * @type {number}
     */
    this.value_ = 0;

    /**
     * The current value formatted into a String.
     * @private
     * @type {?string}
     */
    this.formattedValue_ = null;

    /**
     * The current colors theme.
     * @private
     * @type {?GaugeTheme}
     */
    this.theme_ = null;

    /**
     * Title to display above the gauge center.
     * @private
     * @type {?string}
     */
    this.titleTop_ = null;

    /**
     * Title to display below the gauge center.
     * @private
     * @type {?string}
     */
    this.titleBottom_ = null;

    /**
     * Font to use for drawing titles.
     * If null (default), computed dynamically with a size relative to the
     * gauge radius.
     * @private
     * @type {?Font}
     */
    this.titleFont_ = null;

    /**
     * Font to use for drawing the formatted value.
     * If null (default), computed dynamically with a size relative to the
     * gauge radius.
     * @private
     * @type {?Font}
     */
    this.valueFont_ = null;

    /**
     * Font to use for drawing tick labels.
     * If null (default), computed dynamically with a size relative to the
     * gauge radius.
     * @private
     * @type {?Font}
     */
    this.tickLabelFont_ = null;

    /**
     * The size in angles of the gauge axis area.
     * @private
     * @type {number}
     */
    this.angleSpan_ = 270;

    /**
     * The radius for drawing the needle.
     * Computed on full redraw, and used on every animation step of moving
     * the needle.
     * @type {number}
     * @private
     */
    this.needleRadius_ = 0;

    /**
     * The group elemnt of the needle. Contains all elements that change when
     * the gauge value changes.
     * @type {?GroupElement}
     * @private
     */
    this.needleGroup_ = null;

    /**
     * The current position (0-1) of the visible needle.
     * Initially set to null to prevent animation on first opening of the gauge.
     * @type {?number}
     * @private
     */
    this.needleValuePosition_ = null;

    /**
     * Text labels to display by major tick marks.
     * @type {?Array<string>}
     * @private
     */
    this.majorTickLabels_ = null;

    /**
     * Animation object while needle is being moved (animated).
     * @type {?Animation}
     * @private
     */
    this.animation_ = null;
  }

  /**
   * @return {number} The minimum value of the range.
   */
  getMinimum() {
    return this.minValue_;
  }

  /**
   * Sets the minimum value of the range
   * @param {number} min The minimum value of the range.
   */
  setMinimum(min) {
    this.minValue_ = min;
    const element = this.getElement();
    if (element) {
      aria.setState(element, 'valuemin', min);
    }
  }

  /**
   * @return {number} The maximum value of the range.
   */
  getMaximum() {
    return this.maxValue_;
  }

  /**
   * Sets the maximum number of the range
   * @param {number} max The maximum value of the range.
   */
  setMaximum(max) {
    this.maxValue_ = max;

    const element = this.getElement();
    if (element) {
      aria.setState(element, 'valuemax', max);
    }
  }

  /**
   * Sets the current value range displayed by the gauge.
   * @param {number} value The current value for the gauge. This value
   *     determines the position of the needle of the gauge.
   * @param {string=} opt_formattedValue The string value to show in the gauge.
   *     If not specified, no string value will be displayed.
   */
  setValue(value, opt_formattedValue) {
    this.value_ = value;
    this.formattedValue_ = opt_formattedValue || null;

    this.stopAnimation_();  // Stop the active animation if exists

    // Compute desired value position (normalize value to range 0-1)
    const valuePosition = this.valueToRangePosition_(value);
    if (this.needleValuePosition_ == null) {
      // No animation on initial display
      this.needleValuePosition_ = valuePosition;
      this.drawValue_();
    } else {
      // Animate move
      this.animation_ = new Animation(
          [this.needleValuePosition_], [valuePosition], NEEDLE_MOVE_TIME,
          easing.inAndOut);

      const events = [
        Transition.EventType.BEGIN, Animation.EventType.ANIMATE,
        Transition.EventType.END
      ];
      googEvents.listen(this.animation_, events, this.onAnimate_, false, this);
      googEvents.listen(
          this.animation_, Transition.EventType.END, this.onAnimateEnd_, false,
          this);

      // Start animation
      this.animation_.play(false);
    }

    const element = this.getElement();
    if (element) {
      aria.setState(element, 'valuenow', this.value_);
    }
  }

  /**
   * Sets the number of major tick sections and minor tick sections.
   * @param {number} majorUnits The number of major tick sections.
   * @param {number} minorUnits The number of minor tick sections for each major
   *     tick section.
   */
  setTicks(majorUnits, minorUnits) {
    this.majorTicks_ = Math.max(1, majorUnits);
    this.minorTicks_ = Math.max(1, minorUnits);
    this.draw_();
  }

  /**
   * Sets the labels of the major ticks.
   * @param {?Array<string>} tickLabels A text label for each major tick value.
   */
  setMajorTickLabels(tickLabels) {
    this.majorTickLabels_ = tickLabels;
    this.draw_();
  }

  /**
   * Sets the top title of the gauge.
   * The top title is displayed above the center.
   * @param {string} text The top title text.
   */
  setTitleTop(text) {
    this.titleTop_ = text;
    this.draw_();
  }

  /**
   * Sets the bottom title of the gauge.
   * The top title is displayed below the center.
   * @param {string} text The bottom title text.
   */
  setTitleBottom(text) {
    this.titleBottom_ = text;
    this.draw_();
  }

  /**
   * Sets the font for displaying top and bottom titles.
   * @param {?Font} font The font for titles.
   */
  setTitleFont(font) {
    this.titleFont_ = font;
    this.draw_();
  }

  /**
   * Sets the font for displaying the formatted value.
   * @param {?Font} font The font for displaying the value.
   */
  setValueFont(font) {
    this.valueFont_ = font;
    this.drawValue_();
  }

  /**
   * Sets the color theme for drawing the gauge.
   * @param {?GaugeTheme} theme The color theme to use.
   */
  setTheme(theme) {
    this.theme_ = theme;
    this.draw_();
  }

  /**
   * Set the background color for a range of values on the gauge.
   * @param {number} fromValue The lower (start) value of the colored range.
   * @param {number} toValue The higher (end) value of the colored range.
   * @param {string} color The color name to paint the range with. For example
   *     'red' or '#ffcc00'.
   */
  addBackgroundColor(fromValue, toValue, color) {
    this.rangeColors_.push(new GaugeColoredRange(fromValue, toValue, color));
    this.draw_();
  }

  /**
   * Creates the DOM representation of the graphics area.
   * @override
   */
  createDom() {
    this.setElementInternal(this.getDomHelper().createDom(
        TagName.DIV, goog.getCssName('goog-gauge'),
        this.graphics_.getElement()));
  }

  /**
   * Clears the entire graphics area.
   * @private
   */
  clear_() {
    this.graphics_.clear();
    this.needleGroup_ = null;
  }

  /**
   * Redraw the entire gauge.
   * @private
   * @suppress {strictPrimitiveOperators} Part of the
   * go/strict_warnings_migration
   */
  draw_() {
    if (!this.isInDocument()) {
      return;
    }

    this.clear_();

    let x;
    let y;

    const size = Math.min(this.width_, this.height_);
    let r = Math.round(FACTOR_RADIUS_FROM_SIZE * size);
    const cx = this.width_ / 2;
    const cy = this.height_ / 2;

    let theme = this.theme_;
    if (!theme) {
      // Lazy allocation of default theme, common to all instances
      theme = Gauge.prototype.theme_ = new GaugeTheme();
    }

    // Draw main circle frame around gauge
    const graphics = this.graphics_;
    let stroke = this.theme_.getExternalBorderStroke();
    let fill = theme.getExternalBorderFill(cx, cy, r);
    graphics.drawCircle(cx, cy, r, stroke, fill);

    r -= stroke.getWidth();
    r = Math.round(r * FACTOR_MAIN_AREA);
    stroke = theme.getInternalBorderStroke();
    fill = theme.getInternalBorderFill(cx, cy, r);
    graphics.drawCircle(cx, cy, r, stroke, fill);
    r -= stroke.getWidth() * 2;

    // Draw Background with external and internal borders
    const rBackgroundInternal = r * FACTOR_COLOR_RADIUS;
    for (let i = 0; i < this.rangeColors_.length; i++) {
      const rangeColor = this.rangeColors_[i];
      const fromValue = rangeColor.fromValue;
      const toValue = rangeColor.toValue;
      const path = new Path();
      const fromAngle = this.valueToAngle_(fromValue);
      const toAngle = this.valueToAngle_(toValue);
      // Move to outer point at "from" angle
      path.moveTo(
          cx + googMath.angleDx(fromAngle, r),
          cy + googMath.angleDy(fromAngle, r));
      // Arc to outer point at "to" angle
      path.arcTo(r, r, fromAngle, toAngle - fromAngle);
      // Line to inner point at "to" angle
      path.lineTo(
          cx + googMath.angleDx(toAngle, rBackgroundInternal),
          cy + googMath.angleDy(toAngle, rBackgroundInternal));
      // Arc to inner point at "from" angle
      path.arcTo(
          rBackgroundInternal, rBackgroundInternal, toAngle,
          fromAngle - toAngle);
      path.close();
      fill = new SolidFill(rangeColor.backgroundColor);
      graphics.drawPath(path, null, fill);
    }

    // Draw titles
    if (this.titleTop_ || this.titleBottom_) {
      let font = this.titleFont_;
      if (!font) {
        // Lazy creation of font
        const fontSize = Math.round(r * FACTOR_TITLE_FONT_SIZE);
        font = new Font(fontSize, TITLE_FONT_NAME);
        this.titleFont_ = font;
      }
      fill = new SolidFill(theme.getTitleColor());
      if (this.titleTop_) {
        y = cy - Math.round(r * FACTOR_TITLE_OFFSET);
        graphics.drawTextOnLine(
            this.titleTop_, 0, y, this.width_, y, 'center', font, null, fill);
      }
      if (this.titleBottom_) {
        y = cy + Math.round(r * FACTOR_TITLE_OFFSET);
        graphics.drawTextOnLine(
            this.titleBottom_, 0, y, this.width_, y, 'center', font, null,
            fill);
      }
    }

    // Draw tick marks
    const majorTicks = this.majorTicks_;
    const minorTicks = this.minorTicks_;
    const rMajorTickInternal = r * FACTOR_MAJOR_TICKS;
    const rMinorTickInternal = r * FACTOR_MINOR_TICKS;
    const ticks = majorTicks * minorTicks;
    const valueRange = this.maxValue_ - this.minValue_;
    const tickValueSpan = valueRange / ticks;
    const majorTicksPath = new Path();
    const minorTicksPath = new Path();

    const tickLabelFill = new SolidFill(theme.getTickLabelColor());
    let tickLabelFont = this.tickLabelFont_;
    if (!tickLabelFont) {
      tickLabelFont = new Font(
          Math.round(r * FACTOR_TICK_LABEL_FONT_SIZE), TITLE_FONT_NAME);
    }
    const tickLabelFontSize = tickLabelFont.size;

    for (let i = 0; i <= ticks; i++) {
      const angle = this.valueToAngle_(i * tickValueSpan + this.minValue_);
      const isMajorTick = i % minorTicks == 0;
      const rInternal = isMajorTick ? rMajorTickInternal : rMinorTickInternal;
      const path = isMajorTick ? majorTicksPath : minorTicksPath;
      x = cx + googMath.angleDx(angle, rInternal);
      y = cy + googMath.angleDy(angle, rInternal);
      path.moveTo(x, y);
      x = cx + googMath.angleDx(angle, r);
      y = cy + googMath.angleDy(angle, r);
      path.lineTo(x, y);

      // Draw the tick's label for major ticks
      if (isMajorTick && this.majorTickLabels_) {
        const tickIndex = Math.floor(i / minorTicks);
        const label = this.majorTickLabels_[tickIndex];
        if (label) {
          x = cx + googMath.angleDx(angle, rInternal - tickLabelFontSize / 2);
          y = cy + googMath.angleDy(angle, rInternal - tickLabelFontSize / 2);
          let x1;
          let x2;

          let align = 'center';
          if (angle > 280 || angle < 90) {
            align = 'right';
            x1 = 0;
            x2 = x;
          } else if (angle >= 90 && angle < 260) {
            align = 'left';
            x1 = x;
            x2 = this.width_;
          } else {
            // Values around top (angle 260-280) are centered around point
            const dw = Math.min(x, this.width_ - x);  // Nearest side border
            x1 = x - dw;
            x2 = x + dw;
            y += Math.round(tickLabelFontSize / 4);  // Movea bit down
          }
          graphics.drawTextOnLine(
              label, x1, y, x2, y, align, tickLabelFont, null, tickLabelFill);
        }
      }
    }
    stroke = theme.getMinorTickStroke();
    graphics.drawPath(minorTicksPath, stroke, null);
    stroke = theme.getMajorTickStroke();
    graphics.drawPath(majorTicksPath, stroke, null);

    // Draw the needle and the value label. Stop animation when doing
    // full redraw and jump to the final value position.
    this.stopAnimation_();
    this.needleRadius_ = r;
    this.drawValue_();
  }

  /**
   * Handle animation events while the hand is moving.
   * @param {!AnimationEvent} e The event.
   * @private
   */
  onAnimate_(e) {
    this.needleValuePosition_ = e.x;
    this.drawValue_();
  }

  /**
   * Handle animation events when hand move is complete.
   * @private
   */
  onAnimateEnd_() {
    this.stopAnimation_();
  }

  /**
   * Stop the current animation, if it is active.
   * @private
   */
  stopAnimation_() {
    if (this.animation_) {
      googEvents.removeAll(this.animation_);
      this.animation_.stop(false);
      this.animation_ = null;
    }
  }

  /**
   * Convert a value to the position in the range. The returned position
   * is a value between 0 and 1, where 0 indicates the lowest range value,
   * 1 is the highest range, and any value in between is proportional
   * to mapping the range to (0-1).
   * If the value is not within the range, the returned value may be a bit
   * lower than 0, or a bit higher than 1. This is done so that values out
   * of range will be displayed just a bit outside of the gauge axis.
   * @param {number} value The value to convert.
   * @private
   * @return {number} The range position.
   */
  valueToRangePosition_(value) {
    const valueRange = this.maxValue_ - this.minValue_;
    let valuePct = (value - this.minValue_) / valueRange;  // 0 to 1

    // If value is out of range, trim it not to be too much out of range
    valuePct = Math.max(valuePct, -MAX_EXCEED_POSITION_POSITION);
    valuePct = Math.min(valuePct, 1 + MAX_EXCEED_POSITION_POSITION);

    return valuePct;
  }

  /**
   * Convert a value to an angle based on the value range and angle span
   * @param {number} value The value.
   * @return {number} The angle where this value is located on the round
   *     axis, based on the range and angle span.
   * @private
   */
  valueToAngle_(value) {
    const valuePct = this.valueToRangePosition_(value);
    return this.valuePositionToAngle_(valuePct);
  }

  /**
   * Convert a value-position (percent in the range) to an angle based on
   * the angle span. A value-position is a value that has been proportinally
   * adjusted to a value betwwen 0-1, proportionaly to the range.
   * @param {number} valuePct The value.
   * @return {number} The angle where this value is located on the round
   *     axis, based on the range and angle span.
   * @private
   */
  valuePositionToAngle_(valuePct) {
    const startAngle = googMath.standardAngle((360 - this.angleSpan_) / 2 + 90);
    return this.angleSpan_ * valuePct + startAngle;
  }

  /**
   * Draw the elements that depend on the current value (the needle and
   * the formatted value). This function is called whenever a value is changed
   * or when the entire gauge is redrawn.
   * @private
   */
  drawValue_() {
    if (!this.isInDocument()) {
      return;
    }

    const r = this.needleRadius_;
    const graphics = this.graphics_;
    const theme = this.theme_;
    const cx = this.width_ / 2;
    const cy = this.height_ / 2;
    const angle = this.valuePositionToAngle_(
        /** @type {number} */ (this.needleValuePosition_));

    // Compute the needle path
    const frontRadius = Math.round(r * FACTOR_NEEDLE_FRONT);
    const backRadius = Math.round(r * FACTOR_NEEDLE_BACK);
    const frontDx = googMath.angleDx(angle, frontRadius);
    const frontDy = googMath.angleDy(angle, frontRadius);
    const backDx = googMath.angleDx(angle, backRadius);
    const backDy = googMath.angleDy(angle, backRadius);
    const angleRight = googMath.standardAngle(angle + 90);
    const distanceControlPointBase = r * FACTOR_NEEDLE_WIDTH;
    const controlPointMidDx =
        googMath.angleDx(angleRight, distanceControlPointBase);
    const controlPointMidDy =
        googMath.angleDy(angleRight, distanceControlPointBase);

    const path = new Path();
    path.moveTo(cx + frontDx, cy + frontDy);
    path.curveTo(
        cx + controlPointMidDx, cy + controlPointMidDy,
        cx - backDx + (controlPointMidDx / 2),
        cy - backDy + (controlPointMidDy / 2), cx - backDx, cy - backDy);
    path.curveTo(
        cx - backDx - (controlPointMidDx / 2),
        cy - backDy - (controlPointMidDy / 2), cx - controlPointMidDx,
        cy - controlPointMidDy, cx + frontDx, cy + frontDy);

    // Draw the needle hinge
    const rh = Math.round(r * FACTOR_NEEDLE_HINGE);

    // Clean previous needle
    let needleGroup = this.needleGroup_;
    if (needleGroup) {
      needleGroup.clear();
    } else {
      needleGroup = this.needleGroup_ = graphics.createGroup();
    }

    // Draw current formatted value if provided.
    if (this.formattedValue_) {
      let font = this.valueFont_;
      if (!font) {
        const fontSize = Math.round(r * FACTOR_VALUE_FONT_SIZE);
        font = new Font(fontSize, TITLE_FONT_NAME);
        font.bold = true;
        this.valueFont_ = font;
      }
      let fill = new SolidFill(theme.getValueColor());
      const y = cy + Math.round(r * FACTOR_VALUE_OFFSET);
      graphics.drawTextOnLine(
          this.formattedValue_, 0, y, this.width_, y, 'center', font, null,
          fill, needleGroup);
    }

    // Draw the needle
    let stroke = theme.getNeedleStroke();
    let fill = theme.getNeedleFill(cx, cy, rh);
    graphics.drawPath(path, stroke, fill, needleGroup);
    stroke = theme.getHingeStroke();
    fill = theme.getHingeFill(cx, cy, rh);
    graphics.drawCircle(cx, cy, rh, stroke, fill, needleGroup);
  }

  /**
   * Redraws the entire gauge.
   * Should be called after theme colors have been changed.
   */
  redraw() {
    this.draw_();
  }

  /** @override */
  enterDocument() {
    super.enterDocument();

    // set roles and states
    const el = this.getElement();
    asserts.assert(el, 'The DOM element for the gauge cannot be null.');
    aria.setRole(el, 'progressbar');
    aria.setState(el, 'live', 'polite');
    aria.setState(el, 'valuemin', this.minValue_);
    aria.setState(el, 'valuemax', this.maxValue_);
    aria.setState(el, 'valuenow', this.value_);
    this.draw_();
  }

  /** @override */
  exitDocument() {
    super.exitDocument();
    this.stopAnimation_();
  }

  /** @override */
  disposeInternal() {
    this.stopAnimation_();
    this.graphics_.dispose();
    delete this.graphics_;
    delete this.needleGroup_;
    delete this.theme_;
    delete this.rangeColors_;
    super.disposeInternal();
  }
}

exports = Gauge;
