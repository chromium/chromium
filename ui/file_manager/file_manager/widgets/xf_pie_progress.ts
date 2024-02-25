// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {css, customElement, html, property, svg, XfBase} from './xf_base.js';

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

  private outlineShape = svg`
    <circle
      class="outline"
      cx="${this.center}"
      cy="${this.center}"
      r="${this.center}"
    />`;
  private queuedShape = svg`
    <circle
      class="queued"
      stroke-width="1.6"
      cx="${this.center}"
      cy="${this.center}"
      r="5.6"
    />`;
  private edgeShape = svg`
    <circle
      class="edge"
      stroke-width="2"
      cx="${this.center}"
      cy="${this.center}"
      r="${this.radius}"
    />`;

  static override get styles() {
    return getCSS();
  }

  override render() {
    const {progress, size, center, radius} = this;

    const isQueued = progress === 0;

    // The progress pie is drawn as an arc with a thick stroke width (as thick
    // as the radius of the pie).
    const arcRadius = radius * 0.5;
    const maxArcLength = 2.0 * Math.PI * arcRadius;
    const pie = svg`
      <circle class="pie"
        stroke-width="${radius}"
        stroke-dasharray="${maxArcLength}"
        stroke-dashoffset="${maxArcLength * (1 - progress)}"
        cx="${center}"
        cy="${center}"
        r="${arcRadius}"
        transform="rotate(-90, ${center}, ${center})"
        visibility="${isQueued ? 'hidden' : 'visible'}"
      />`;

    const edge = isQueued ? this.queuedShape : this.edgeShape;

    return html`
      <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 ${size} ${size}">
        ${this.outlineShape} ${pie} ${edge}
      </svg>
    `;
  }
}

function getCSS() {
  return css`
    svg {
      height: 100%;
      width: 100%;
    }

    .queued {
      fill: none;
      stroke: currentColor
    }

    .edge {
      fill: none;
      stroke: var(--cros-sys-progress);
    }

    .pie {
      fill: none;
      stroke: var(--cros-sys-progress);
      transition: stroke-dashoffset 1s ease-out;
    }

    .outline {
      fill: var(--xf-icon-color-outline, transparent);
      stroke: none;
    }
  `;
}
