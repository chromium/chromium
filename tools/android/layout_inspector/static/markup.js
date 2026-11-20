// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/******** Markup Constants ********/

/**
 * Roles used by SVG Components that derive from SvgMarkupItem.
 * @enum {string}
 */
const MARKUP_ROLE = {
  NONE: '',
  HIGHLIGHT: 'highlight',
};

/**
 * CSS class names identifying transient interactive states and display
 * modifiers.
 * @enum {string}
 */
const MARKUP_STATUS = {
  HOVER: 'hover',
};

/******** SvgMarkupItem ********/
/**
 * Base class for UI components backed by an SVG element, standardizing
 * role assignment and visibility management.
 */
class SvgMarkupItem {
  /** @type {MARKUP_ROLE} */
  static ROLE = MARKUP_ROLE.NONE;

  /** @param {!SVGElement} root */
  constructor(root) {
    /** @type {!SVGElement} */
    this.root = root;
    this.root.setAttribute('data-role', this.constructor.ROLE);
  }

  /**
   * Toggles visibility of the component's root element.
   * @param {boolean} visible
   */
  setVisible(visible) {
    g_svg.edit(this.root).display(visible ? null : 'none');
  }
}

/******** SvgMarkupTheme ********/
/**
 * Manages the dynamic scoped stylesheet and export styling for markup overlays.
 * The styles are injected into the main app and into temporary SVG used when
 * flattening Screenshot and Markup to image.
 */
class SvgMarkupTheme {
  static STYLE_VARS = [
    '--color-hover',
    '--bg-hover',
  ];

  /**
   * Joint SVG markup CSS rules, stored statically to enable SVG export.
   * @type {string}
   */
  static cssStore = '';

  // Populate role-independent styles.
  static {
    const {HOVER} = MARKUP_STATUS;
    this.cssStore += `
/* State & Variation variable reassignments. */
.svg-markup {
  .${HOVER} {
    --color-solid: var(--color-hover);
    --color-shade: var(--bg-hover);
  }
}

/* Derived class bindings. */
`;
  }

  /**
   * Registers CSS presentation rules for an `SvgMarkupItem`-derived component
   * into {@link SvgMarkupTheme.cssStore}. Maps specified CSS property names to
   * semantic theme variables (`--color-solid` and `--color-shade`) scoped to
   * the component's `data-role` selector.
   *
   * @param {MARKUP_ROLE} role
   * @param {!Array<string>} subselectors Relative selectors within the
   *     component root (e.g., ['rect'], ['> rect'], or [] for root itself).
   * @param {!Array<string>} solidStyles CSS property names mapped to
   *     `--color-solid` (e.g., ['fill'] or ['stroke']).
   * @param {!Array<string>} shadeStyles CSS property names mapped to
   *     `--color-shade` (e.g., ['fill']).
   */
  static registerCss(role, subselectors, solidStyles, shadeStyles) {
    const localCss = [];
    const selectors = [`[data-role="${role}"]`, ...subselectors];
    localCss.push('.svg-markup {');
    localCss.push(`  ${selectors.join(' ')} {`);
    for (const style of solidStyles) {
      localCss.push(`    ${style}: var(--color-solid);`);
    }
    for (const style of shadeStyles) {
      localCss.push(`    ${style}: var(--color-shade);`);
    }
    localCss.push('  }');
    localCss.push('}');
    localCss.push('');
    SvgMarkupTheme.cssStore += localCss.join('\n');
  }

  /**
   * Injects the dynamically generated SVG stylesheet into the document head.
   * Should be called during DOMContentLoaded.
   */
  static injectGlobalCss() {
    const style = document.createElement('style');
    style.id = 'svg-dynamic-styles';
    style.textContent = SvgMarkupTheme.cssStore;
    document.head.appendChild(style);
  }
}

/******** ViewHighlight ********/
/**
 * Renders a dashed rectangular SVG overlay over a {@link ViewNode}. It
 * correctly snaps to inclusive-inclusive screen coordinates so that 1px strokes
 * align perfectly with the underlying pixels regardless of the zoom scale.
 */
class ViewHighlight extends SvgMarkupItem {
  static ROLE = MARKUP_ROLE.HIGHLIGHT;
  static {
    SvgMarkupTheme.registerCss(this.ROLE, ['> path'], ['stroke'], ['fill']);
  }

  constructor() {
    super(g_svg.init('g').get());

    this.rectElt =
        g_svg.init('path').strokeWidth('1').strokeDasharray('2,2').get();

    g_svg.edit(this.root).append(this.rectElt);

    this.rect = null;
    this.setVisible(false);
  }

  _updatePlacement(visOpts) {
    const sLeft = visOpts.w2sFloor(this.rect.left);
    const sTop = visOpts.w2sFloor(this.rect.top);
    const sRight = visOpts.w2sFloor(this.rect.right);
    const sBottom = visOpts.w2sFloor(this.rect.bottom);
    g_svg.edit(this.rectElt).pathInnerRect(sLeft, sTop, sRight, sBottom);
  }

  /**
   * @param {!VisOptions} visOpts
   */
  redraw(visOpts) {
    if (this.rect) this._updatePlacement(visOpts);
  }

  /**
   * @param {!Rectangle} rect
   * @param {!VisOptions} visOpts
   */
  setRectAndRedraw(rect, visOpts) {
    this.rect = rect;
    this.redraw(visOpts);
  }
}
