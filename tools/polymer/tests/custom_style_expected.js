import {Polymer, html} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import './foo.m.js';
const $_documentContainer = document.createElement('template');
$_documentContainer.innerHTML = `
<custom-style>
  <style>
    html {
      --foo-bar: 2rem;
    }
  </style>
</custom-style>
`;
document.head.appendChild($_documentContainer.content);