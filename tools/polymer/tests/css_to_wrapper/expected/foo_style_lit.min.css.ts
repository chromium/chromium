import {css, CSSResultGroup} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {getCss as getOther1} from './other1.css.js';
import {getCss as getOther2} from './other2.css.js';

let instance: CSSResultGroup|null = null;
export function getCss() {
  return instance || (instance = [...[getOther1(),getOther2()], css`div{font-size:2rem;--foo-bar:calc(var(--foo-bar1)
      - var(--foo-bar2)
      - 3 * var(--foo-bar3))}`]);
}