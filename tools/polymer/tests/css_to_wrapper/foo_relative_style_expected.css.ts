import {html} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import './other1.css.js';
import './other2.css.js';

const styleMod = document.createElement('dom-module');
styleMod.appendChild(html`
  <template>
    <style include="other1 other2">
div {
  font-size: 2rem;
  --foo-bar: calc(var(--foo-bar1)
      - var(--foo-bar2)
      - 3 * var(--foo-bar3));
}
    </style>
  </template>
`.content);
styleMod.register('foo-relative-style');