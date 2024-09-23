import {css, CSSResultGroup} from 'chrome://resources/lit/v3_0/lit.rollup.js';
import {getCss as getOther1} from './other1.css.js';
import {getCss as getOther2} from './other2.css.js';
import './other_vars.css.js';

let instance: CSSResultGroup|null = null;
export function getCss() {
  return instance || (instance = [...[getOther1(),getOther2()], css`div {
  color: blue;
}`]);
}