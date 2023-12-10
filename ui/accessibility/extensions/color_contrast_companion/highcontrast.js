// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var mode;
var enabled = false;
var scheme = '';
var timeoutId = null;

var filterMap = {
  '0': 'url("#hc_extension_off")',
  '1': 'url("#hc_extension_highcontrast")',
  '2': 'url("#hc_extension_grayscale")',
  '3': 'url("#hc_extension_invert")',
  '4': 'url("#hc_extension_invert_grayscale")',
  '5': 'url("#hc_extension_yellow_on_black")'
};

var svgContent =
    '<svg xmlns="http://www.w3.org/2000/svg" version="1.1"><defs><filter x="0" y="0" width="99999" height="99999" id="hc_extension_off"><feComponentTransfer><feFuncR type="table" tableValues="0 1"/><feFuncG type="table" tableValues="0 1"/><feFuncB type="table" tableValues="0 1"/></feComponentTransfer></filter><filter x="0" y="0" width="99999" height="99999" id="hc_extension_highcontrast"><feComponentTransfer><feFuncR type="gamma" exponent="3.0"/><feFuncG type="gamma" exponent="3.0"/><feFuncB type="gamma" exponent="3.0"/></feComponentTransfer></filter><filter x="0" y="0" width="99999" height="99999" id="hc_extension_highcontrast_back"><feComponentTransfer><feFuncR type="gamma" exponent="0.33"/><feFuncG type="gamma" exponent="0.33"/><feFuncB type="gamma" exponent="0.33"/></feComponentTransfer></filter><filter x="0" y="0" width="99999" height="99999" id="hc_extension_grayscale"><feColorMatrix type="matrix" values="0.2126 0.7152 0.0722 0 0 0.2126 0.7152 0.0722 0 0 0.2126 0.7152 0.0722 0 0 0 0 0 1 0"/><feComponentTransfer><feFuncR type="gamma" exponent="3"/><feFuncG type="gamma" exponent="3"/><feFuncB type="gamma" exponent="3"/></feComponentTransfer></filter><filter x="0" y="0" width="99999" height="99999" id="hc_extension_grayscale_back"><feComponentTransfer><feFuncR type="gamma" exponent="0.33"/><feFuncG type="gamma" exponent="0.33"/><feFuncB type="gamma" exponent="0.33"/></feComponentTransfer></filter><filter x="0" y="0" width="99999" height="99999" id="hc_extension_invert"><feComponentTransfer><feFuncR type="gamma" amplitude="-1" exponent="3" offset="1"/><feFuncG type="gamma" amplitude="-1" exponent="3" offset="1"/><feFuncB type="gamma" amplitude="-1" exponent="3" offset="1"/></feComponentTransfer></filter><filter x="0" y="0" width="99999" height="99999" id="hc_extension_invert_back"><feComponentTransfer><feFuncR type="table" tableValues="1 0"/><feFuncG type="table" tableValues="1 0"/><feFuncB type="table" tableValues="1 0"/></feComponentTransfer><feComponentTransfer><feFuncR type="gamma" exponent="1.7"/><feFuncG type="gamma" exponent="1.7"/><feFuncB type="gamma" exponent="1.7"/></feComponentTransfer></filter><filter x="0" y="0" width="99999" height="99999" id="hc_extension_invert_grayscale"><feColorMatrix type="matrix" values="0.2126 0.7152 0.0722 0 0 0.2126 0.7152 0.0722 0 0 0.2126 0.7152 0.0722 0 0 0 0 0 1 0"/><feComponentTransfer><feFuncR type="gamma" amplitude="-1" exponent="3" offset="1"/><feFuncG type="gamma" amplitude="-1" exponent="3" offset="1"/><feFuncB type="gamma" amplitude="-1" exponent="3" offset="1"/></feComponentTransfer></filter><filter x="0" y="0" width="99999" height="99999" id="hc_extension_yellow_on_black"><feComponentTransfer><feFuncR type="gamma" amplitude="-1" exponent="3" offset="1"/><feFuncG type="gamma" amplitude="-1" exponent="3" offset="1"/><feFuncB type="gamma" amplitude="-1" exponent="3" offset="1"/></feComponentTransfer><feColorMatrix type="matrix" values="0.3 0.5 0.2 0 0 0.3 0.5 0.2 0 0 0 0 0 0 0 0 0 0 1 0"/></filter><filter x="0" y="0" width="99999" height="99999" id="hc_extension_yellow_on_black_back"><feComponentTransfer><feFuncR type="table" tableValues="1 0"/><feFuncG type="table" tableValues="1 0"/><feFuncB type="table" tableValues="1 0"/></feComponentTransfer><feComponentTransfer><feFuncR type="gamma" exponent="0.33"/><feFuncG type="gamma" exponent="0.33"/><feFuncB type="gamma" exponent="0.33"/></feComponentTransfer></filter></defs></svg>';

