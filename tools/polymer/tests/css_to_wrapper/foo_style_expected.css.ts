import 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import './other1.css.js';
import './other2.css.js';

const styleMod = document.createElement('dom-module');
styleMod.innerHTML = `
  <template>
    <style include="other1 other2">
div {
  font-size: 2rem;
}
    </style>
  </template>
`;
styleMod.register('foo-style');