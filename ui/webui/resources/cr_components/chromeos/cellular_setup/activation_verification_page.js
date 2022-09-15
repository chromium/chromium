// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * This page is displayed when the activation code is being verified, and
 * an ESim profile is being installed.
 */
import './base_page.js';
import '//resources/polymer/v3_0/paper-spinner/paper-spinner-lite.js';
import '//resources/polymer/v3_0/iron-media-query/iron-media-query.js';
import '//resources/cr_elements/cr_lottie/cr_lottie.js';

import {html, Polymer} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {I18nBehavior} from '../../../cr_elements/i18n_behavior.js';

Polymer({
  _template: html`{__html_template__}`,
  is: 'activation-verification-page',

  behaviors: [I18nBehavior],

  properties: {
    /**
     * @type {boolean}
     * @private
     */
    isDarkModeActive_: {
      type: Boolean,
      value: false,
    },
  },

  /**
   * @return {string}
   * @private
   */
  getAnimationUrl_() {
    return this.isDarkModeActive_ ? 'spinner_dark.json' : 'spinner.json';
  },
});
