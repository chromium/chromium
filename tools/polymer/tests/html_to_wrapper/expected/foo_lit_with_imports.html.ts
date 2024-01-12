import {html} from '//resources/lit/v3_0/lit.rollup.js';
import type {FooLitWithImportsElement} from './foo_lit_with_imports.js';
import {nothing} from '//resources/lit/v3_0/lit.rollup.js';

export function getHtml(this: FooLitWithImportsElement) {
  return html`<!--_html_template_start_--><div aria-label="${this.foo || nothing}">Hello world</div><!--_html_template_end_-->`;
}