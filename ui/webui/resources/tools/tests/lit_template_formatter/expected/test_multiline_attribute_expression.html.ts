// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html} from '//resources/lit/v3_0/lit.rollup.js';

export function getHtml(this: DummyTestElement) {
  return html`<!--_html_template_start_-->
<div id="container">
  <cr-icon ?hidden="${!this.someCondition}"
      icon="${
          this.someOtherConditionProperty ? 'my-dummy-icons:hello' :
                                            'my-dummy-icons:world'}"
      alt="" title="hello world">
  </cr-icon>
  <button
      ?hidden="${
          this.someLongHiddenCondition && this.myCoolFeature &&
              !this.someOtherConditionThatIsLong}"
      @some-very-very-long-event-name="${
          this.onMyButtonWithExtraTextSomeVeryVeryLongEventName}">
    Click Me!
  </button>
  <select>
    <option value="${this.someConfigObject.someSelectedOptionValue}"
        ?selected="${
            this.isSomeOptionSelected_(
                this.someConfigObject.someSelectedOptionValue)}">
      Some Option
    </option>
  </select>
  <div>
    <div>
      <div>
        <div>
          <div>
            <div>
              <input .value="${this.someValue}"
                  @very-long-attribute-event-listener-name="${
                      this.onHandlerCallbackAction_.bind(
                          this, item.id, item.name)}">
            </div>
          </div>
        </div>
      </div>
    </div>
  </div>
</div>
<!--_html_template_end_-->`;
}
