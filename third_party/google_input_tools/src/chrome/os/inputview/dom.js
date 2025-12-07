// Copyright 2014 The Cloud Input Tools Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * @fileoverview Provides common operation of dom for input tools.
 */


goog.provide('i18n.input.common.dom');

goog.require('goog.array');
goog.require('goog.dom');
goog.require('goog.dom.TagName');
goog.require('goog.dom.classlist');
goog.require('goog.style');
goog.require('i18n.input.common.GlobalSettings');


/**
 * When detects whether the same domain iframe, browser will throw
 * exceptions on accessing the cross domain iframe. Stores result to avoid to
 * throws exception twice.
 * Key is document uid, value is object.  ifrmae uid : true/false
 *
 * @type {!Object.<number, !Object.<number, boolean>>}
 * @private
 */
i18n.input.common.dom.sameDomainIframes_ = {};


/**
 * Sets class names to an element.
 *
 * @param {Element} elem Element to set class names.
 * @param {Array.<string>} classes Class names.
 */
i18n.input.common.dom.setClasses = function(elem, classes) {
  if (elem) {
    for (var i = 0; i < classes.length; i++) {
      if (i == 0) {
        goog.dom.classlist.set(elem, classes[0]);
      } else {
        goog.dom.classlist.add(elem, classes[i]);
      }
    }
  }
};


/**
 * Create the iframe in given document or default document. Then the input tool
 * UI element will be create inside the iframe document to avoid CSS conflict.
 *
 * @param {Document=} opt_doc The given document.
 * @return {!Element} The iframe element.
 */
i18n.input.common.dom.createIframeWrapper = function(opt_doc) {
  var doc = opt_doc || document;
  var dom = goog.dom.getDomHelper();
  var frame = dom.createDom(goog.dom.TagName.IFRAME, {
    'frameborder': '0',
    'scrolling': 'no',
    'style': 'background-color:transparent;border:0;display:none;'
  });
  dom.append(/** @type {!Element} */ (doc.body), frame);
  var frameDoc = dom.getFrameContentDocument(frame);

  var css = i18n.input.common.GlobalSettings.alternativeImageUrl ?
      i18n.input.common.GlobalSettings.css.replace(
      /^(?:https?:)?\/\/ssl.gstatic.com\/inputtools\/images/g,
      i18n.input.common.GlobalSettings.alternativeImageUrl) :
      i18n.input.common.GlobalSettings.css;
  goog.style.installStyles(
      'html body{border:0;margin:0;padding:0} html,body{overflow:hidden}' +
      css, /** @type {!Element} */(frameDoc.body));
  return frame;
};


/**
 * The property need to be copied from original element to its iframe wrapper.
 *
 * @type {!Array.<string>}
 * @private
 */
i18n.input.common.dom.iframeWrapperProperty_ = ['box-shadow', 'z-index',
  'margin', 'position', 'display'];


/**
 * Copies the necessary properties value from original element to its iframe
 * wrapper element.
 *
 * @param {Element} element .
 * @param {Element} iframe The iframe wrapper element.
 */
i18n.input.common.dom.copyNecessaryStyle = function(element, iframe) {
  goog.style.setContentBoxSize(iframe, goog.style.getSize(element));
  goog.array.forEach(i18n.input.common.dom.iframeWrapperProperty_,
      function(property) {
        goog.style.setStyle(iframe, property,
            goog.style.getComputedStyle(element, property));
      });
};
