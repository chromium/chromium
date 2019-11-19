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

import {IronA11yKeysBehavior} from '../iron-a11y-keys-behavior/iron-a11y-keys-behavior.js';
import {Polymer} from '../polymer/polymer_bundled.min.js';
/**
`iron-a11y-keys` provides a cross-browser interface for processing
keyboard commands. The interface adheres to [WAI-ARIA best
practices](http://www.w3.org/TR/wai-aria-practices/#kbd_general_binding).
It uses an expressive syntax to filter key presses.

## Basic usage

The sample code below is a portion of a custom element. The goal is to call
the `onEnter` method whenever the `paper-input` element is in focus and
the `Enter` key is pressed.

    <iron-a11y-keys id="a11y" target="[[target]]" keys="enter"
                        on-keys-pressed="onEnter"></iron-a11y-keys>
    <paper-input id="input"
                 placeholder="Type something. Press enter. Check console."
                 value="{{userInput::input}}"></paper-input>

The custom element declares an `iron-a11y-keys` element that is bound to a
property called `target`. The `target` property
needs to evaluate to the `paper-input` node. `iron-a11y-keys` registers
an event handler for the target node using Polymer's [annotated event handler
syntax](https://www.polymer-project.org/1.0/docs/devguide/events.html#annotated-listeners).
`{{userInput::input}}` sets the `userInput` property to the user's input on each
keystroke.

The last step is to link the two elements within the custom element's
registration.

    ...
    properties: {
      userInput: {
        type: String,
        notify: true,
      },
      target: {
        type: Object,
        value: function() {
          return this.$.input;
        }
      },
    },
    onEnter: function() {
      console.log(this.userInput);
    }
    ...

## The `keys` attribute

The `keys` attribute expresses what combination of keys triggers the event.

The attribute accepts a space-separated, plus-sign-concatenated
set of modifier keys and some common keyboard keys.

The common keys are: `a-z`, `0-9` (top row and number pad), `*` (shift 8 and
number pad), `F1-F12`, `Page Up`, `Page Down`, `Left Arrow`, `Right Arrow`,
`Down Arrow`, `Up Arrow`, `Home`, `End`, `Escape`, `Space`, `Tab`, `Enter`.

The modifier keys are: `Shift`, `Control`, `Alt`, `Meta`.

All keys are expected to be lowercase and shortened. E.g.
`Left Arrow` is `left`, `Page Down` is `pagedown`, `Control` is `ctrl`,
`F1` is `f1`, `Escape` is `esc`, etc.

### Grammar

Below is the
[EBNF](http://en.wikipedia.org/wiki/Extended_Backus%E2%80%93Naur_Form) Grammar
of the `keys` attribute.

    modifier = "shift" | "ctrl" | "alt" | "meta";
    ascii = ? /[a-z0-9]/ ? ;
    fnkey = ? f1 through f12 ? ;
    arrow = "up" | "down" | "left" | "right" ;
    key = "tab" | "esc" | "space" | "*" | "pageup" | "pagedown" |
          "home" | "end" | arrow | ascii | fnkey;
    event = "keypress" | "keydown" | "keyup";
    keycombo = { modifier, "+" }, key, [ ":", event ] ;
    keys = keycombo, { " ", keycombo } ;

### Example

Given the following value for `keys`:

`ctrl+shift+f7 up pagedown esc space alt+m`

The event is fired if any of the following key combinations are fired:
`Control` and `Shift` and `F7` keys, `Up Arrow` key, `Page Down` key,
`Escape` key, `Space` key, `Alt` and `M` keys.

### WAI-ARIA Slider Example

The following is an example of the set of keys that fulfills WAI-ARIA's
"slider" role [best
practices](http://www.w3.org/TR/wai-aria-practices/#slider):

    <iron-a11y-keys target="[[target]]" keys="left pagedown down"
                    on-keys-pressed="decrement"></iron-a11y-keys>
    <iron-a11y-keys target="[[target]]" keys="right pageup up"
                    on-keys-pressed="increment"></iron-a11y-keys>
    <iron-a11y-keys target="[[target]]" keys="home"
                    on-keys-pressed="setMin"></iron-a11y-keys>
    <iron-a11y-keys target="[[target]]" keys="end"
                    on-keys-pressed="setMax"></iron-a11y-keys>

The `target` properties must evaluate to a node. See the basic usage
example above.

Each of the values for the `on-keys-pressed` attributes must evalute
to methods. The `increment` method should move the slider a set amount
toward the maximum value. `decrement` should move the slider a set amount
toward the minimum value. `setMin` should move the slider to the minimum
value. `setMax` should move the slider to the maximum value.

@demo demo/index.html
*/



Polymer({
  is: 'iron-a11y-keys',

  behaviors: [IronA11yKeysBehavior],

  properties: {
    /** @type {?Node} */
    target: {type: Object, observer: '_targetChanged'},

    /**
     * Space delimited list of keys where each key follows the format:
     * `[MODIFIER+]*KEY[:EVENT]`.
     * e.g. `keys="space ctrl+shift+tab enter:keyup"`.
     * More detail can be found in the "Grammar" section of the documentation
     */
    keys: {type: String, reflectToAttribute: true, observer: '_keysChanged'}
  },

  attached: function() {
    if (!this.target) {
      this.target = this.parentNode;
    }
  },

  _targetChanged: function(target) {
    this.keyEventTarget = target;
  },

  _keysChanged: function() {
    this.removeOwnKeyBindings();
    this.addOwnKeyBinding(this.keys, '_fireKeysPressed');
  },

  _fireKeysPressed: function(event) {
    this.fire('keys-pressed', event.detail, {});
  }
});
