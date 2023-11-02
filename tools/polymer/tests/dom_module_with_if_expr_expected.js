// <if expr="chromeos_ash">
import '../shared_vars_chromeos_css.m.js';
// </if>
import {Polymer, html} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {PaperRippleBehavior} from '//resources/polymer/v3_0/paper-behaviors/paper-ripple-behavior.js';
import '../shared_vars_css.m.js';
import './foo.m.js';
// <if expr="chromeos_ash">
import './bar.m.js';
// </if>

Polymer({
  _template: html`<!--_html_template_start_-->
    <style>
      div {
        font-size: 2rem;
      }
    </style>
    <div>Hello world</div>
<!--_html_template_end_-->`,
  is: 'cr-test-foo',
  behaviors: [PaperRippleBehavior],
});
