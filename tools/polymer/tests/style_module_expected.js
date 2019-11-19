import {Polymer, html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import './some_other_style.m.js';
const template = document.createElement('template');
template.innerHTML = `
<dom-module id="cr-foo-style" assetpath="chrome://resources/">
  <template>
    <style include="some-other-style">
      :host {
        margin: 0;
      }
    </style>
  </template>
</dom-module>
`;
document.body.appendChild(template.content.cloneNode(true));