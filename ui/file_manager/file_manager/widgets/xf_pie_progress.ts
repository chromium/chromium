// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {addCSSPrefixSelector} from '../common/js/dom_utils.js';

import {css, customElement, html, property, svg, XfBase} from './xf_base.js';

const TWO_PI = 2.0 * Math.PI;
const HALF_PI = 0.5 * Math.PI;

/**
 * Displays a pie shaped progress indicator.
 * Accepts a `progress` property ranging from 0 to 1.
 */
@customElement('xf-pie-progress')
export class XfPieProgress extends XfBase {
  // This should be a number between 0 and 1.
  @property({type: Number, reflect: true}) progress = 0;

  private size = 16;  // Size of the SVG square measured by its side length.
  private center = this.size / 2.0;  // Center of the pie circle (both X and Y).
  private radius = 5.4;              // Radius of the pie circle.

  private outlineShape = svg`<circle
    class="outline"
    cx="${this.center}"
    cy="${this.center}"
    r="${this.center}"
  />`;
  private queuedShape = svg`<circle
      class="queued"
      cx="${this.center}"
      cy="${this.center}"
      r="5.6"
      fill="none"
      stroke-width="1.6"
    />;`;

  static override get styles() {
    return getCSS();
  }

  override render() {
    const {progress, size, center, radius} = this;

    let contents = svg``;

    if (progress === 0) {
      // Display the queued shape.
      contents = this.queuedShape;
    } else if (progress >= 0.99) {
      // The completed pie is easier to draw.
      contents = svg`
        <circle
          class="edge full"
          stroke-width="2"
          cx="${center}"
          cy="${center}"
          r="${radius}"
        />
      `;
    } else {
      // Finishing angle of the pie arc. Notice that the starting angle is
      // always -PI/2. I.e., the pie is drawn starting from the top of the
      // circle and it advances in a clockwise fashion.
      const radians = TWO_PI * progress - HALF_PI;

      // Finishing cartesian coordinates of the pie arc. Notice that the
      // starting coordinates are always <0, -radius> (the top of the circle).
      const x = center + radius * Math.cos(radians);
      const y = center + radius * Math.sin(radians);

      // Determines which arc fitting the other arguments should be rendered.
      // Render the smaller one until we are drawing an arc with an angle
      // greater than 180 degrees. More info:
      // https://www.w3.org/TR/SVG/paths.html#PathDataEllipticalArcCommands
      const largeArcFlag = progress <= 0.5 ? '0' : '1';

      contents = svg`<circle
          class="edge"
          stroke-width="2"
          cx="${center}"
          cy="${center}"
          r="${radius}"
        />
        <path
          class="pie"
          d="
    M ${center} ${center}
    l 0 ${- radius}
    A ${radius} ${radius} 0 ${largeArcFlag} 1 ${x} ${y}
    Z"
        />`;
    }

    return html`
      <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 ${size} ${size}">
        ${this.outlineShape} ${contents}
      </svg>
    `;
  }
}

function getCSS() {
  const legacyStyle = css`
    svg {
      height: 100%;
      width: 100%;
    }

    .queued {
      stroke: var(--cros-icon-color-secondary);
    }

    .edge {
      fill: none;
      stroke: var(--cros-icon-color-prominent);
    }

    .full {
      fill: var(--cros-icon-color-prominent);
    }

    .pie {
      fill: var(--cros-icon-color-prominent);
      stroke: none;
    }

    .outline {
      fill: var(--xf-icon-color-outline, transparent);
    }
  `;

  const refresh23Style = css`
    svg {
      height: 100%;
      width: 100%;
    }

    .queued {
      stroke: currentColor
    }

    .edge {
      fill: none;
      stroke: var(--cros-sys-progress);
    }

    .full {
      fill: var(--cros-sys-progress);
    }

    .pie {
      fill: var(--cros-sys-progress);
      stroke: none;
    }

    .outline {
      fill: var(--xf-icon-color-outline, transparent);
    }
  `;

  return [
    addCSSPrefixSelector(legacyStyle, '[theme="legacy"]'),
    addCSSPrefixSelector(refresh23Style, '[theme="refresh23"]'),
  ];
}
