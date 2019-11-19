/**
@license
Copyright (c) 2015 The Polymer Project Authors. All rights reserved.
This code may only be used under the BSD style license found at
http://polymer.github.io/LICENSE.txt The complete set of authors may be found at
http://polymer.github.io/AUTHORS.txt The complete set of contributors may be
found at http://polymer.github.io/CONTRIBUTORS.txt Code distributed by Google as
part of the polymer project is also subject to an additional IP rights grant
found at http://polymer.github.io/PATENTS.txt
*/
import '../polymer/polymer_bundled.min.js';

/**
 * Use `NeonAnimationBehavior` to implement an animation.
 * @polymerBehavior
 */
export const NeonAnimationBehavior = {

  properties: {

    /**
     * Defines the animation timing.
     */
    animationTiming: {
      type: Object,
      value: function() {
        return {
          duration: 500, easing: 'cubic-bezier(0.4, 0, 0.2, 1)', fill: 'both'
        }
      }
    }

  },

  /**
   * Can be used to determine that elements implement this behavior.
   */
  isNeonAnimation: true,

  /**
   * Do any animation configuration here.
   */
  // configure: function(config) {
  // },

  created: function() {
    if (!document.body.animate) {
      console.warn(
          'No web animations detected. This element will not' +
          ' function without a web animations polyfill.');
    }
  },

  /**
   * Returns the animation timing by mixing in properties from `config` to the
   * defaults defined by the animation.
   */
  timingFromConfig: function(config) {
    if (config.timing) {
      for (var property in config.timing) {
        this.animationTiming[property] = config.timing[property];
      }
    }
    return this.animationTiming;
  },

  /**
   * Sets `transform` and `transformOrigin` properties along with the prefixed
   * versions.
   */
  setPrefixedProperty: function(node, property, value) {
    var map = {
      'transform': ['webkitTransform'],
      'transformOrigin': ['mozTransformOrigin', 'webkitTransformOrigin']
    };
    var prefixes = map[property];
    for (var prefix, index = 0; prefix = prefixes[index]; index++) {
      node.style[prefix] = value;
    }
    node.style[property] = value;
  },

  /**
   * Called when the animation finishes.
   */
  complete: function(config) {}

};
