import {html} from '//resources/lit/v3_0/lit.rollup.js';
import type {FooLitElement} from './foo_lit.js';

export function getHtml(this: FooLitElement) {
  return html`<!--_html_template_start_--><div>Hello world</div>
<!--_html_template_end_-->`;
}