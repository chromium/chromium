import 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import './other1.css.js';
import './other2.css.js';

const $_documentContainer = document.createElement('template');
$_documentContainer.innerHTML = `
<custom-style>
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
</custom-style>
`;
document.head.appendChild($_documentContainer.content);