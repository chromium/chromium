import {Polymer, html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {PaperRippleBehavior} from 'chrome://resources/polymer/v3_0/paper-behaviors/paper-ripple-behavior.js';
import '../shared_vars_css.m.js';
import './foo.m.js';

  let instance_ = null;

  let bar_ = 1;

  export const someExport = true;

  export function getInstance() {
    return assert(instance_);
  }

  function getBarInternal_() {
    return bar_;
  }

  export function getBar(isTest) {
    return isTest ? 0 : getBarInternal_();
  }

  export let CrTestFooElement = Polymer({
  _template: html`
    <style>
      div {
        font-size: 2rem;
      }
    </style>
    <div>Hello world</div>
`,
    is: 'cr-test-foo',
    behaviors: [PaperRippleBehavior],
  });

