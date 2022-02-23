// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {assert} from '../../js/assert.m.js';

/**
 * TODO(dpapad): shim for not having Animation.finished implemented. Can
 * replace with Animation.finished if Chrome implements it (see:
 * crbug.com/257235).
 * @param {!Animation} animation
 * @return {!Promise}
 */
function whenFinished(animation) {
  return new Promise(function(resolve, reject) {
    animation.addEventListener('finish', resolve);
  });
}

/**
 * @param {!Element} element
 * @return {!Element}
 */
function getEffectiveView(element) {
  return element.matches('cr-lazy-render') ? element.get() : element;
}

/**
 * @param {!Element} element
 * @param {!string} eventType
 */
function dispatchCustomEvent(element, eventType) {
  element.dispatchEvent(
      new CustomEvent(eventType, {bubbles: true, composed: true}));
}

/** @type {!Map<string, function(!Element): !Promise>} */
const viewAnimations = new Map();
viewAnimations.set('fade-in', element => {
  const animation = element.animate(
      [{opacity: 0}, {opacity: 1}],
      /** @type {!KeyframeAnimationOptions } */ ({
        duration: 180,
        easing: 'ease-in-out',
        iterations: 1,
      }));

  return whenFinished(animation);
});
viewAnimations.set('fade-out', element => {
  const animation = element.animate(
      [{opacity: 1}, {opacity: 0}],
      /** @type {!KeyframeAnimationOptions} */ ({
        duration: 180,
        easing: 'ease-in-out',
        iterations: 1,
      }));

  return whenFinished(animation);
});
viewAnimations.set('slide-in-fade-in-ltr', element => {
  const animation = element.animate(
      [
        {transform: 'translateX(-8px)', opacity: 0},
        {transform: 'translateX(0)', opacity: 1}
      ],
      /** @type {!KeyframeAnimationOptions} */ ({
        duration: 300,
        easing: 'cubic-bezier(0.0, 0.0, 0.2, 1)',
        fill: 'forwards',
        iterations: 1,
      }));

  return whenFinished(animation);
});
viewAnimations.set('slide-in-fade-in-rtl', element => {
  const animation = element.animate(
      [
        {transform: 'translateX(8px)', opacity: 0},
        {transform: 'translateX(0)', opacity: 1}
      ],
      /** @type {!KeyframeAnimationOptions} */ ({
        duration: 300,
        easing: 'cubic-bezier(0.0, 0.0, 0.2, 1)',
        fill: 'forwards',
        iterations: 1,
      }));

  return whenFinished(animation);
});

/** @polymer */
export class CrViewManagerElement extends PolymerElement {
  static get is() {
    return 'cr-view-manager';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  /**
   * @param {!Element} element
   * @param {string} animation
   * @return {!Promise}
   * @private
   */
  exit_(element, animation) {
    const animationFunction = viewAnimations.get(animation);
    element.classList.remove('active');
    element.classList.add('closing');
    dispatchCustomEvent(element, 'view-exit-start');
    if (!animationFunction) {
      // Nothing to animate. Immediately resolve.
      element.classList.remove('closing');
      dispatchCustomEvent(element, 'view-exit-finish');
      return Promise.resolve();
    }
    return animationFunction(element).then(() => {
      element.classList.remove('closing');
      dispatchCustomEvent(element, 'view-exit-finish');
    });
  }

  /**
   * @param {!Element} view
   * @param {string} animation
   * @return {!Promise}
   * @private
   */
  enter_(view, animation) {
    const animationFunction = viewAnimations.get(animation);
    const effectiveView = getEffectiveView(view);
    effectiveView.classList.add('active');
    dispatchCustomEvent(effectiveView, 'view-enter-start');
    if (!animationFunction) {
      // Nothing to animate. Immediately resolve.
      dispatchCustomEvent(effectiveView, 'view-enter-finish');
      return Promise.resolve();
    }
    return animationFunction(effectiveView).then(() => {
      dispatchCustomEvent(effectiveView, 'view-enter-finish');
    });
  }

  /**
   * @param {string} newViewId
   * @param {string=} enterAnimation
   * @param {string=} exitAnimation
   * @return {!Promise}
   */
  switchView(newViewId, enterAnimation, exitAnimation) {
    const previousView = this.querySelector('.active');
    const newView = assert(this.querySelector('#' + newViewId));

    const promises = [];
    if (previousView) {
      promises.push(this.exit_(previousView, exitAnimation || 'fade-out'));
      promises.push(this.enter_(newView, enterAnimation || 'fade-in'));
    } else {
      promises.push(this.enter_(newView, 'no-animation'));
    }

    return Promise.all(promises);
  }
}

customElements.define(CrViewManagerElement.is, CrViewManagerElement);