var cssTemplate =
    'html[hc="a0"] {   filter: url("#hc_extension_off"); } html[hcx="0"] img[src*="jpg"], html[hcx="0"] img[src*="jpeg"], html[hcx="0"] svg image, html[hcx="0"] img.rg_i, html[hcx="0"] embed, html[hcx="0"] object, html[hcx="0"] video {   filter: url("#hc_extension_off"); }  html[hc="a1"] {   filter: url("#hc_extension_highcontrast"); } html[hcx="1"] img[src*="jpg"], html[hcx="1"] img[src*="jpeg"], html[hcx="1"] img.rg_i, html[hcx="1"] svg image, html[hcx="1"] embed, html[hcx="1"] object, html[hcx="1"] video {   filter: url("#hc_extension_highcontrast_back"); }  html[hc="a2"] {   filter: url("#hc_extension_grayscale"); } html[hcx="2"] img[src*="jpg"], html[hcx="2"] img[src*="jpeg"], html[hcx="2"] img.rg_i, html[hcx="2"] svg image, html[hcx="2"] embed, html[hcx="2"] object, html[hcx="2"] video {   filter: url("#hc_extension_grayscale_back"); }  html[hc="a3"] {   filter: url("#hc_extension_invert"); } html[hcx="3"] img[src*="jpg"], html[hcx="3"] img[src*="jpeg"], html[hcx="3"] img.rg_i, html[hcx="3"] svg image, html[hcx="3"] embed, html[hcx="3"] object, html[hcx="3"] video {   filter: url("#hc_extension_invert_back"); }  html[hc="a4"] {   filter: url("#hc_extension_invert_grayscale"); } html[hcx="4"] img[src*="jpg"], html[hcx="4"] img[src*="jpeg"], html[hcx="4"] img.rg_i, html[hcx="4"] svg image, html[hcx="4"] embed, html[hcx="4"] object, html[hcx="4"] video {   filter: url("#hc_extension_invert_back"); }  html[hc="a5"] {   filter: url("#hc_extension_yellow_on_black"); } html[hcx="5"] img[src*="jpg"], html[hcx="5"] img[src*="jpeg"], html[hcx="5"] img.rg_i, html[hcx="5"] svg image, html[hcx="5"] embed, html[hcx="5"] object, html[hcx="5"] video {   filter: url("#hc_extension_yellow_on_black_back"); }';

/**
 * Add the elements to the pgae that make high-contrast adjustments possible.
 */
