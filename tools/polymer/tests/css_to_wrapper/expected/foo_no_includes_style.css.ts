import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import './other_vars.css.js';

const styleMod = document.createElement('dom-module');
styleMod.appendChild(html`
  <template>
    <style>
div {
  font-size: 2rem;
}
    </style>
  </template>
`.content);
styleMod.register('foo-no-includes-style');