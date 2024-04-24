import './other1.css.js';
import './other2.css.js';
export {};

const sheet = new CSSStyleSheet();
sheet.replaceSync(`
html {
  --my-var: 9px;
}

@media (prefers-color-scheme: dark) {
  html {
    --my-var: 10px;
  }
}`);
document.adoptedStyleSheets = [...document.adoptedStyleSheets, sheet];