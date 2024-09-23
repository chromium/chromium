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

import {IronRangeBehavior} from '../iron-range-behavior/iron-range-behavior.js';
import {Polymer} from '../polymer/polymer_bundled.min.js';
import {html} from '../polymer/polymer_bundled.min.js';

/**
Material design: [Progress &
activity](https://www.google.com/design/spec/components/progress-activity.html)

The progress bars are for situations where the percentage completed can be
determined. They give users a quick sense of how much longer an operation
will take.

Example:

    <paper-progress value="10"></paper-progress>

There is also a secondary progress which is useful for displaying intermediate
progress, such as the buffer level during a streaming playback progress bar.

Example:

    <paper-progress value="10" secondary-progress="30"></paper-progress>

### Styling progress bar:

To change the active progress bar color:

    paper-progress {
       --paper-progress-active-color: #e91e63;
    }

To change the secondary progress bar color:

    paper-progress {
      --paper-progress-secondary-color: #f8bbd0;
    }

To change the progress bar background color:

    paper-progress {
      --paper-progress-container-color: #64ffda;
    }

Add the class `transiting` to a paper-progress to animate the progress bar when
the value changed. You can also customize the transition:

    paper-progress {
      --paper-progress-transition-duration: 0.08s;
      --paper-progress-transition-timing-function: ease;
      --paper-progress-transition-delay: 0s;
    }

To change the duration of the indeterminate cycle:

    paper-progress {
      --paper-progress-indeterminate-cycle-duration: 2s;
    }

The following mixins are available for styling:

Custom property | Description | Default
----------------|-------------|---------
`--paper-progress-transition-duration` | Duration of the transition | `0.08s`
`--paper-progress-transition-timing-function` | The timing function for the transition | `ease`
`--paper-progress-transition-delay` | delay for the transition | `0s`
`--paper-progress-container-color` | Color of the container | `--google-grey-300`
`--paper-progress-active-color` | The color of the active bar | `--google-green-500`
`--paper-progress-secondary-color` | The color of the secondary bar | `--google-green-100`
`--paper-progress-disabled-active-color` | The color of the active bar if disabled | `--google-grey-500`
`--paper-progress-disabled-secondary-color` | The color of the secondary bar if disabled  | `--google-grey-300`
`--paper-progress-height` | The height of the progress bar | `4px`
`--paper-progress-indeterminate-cycle-duration` | Duration of an indeterminate cycle | `2s`

@group Paper Elements
@element paper-progress
@demo demo/index.html
*/
Polymer({
  _template: html`
    <style>
      :host {
        display: block;
        width: 200px;
        position: relative;
        overflow: hidden;
        --google-green-100: #b7e1cd;
        --google-green-500: #0f9d58;
        --google-grey-300: #e0e0e0;
        --google-grey-500: #9e9e9e;
      }

      :host([hidden]), [hidden] {
        display: none !important;
      }

      #progressContainer {
        position: relative;
      }

      #progressContainer,
      /* the stripe for the indeterminate animation*/
      .indeterminate::after {
        height: var(--paper-progress-height, 4px);
      }

      #primaryProgress,
      #secondaryProgress,
      .indeterminate::after {
        position: absolute;
        top: 0;
        right: 0;
        bottom: 0;
        left: 0;
      }

      #progressContainer,
      .indeterminate::after {
        background: var(--paper-progress-container-color, var(--google-grey-300));
      }

      :host(.transiting) #primaryProgress,
      :host(.transiting) #secondaryProgress {
        transition-property: transform;

        /* Duration */
        transition-duration: var(--paper-progress-transition-duration, 0.08s);

        /* Timing function */
        transition-timing-function: var(--paper-progress-transition-timing-function, ease);

        /* Delay */
        transition-delay: var(--paper-progress-transition-delay, 0s);
      }

      #primaryProgress,
      #secondaryProgress {
        transform-origin: left center;
        transform: scaleX(0);
        will-change: transform;
      }

      #primaryProgress {
        background: var(--paper-progress-active-color, var(--google-green-500));
      }

      #secondaryProgress {
        background: var(--paper-progress-secondary-color, var(--google-green-100));
      }

      :host([disabled]) #primaryProgress {
        background: var(--paper-progress-disabled-active-color, var(--google-grey-500));
      }

      :host([disabled]) #secondaryProgress {
        background: var(--paper-progress-disabled-secondary-color, var(--google-grey-300));
      }

      :host(:not([disabled])) #primaryProgress.indeterminate {
        transform-origin: right center;
        animation: indeterminate-bar var(--paper-progress-indeterminate-cycle-duration, 2s) linear infinite;
      }

      :host(:not([disabled])) #primaryProgress.indeterminate::after {
        content: "";
        transform-origin: center center;

        animation: indeterminate-splitter var(--paper-progress-indeterminate-cycle-duration, 2s) linear infinite;
      }

      @-webkit-keyframes indeterminate-bar {
        0% {
        }
        50% {
        }
        75% {
        }
        100% {
        }
      }

      @-webkit-keyframes indeterminate-splitter {
        0% {
        }
        30% {
        }
        90% {
        }
        100% {
        }
      }

      @keyframes indeterminate-bar {
        0% {
          transform: scaleX(1) translateX(-100%);
        }
        50% {
          transform: scaleX(1) translateX(0%);
        }
        75% {
          transform: scaleX(1) translateX(0%);
          animation-timing-function: cubic-bezier(.28,.62,.37,.91);
        }
        100% {
          transform: scaleX(0) translateX(0%);
        }
      }

      @keyframes indeterminate-splitter {
        0% {
          transform: scaleX(.75) translateX(-125%);
        }
        30% {
          transform: scaleX(.75) translateX(-125%);
          animation-timing-function: cubic-bezier(.42,0,.6,.8);
        }
        90% {
          transform: scaleX(.75) translateX(125%);
        }
        100% {
          transform: scaleX(.75) translateX(125%);
        }
      }
    </style>

    <div id="progressContainer">
      <div id="secondaryProgress" hidden\$="[[_hideSecondaryProgress(secondaryRatio)]]"></div>
      <div id="primaryProgress"></div>
    </div>
`,

  is: 'paper-progress',
  behaviors: [IronRangeBehavior],

  properties: {
    /**
     * The number that represents the current secondary progress.
     */
    secondaryProgress: {type: Number, value: 0},

    /**
     * The secondary ratio
     */
    secondaryRatio: {type: Number, value: 0, readOnly: true},

    /**
     * Use an indeterminate progress indicator.
     */
    indeterminate:
        {type: Boolean, value: false, observer: '_toggleIndeterminate'},

    /**
     * True if the progress is disabled.
     */
    disabled: {
      type: Boolean,
      value: false,
      reflectToAttribute: true,
      observer: '_disabledChanged'
    }
  },

  observers:
      ['_progressChanged(secondaryProgress, value, min, max, indeterminate)'],

  hostAttributes: {role: 'progressbar'},

  _toggleIndeterminate: function(indeterminate) {
    // If we use attribute/class binding, the animation sometimes doesn't
    // translate properly on Safari 7.1. So instead, we toggle the class here in
    // the update method.
    this.toggleClass('indeterminate', indeterminate, this.$.primaryProgress);
  },

  _transformProgress: function(progress, ratio) {
    var transform = 'scaleX(' + (ratio / 100) + ')';
    progress.style.transform = progress.style.webkitTransform = transform;
  },

  _mainRatioChanged: function(ratio) {
    this._transformProgress(this.$.primaryProgress, ratio);
  },

  _progressChanged: function(
      secondaryProgress, value, min, max, indeterminate) {
    secondaryProgress = this._clampValue(secondaryProgress);
    value = this._clampValue(value);

    var secondaryRatio = this._calcRatio(secondaryProgress) * 100;
    var mainRatio = this._calcRatio(value) * 100;

    this._setSecondaryRatio(secondaryRatio);
    this._transformProgress(this.$.secondaryProgress, secondaryRatio);
    this._transformProgress(this.$.primaryProgress, mainRatio);

    this.secondaryProgress = secondaryProgress;

    if (indeterminate) {
      this.removeAttribute('aria-valuenow');
    } else {
      this.setAttribute('aria-valuenow', value);
    }
    this.setAttribute('aria-valuemin', min);
    this.setAttribute('aria-valuemax', max);
  },

  _disabledChanged: function(disabled) {
    this.setAttribute('aria-disabled', disabled ? 'true' : 'false');
  },

  _hideSecondaryProgress: function(secondaryRatio) {
    return secondaryRatio === 0;
  }
});
