// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
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

/** @type {!Map<string, function(!Element): !Promise>} */
const viewAnimations = new Map();
viewAnimations.set('no-animation', () => Promise.resolve());
viewAnimations.set('fade-in', element => {
  // The call to animate can have 2 methods of passing the keyframes, however as
  // of the current closure version, only one of them is supported. See
  // https://crbug.com/987842 for more info.
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
  // The call to animate can have 2 methods of passing the keyframes, however as
  // of the current closure version, only one of them is supported. See
  // https://crbug.com/987842 for more info.
  const animation = element.animate(
      [{opacity: 1}, {opacity: 0}],
      /** @type {!KeyframeAnimationOptions} */ ({
        duration: 180,
        easing: 'ease-in-out',
        iterations: 1,
      }));

  return whenFinished(animation);
});

Polymer({
  is: 'cr-view-manager',

  /**
   * @param {!Element} element
   * @param {string} animation
   * @return {!Promise}
   * @private
   */
  exit_: function(element, animation) {
    const animationFunction = viewAnimations.get(animation);
    assert(animationFunction);

    element.classList.remove('active');
    element.classList.add('closing');
    element.dispatchEvent(
        new CustomEvent('view-exit-start', {bubbles: true, composed: true}));
    return animationFunction(element).then(function() {
      element.classList.remove('closing');
      element.dispatchEvent(
          new CustomEvent('view-exit-finish', {bubbles: true, composed: true}));
    });
  },

  /**
   * @param {!Element} view
   * @param {string} animation
   * @return {!Promise}
   * @private
   */
  enter_: function(view, animation) {
    const animationFunction = viewAnimations.get(animation);
    assert(animationFunction);

    const effectiveView = view.matches('cr-lazy-render') ? view.get() : view;

    effectiveView.classList.add('active');
    effectiveView.dispatchEvent(
        new CustomEvent('view-enter-start', {bubbles: true, composed: true}));
    return animationFunction(effectiveView).then(() => {
      effectiveView.dispatchEvent(new CustomEvent(
          'view-enter-finish', {bubbles: true, composed: true}));
    });
  },

  /**
   * @param {string} newViewId
   * @param {string=} enterAnimation
   * @param {string=} exitAnimation
   * @return {!Promise}
   */
  switchView: function(newViewId, enterAnimation, exitAnimation) {
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
  },
});
})();
