// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {getTemplate} from './xf_circular_progress.html.js';

const MAX_PROGRESS = 100.0;

/**
 * Definition of a circular progress indicator custom element.
 * The element supports two attributes for control - 'radius' and 'progress'.
 * Default radius for the element is 10px, use the DOM API
 *   element.setAttribute('radius', '12px'); to set the radius to 12px;
 * Default progress is 0, progress is a value from 0 to 100, use
 *   element.setAttribute('progress', '50'); to set progress to half complete
 * or alternately, set the 'element.progress' JS property for the same result.
 */
export class CircularProgress extends HTMLElement {
  private fullCircle_ = 63;
  private progress_ = 0.0;

  /**
   * The visual indicator for the progress is accomplished by changing the
   * stroke-dasharray SVG attribute on the top circle. The stroke-dasharray
   * is calculated by using the circumference of the circle as the 100%
   * length and then setting the dash length to match the percentage of
   * the set 'progress_' value.
   */
  private indicator_: SVGElement;
  private errormark_: SVGElement;
  private label_: SVGElement;

  constructor() {
    super();

    const template = document.createElement('template');
    template.innerHTML = getTemplate() as unknown as string;
    const fragment = template.content.cloneNode(true);
    this.attachShadow({mode: 'open'}).appendChild(fragment);

    this.indicator_ = this.shadowRoot!.querySelector<SVGElement>('.top')!;
    this.errormark_ = this.shadowRoot!.querySelector<SVGElement>('.errormark')!;
    this.label_ = this.shadowRoot!.querySelector<SVGElement>('.label')!;
  }

  static get is() {
    return 'xf-circular-progress' as const;
  }

  /**
   * Registers this instance to listen to these attribute changes.
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
   * @param progress A value between 0 and MAX_PROGRESS to indicate.
   */
  setProgress(progress: number): number {
    // Clamp progress to 0 .. MAX_PROGRESS.
    progress = Math.min(Math.max(progress, 0), MAX_PROGRESS);
    const value = (progress / MAX_PROGRESS) * this.fullCircle_;
    this.indicator_?.setAttribute(
        'stroke-dasharray', value + ' ' + this.fullCircle_);
    return progress;
  }

  /**
   * Sets the position of the error indicator.
   * The error indicator is used by the summary panel. Its position is aligned
   * with the top-right square that contains the progress circle itself.
   * @param radius The radius of the progress circle.
   * @param strokeWidth The width of the progress circle stroke.
   */
  private setErrorPosition_(radius: number, strokeWidth: number) {
    const center = 18;
    const x = center + radius + (strokeWidth / 2) - 4;
    const y = center - radius - (strokeWidth / 2) + 4;
    this.errormark_.setAttribute('cx', x.toString());
    this.errormark_.setAttribute('cy', y.toString());
  }

  /**
   * Callback triggered by the browser when our attribute values change.
   * TODO(crbug.com/40620728) Add unit tests to exercise attribute edge cases.
   * @param name Attribute that's changed.
   * @param oldValue Old value of the attribute.
   * @param newValue New value of the attribute.
   */
  attributeChangedCallback(
      name: string, oldValue: null|string, newValue: null|string) {
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
          const circles = this.shadowRoot?.querySelector('#circles');
          circles?.setAttribute('stroke-width', '4');
          strokeWidth = 4;
        }
        // Position the error indicator relative to the progress circle.
        this.setErrorPosition_(radius, strokeWidth);
        // Calculate the circumference for the progress dash length.
        this.fullCircle_ = Math.PI * 2 * radius;
        const bottom = this.shadowRoot?.querySelector('.bottom');
        bottom?.setAttribute('r', radius.toString());
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
   */
  get errorMarkerVisibility(): string {
    return this.errormark_.getAttribute('visibility') || '';
  }

  /**
   * Set the visibility of the error marker.
   * @param visibility Visibility value being set.
   */
  set errorMarkerVisibility(visibility: string) {
    // Reflect the progress property into the attribute.
    this.setAttribute('errormark', visibility);
  }

  /**
   * Getter for the current state of the progress indication.
   */
  get progress(): string {
    return this.progress_.toString();
  }

  /**
   * Sets the progress position between 0 and 100.0.
   * @param progress Progress value being set.
   */
  set progress(progress: string) {
    // Reflect the progress property into the attribute.
    this.setAttribute('progress', progress);
  }

  /**
   * Set the text label in the centre of the progress indicator.
   * This is used to indicate multiple operations in progress.
   * @param label Text to place inside the circle.
   */
  set label(label: string) {
    this.setAttribute('label', label);
  }
}

declare global {
  interface HTMLElementTagNameMap {
    [CircularProgress.is]: CircularProgress;
  }
}

window.customElements.define(CircularProgress.is, CircularProgress);
