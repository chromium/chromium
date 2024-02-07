import {css} from 'chrome://resources/lit/v3_0/lit.rollup.js';


export function getCss() {
  return css`div{font-size:2rem;--foo-bar:calc(var(--foo-bar1)
      - var(--foo-bar2)
      - 3 * var(--foo-bar3))}`;
}