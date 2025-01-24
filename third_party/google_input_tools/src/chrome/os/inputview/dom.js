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
goog.require('goog.uri.utils');
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
 * Check the iframe whether is the same domain as the current domain.
 * Returns the iframe content document when it's the same domain,
 * otherwise return null.
 *
 * @param {!Element} element The iframe element.
 * @return {Document} The iframe content document.
 */
i18n.input.common.dom.getSameDomainFrameDoc = function(element) {
  var uid = goog.getUid(document);
  var frameUid = goog.getUid(element);
  var states = i18n.input.common.dom.sameDomainIframes_[uid];
  if (!states) {
    states = i18n.input.common.dom.sameDomainIframes_[uid] = {};
  }
  /** @preserveTry */
  try {
    var url = window.location.href || '';
    //Note: cross-domain IFRAME's src can be:
    //     http://www...
    //     https://www....
    //     //www.
    // Non-cross-domain IFRAME's src can be:
    //     javascript:...
    //     javascript://...
    //     abc:...
    //     abc://...
    //     abc//...
    //     path/index.html
    if (!(frameUid in states)) {
      if (element.src) {
        var pos = element.src.indexOf('//');
        var protocol = pos < 0 ? 'N/A' : element.src.slice(0, pos);
        states[frameUid] = (protocol != '' &&
            protocol != 'http:' &&
            protocol != 'https:' ||
            goog.uri.utils.haveSameDomain(element.src, url));
      } else {
        states[frameUid] = true;
      }
    }
    return states[frameUid] ? goog.dom.getFrameContentDocument(element) : null;
  } catch (e) {
    states[frameUid] = false;
    return null;
  }
};


/**
 * Gets the same domain iframe or frame document in given document, default
 * given document is current document.
 *
 * @param {Document=} opt_doc The given document.
 * @return {Array.<!Document>} The same domain iframe document.
 */
i18n.input.common.dom.getSameDomainDocuments = function(opt_doc) {
  var doc = opt_doc || document;
  var iframes = [];
  var rets = [];
  goog.array.extend(iframes,
      doc.getElementsByTagName(goog.dom.TagName.IFRAME),
      doc.getElementsByTagName(goog.dom.TagName.FRAME));
  goog.array.forEach(iframes, function(frame) {
    var frameDoc = i18n.input.common.dom.getSameDomainFrameDoc(frame);
    frameDoc && rets.push(frameDoc);
  });
  return rets;
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
