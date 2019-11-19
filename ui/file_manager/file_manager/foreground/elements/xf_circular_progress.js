// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Definition of a circular progress indicator custom element.
 * The element supports two attributes for control - 'radius' and 'progress'.
 * Default radius for the element is 10px, use the DOM API
 *   element.setAttribute('radius', '12px'); to set the radius to 12px;
 * Default progress is 0, progress is a value from 0 to 100, use
 *   element.setAttribute('progress', '50'); to set progress to half complete
 * or alternately, set the 'element.progress' JS property for the same result.
 */
class CircularProgress extends HTMLElement {
  constructor() {
    super();
    const host = document.createElement('template');
    host.innerHTML = this.constructor.template_;
    this.attachShadow({mode: 'open'}).appendChild(host.content.cloneNode(true));

    /** @private {number} */
    this.progress_ = 0.0;

    /**
     * The visual indicator for the progress is accomplished by changing the
     * stroke-dasharray SVG attribute on the top circle. The stroke-dasharray
     * is calculated by using the circumference of the circle as the 100%
     * length and then setting the dash length to match the percentage of
     * the set 'progress_' value.
     * @private {Element}
     */
    this.indicator_ = this.shadowRoot.querySelector('.top');

    /** @private {Element} */
    this.errormark_ = assert(this.shadowRoot.querySelector('.errormark'));

    /** @private {Element} */
    this.label_ = this.shadowRoot.querySelector('.label');

    /** @private {number} */
    this.maxProgress_ = 100.0;

    /**
     * The circumference for the circle (default 63 for radius r='10').
     * @private {number}
     */
    this.fullCircle_ = 63;
  }

  /**
   * Static getter for the custom element template.
   * @private
   */
  static get template_() {
    return `<style>
                    .progress {
                        height: 36px;
                        width: 36px;
                    }
                    .bottom {
                        stroke: rgb(210, 227, 252);
                        fill: none;
                    }
                    .top {
                        stroke: rgb(26, 115, 232);
                        stroke-linecap: round;
                        fill: none;
                    }
                    text {
                        font: bold 14px Roboto;
                        fill: rgb(26, 115, 232);
                    }
                </style>
                <div class='progress'>
                    <svg xmlns='http://www.w3.org/2000/svg'
                        viewBox='0 0 36 36'>
                        <g id='circles' stroke-width='3'>
                          <circle class='bottom' cx='18' cy='18' r='10'/>
                          <circle class='top' transform='rotate(-90 18 18)'
                            cx='18' cy='18' r='10' stroke-dasharray='0 1'/>
                        </g>
                        <text class='label' x='18' y='18' text-anchor='middle'
                            alignment-baseline='central'></text>
                        <circle class='errormark' visibility='hidden'
                            cx='25.5' cy='10.5' r='4'
                            fill='#D93025' stroke='none'/>
                    </svg>
                </div>`;
  }

  /**
   * Registers this instance to listen to these attribute changes.
   * @private
   */
  static get observedAttributes() {
    return [
      'errormark',
      'label',
      'progress',
      'radius',
    ];
  }

  /**
   * Sets the indicators progress position.
   * @param {number} progress A value between 0 and maxProgress_ to indicate.
   * @return {number}
   * @public
   */
  setProgress(progress) {
    // Clamp progress to 0 .. maxProgress_.
    progress = Math.min(Math.max(progress, 0), this.maxProgress_);
    const value = (progress / this.maxProgress_) * this.fullCircle_;
    this.indicator_.setAttribute(
        'stroke-dasharray', value + ' ' + this.fullCircle_);
    return progress;
  }

  /**
   * Sets the position of the error indicator.
   * The error indicator is used by the summary panel. Its position is aligned
   * with the top-right square that contains the progress circle itself.
   * @param {number} radius The radius of the progress circle.
   * @param {number} strokeWidth The width of the progress circle stroke.
   * @private
   */
  setErrorPosition_(radius, strokeWidth) {
    const center = 18;
    const x = center + radius + (strokeWidth / 2) - 4;
    const y = center - radius - (strokeWidth / 2) + 4;
    this.errormark_.setAttribute('cx', x);
    this.errormark_.setAttribute('cy', y);
  }

  /**
   * Callback triggered by the browser when our attribute values change.
   * TODO(crbug.com/947388) Add unit tests to exercise attribute edge cases.
   * @param {string} name Attribute that's changed.
   * @param {?string} oldValue Old value of the attribute.
   * @param {?string} newValue New value of the attribute.
   * @private
   */
  attributeChangedCallback(name, oldValue, newValue) {
    if (oldValue === newValue) {
      return;
    }
    switch (name) {
      case 'errormark':
        this.errormark_.setAttribute('visibility', newValue || '');
        break;
      case 'label':
        this.label_.textContent = newValue;
        break;
      case 'radius':
        if (!newValue) {
          break;
        }
        const radius = Number(newValue);
        // Restrict the allowed size to what fits in our area.
        if (radius < 0 || radius > 16.5) {
          return;
        }
        let strokeWidth = 3;
        if (radius > 10) {
          const circles = this.shadowRoot.querySelector('#circles');
          circles.setAttribute('stroke-width', '4');
          strokeWidth = 4;
        }
        // Position the error indicator relative to the progress circle.
        this.setErrorPosition_(radius, strokeWidth);
        // Calculate the circumference for the progress dash length.
        this.fullCircle_ = Math.PI * 2 * radius;
        const bottom = this.shadowRoot.querySelector('.bottom');
        bottom.setAttribute('r', radius.toString());
        this.indicator_.setAttribute('r', radius.toString());
        this.setProgress(this.progress_);
        break;
      case 'progress':
        const progress = Number(newValue);
        this.progress_ = this.setProgress(progress);
        break;
    }
  }

  /**
   * Getter for the visibility of the error marker.
   * @public
   * @return {string}
   */
  get errorMarkerVisibility() {
    return this.errormark_.getAttribute('visibility');
  }

  /**
   * Set the visibility of the error marker.
   * @param {string} visibility Visibility value being set.
   * @public
   */
  set errorMarkerVisibility(visibility) {
    // Reflect the progress property into the attribute.
    this.setAttribute('errormark', visibility);
  }

  /**
   * Getter for the current state of the progress indication.
   * @public
   * @return {string}
   */
  get progress() {
    return this.progress_.toString();
  }

  /**
   * Sets the progress position between 0 and 100.0.
   * @param {string} progress Progress value being set.
   * @public
   */
  set progress(progress) {
    // Reflect the progress property into the attribute.
    this.setAttribute('progress', progress);
  }

  /**
   * Set the text label in the centre of the progress indicator.
   * This is used to indicate multiple operations in progress.
   * @param {string} label Text to place inside the circle.
   * @public
   */
  set label(label) {
    this.setAttribute('label', label);
  }
}

window.customElements.define('xf-circular-progress', CircularProgress);
