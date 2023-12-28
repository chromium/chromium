import {getTrustedHTML} from '//resources/js/static_types.js';
export function getTemplate() {
  return getTrustedHTML`<!--_html_template_start_--><style>
  div {
    font-size: 2rem;
  }
</style>
<div>Hello world</div>
<!--_html_template_end_-->`;
}