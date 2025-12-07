import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import './other1.css.js';
import './other2.css.js';

const styleMod = document.createElement('dom-module');
styleMod.appendChild(html`
  <template>
    <style include="other1 other2">
div{font-size:2rem;--foo-bar:calc(var(--foo-bar1) - var(--foo-bar2) - 3 * var(--foo-bar3));--foo-bar2:$i18nRaw{someStringId};& span.foo{font-size:1rem}span.bar{font-size:1.5rem}}@container scroll-state(scrollable:top){span.baz{display:block}}
    </style>
  </template>
`.content);
styleMod.register('css-features');