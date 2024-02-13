import {css} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import './other1.css.js';
import './other2.css.js';

const result = css`
html {
  --my-var: 9px;
}

@media (prefers-color-scheme: dark) {
  html {
    --my-var: 10px;
  }
}`;
document.adoptedStyleSheets = [...document.adoptedStyleSheets!, result.styleSheet!];