import {html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import './other1.css.js';
import './other2.css.js';

const template = html`
<style>

html {
  --my-var: 9px;
}

@media (prefers-color-scheme: dark) {
  html {
    --my-var: 10px;
  }
}
</style>
`;
document.head.appendChild(template.content);