function addOrUpdateExtraElements() {
  if (!enabled)
    return;

  // We used to include the CSS, but that doesn't work when the document
  // uses the <base> element to set a relative url. So instead we
  // add a <style> element directly to the document with the right
  // urls hard-coded into it.
  var style = document.getElementById('hc_style');
  if (!style) {
    var baseUrl = window.location.href.replace(window.location.hash, '');
    var css = cssTemplate.replace(/#/g, baseUrl + '#');
    style = document.createElement('style');
    style.id = 'hc_style';
    style.setAttribute('type', 'text/css');
    style.innerHTML = css;
    document.head.appendChild(style);
  }

  // Starting in Chrome 45 we can't apply a filter to the html element,
  // so instead we create an element with low z-index that copies the
  // body's background.
  var bg = document.getElementById('hc_extension_bkgnd');
  if (!bg) {
    bg = document.createElement('div');
    bg.id = 'hc_extension_bkgnd';
    bg.style.position = 'fixed';
    bg.style.left = '0px';
    bg.style.top = '0px';
    bg.style.right = '0px';
    bg.style.bottom = '0px';
    bg.style.zIndex = -1999999999;
    document.body.appendChild(bg);
  }
  bg.style.display = 'block';
  bg.style.background = window.getComputedStyle(document.body).background;

  // As a special case, replace a zero-alpha background with white,
  // otherwise we can't invert it.
  var c = bg.style.backgroundColor;
  c = c.replace(/\s\s*/g, '');
  if (m = /^rgba\(([\d]+),([\d]+),([\d]+),([\d]+|[\d]*.[\d]+)\)/.exec(c)) {
    if (m[4] == '0') {
      bg.style.backgroundColor = '#fff';
    }
  }

  // Add a hidden element with the SVG filters.
  var wrap = document.getElementById('hc_extension_svg_filters');
  if (wrap)
    return;

  wrap = document.createElement('span');
  wrap.id = 'hc_extension_svg_filters';
  wrap.setAttribute('hidden', '');
  wrap.innerHTML = svgContent;
  document.body.appendChild(wrap);
}

/**
 * This is called on load and every time the mode might have changed
 * (i.e. enabling/disabling, or changing the type of contrast adjustment
 * for this page).
 */
function update() {
  var html = document.documentElement;
  if (enabled) {
    if (!document.body) {
      window.setTimeout(update, 100);
      return;
    }
    addOrUpdateExtraElements();
    if (html.getAttribute('hc') != mode + scheme)
      html.setAttribute('hc', mode + scheme);
    if (html.getAttribute('hcx') != scheme)
      html.setAttribute('hcx', scheme);

    if (window == window.top) {
      window.scrollBy(0, 1);
      window.scrollBy(0, -1);
    }
  } else {
    html.setAttribute('hc', mode + '0');
    html.setAttribute('hcx', '0');
    window.setTimeout(function() {
      html.removeAttribute('hc');
      html.removeAttribute('hcx');
      var bg = document.getElementById('hc_extension_bkgnd');
      if (bg)
        bg.style.display = 'none';
    }, 0);
  }
}

/**
 * Called when we get a message from the background page.
 */
function onExtensionMessage(request) {
  if (enabled != request.enabled || scheme != request.scheme) {
    enabled = request.enabled;
    scheme = request.scheme;
    update();
  }
}

/**
 * KeyDown event handler
 */
function onKeyDown(evt) {
  if (evt.keyCode == 122 /* F11 */ && evt.shiftKey) {
    chrome.extension.sendRequest({'toggle_global': true});
    evt.stopPropagation();
    evt.preventDefault();
    return false;
  }
  if (evt.keyCode == 123 /* F12 */ && evt.shiftKey) {
    chrome.extension.sendRequest({'toggle_site': true});
    evt.stopPropagation();
    evt.preventDefault();
    return false;
  }
  return true;
}

function init() {
  if (window == window.top) {
    mode = 'a';
  } else {
    mode = 'b';
  }
  chrome.extension.onRequest.addListener(onExtensionMessage);
  chrome.extension.sendRequest({'init': true}, onExtensionMessage);
  document.addEventListener('keydown', onKeyDown, false);

  // Update again after a few seconds and again after load so that
  // the background isn't wrong for long.
  window.setTimeout(addOrUpdateExtraElements, 2000);
  window.addEventListener('load', function() {
    addOrUpdateExtraElements();

    // Also update when the document body attributes change.
    var config = {attributes: true, childList: false, characterData: false};
    var observer = new MutationObserver(function(mutations) {
      addOrUpdateExtraElements();
    });
    observer.observe(document.body, config);
  });
}

init();
