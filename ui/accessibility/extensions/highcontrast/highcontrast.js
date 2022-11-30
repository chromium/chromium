// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class HighContrast {
  constructor() {
    /** @type {string} */
    this.mode;
    /** @type {boolean} */
    this.enabled = false;
    /** @type {string} */
    this.scheme = '';

    this.init_();
  }

  /** @const {!Object<string, string>} */
  static filterMap = {
    '0': 'url("#hc_extension_off")',
    '1': 'url("#hc_extension_highcontrast")',
    '2': 'url("#hc_extension_grayscale")',
    '3': 'url("#hc_extension_invert")',
    '4': 'url("#hc_extension_invert_grayscale")',
    '5': 'url("#hc_extension_yellow_on_black")'
  };

  /** @const {string} */
  static STYLE_ID = 'hc_style';
  /** @const {string} */
  static BACKGROUND_ID = 'hc_extension_bkgnd';
  /** @const {string} */
  static FILTER_ID = 'hc_extension_svg_filters';
  /** @const {string} */
  static FULL_MODE_ATTRIBUTE = 'hc';
  /** @const {string} */
  static SCHEME_ATTRIBUTE = 'hcx';

  /**
   * Add the elements to the pgae that make high-contrast adjustments possible.
   */
  addOrUpdateExtraElements() {
    if (!this.enabled)
      return;

    // We used to include the CSS, but that doesn't work when the document
    // uses the <base> element to set a relative url. So instead we
    // add a <style> element directly to the document with the right
    // urls hard-coded into it.
    let style = document.getElementById(HighContrast.STYLE_ID);
    if (!style) {
      const baseUrl = window.location.href.replace(window.location.hash, '');
      const css = HighContrast.cssTemplate.replace(/#/g, baseUrl + '#');
      style = document.createElement('style');
      style.id = HighContrast.STYLE_ID;
      style.setAttribute('type', 'text/css');
      style.innerHTML = css;
      document.head.appendChild(style);
    }

    // Starting in Chrome 45 we can't apply a filter to the html element,
    // so instead we create an element with low z-index that copies the
    // body's background.
    let bg = document.getElementById(HighContrast.BACKGROUND_ID);
    if (!bg) {
      bg = document.createElement('div');
      bg.id = HighContrast.BACKGROUND_ID;
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
    let color = bg.style.backgroundColor;
    color = color.replace(/\s\s*/g, '');
    let parts =
        /^rgba\(([\d]+),([\d]+),([\d]+),([\d]+|[\d]*.[\d]+)\)/.exec(color);
    if (parts && parts[4] == '0') {
      bg.style.backgroundColor = '#fff';
    }

    // Add a hidden element with the SVG filters.
    let wrap = document.getElementById(HighContrast.FILTER_ID);
    if (wrap)
      return;

    wrap = document.createElement('span');
    wrap.id = HighContrast.FILTER_ID;
    wrap.setAttribute('hidden', '');
    wrap.innerHTML = HighContrast.svgContent;
    document.body.appendChild(wrap);
  }

  /**
   * This is called on load and every time the mode might have changed
   * (i.e. enabling/disabling, or changing the type of contrast adjustment
   * for this page).
   */
  update() {
    const html = document.documentElement;
    if (this.enabled) {
      if (!document.body) {
        // TODO: listen for appropriate ready event.
        window.setTimeout(this.update.bind(this), 100);
        return;
      }
      this.addOrUpdateExtraElements();
      if (html.getAttribute(HighContrast.FULL_MODE_ATTRIBUTE) !=
          this.mode + this.scheme) {
        html.setAttribute(
            HighContrast.FULL_MODE_ATTRIBUTE, this.mode + this.scheme);
      }
      if (html.getAttribute(HighContrast.SCHEME_ATTRIBUTE) != this.scheme) {
        html.setAttribute(HighContrast.SCHEME_ATTRIBUTE, this.scheme);
      }

      if (window == window.top) {
        window.scrollBy(0, 1);
        window.scrollBy(0, -1);
      }
    } else {
      html.setAttribute(HighContrast.FULL_MODE_ATTRIBUTE, this.mode + '0');
      html.setAttribute(HighContrast.SCHEME_ATTRIBUTE, '0');
      window.setTimeout(() => {
        html.removeAttribute(HighContrast.FULL_MODE_ATTRIBUTE);
        html.removeAttribute(HighContrast.SCHEME_ATTRIBUTE);
        const bg = document.getElementById('hc_extension_bkgnd');
        if (bg)
          bg.style.display = 'none';
      }, 0);
    }
  }

  /**
   * Called when we get a message from the background page.
   */
  onExtensionMessage(request) {
    if (this.enabled != request.enabled || this.scheme != request.scheme) {
      this.enabled = request.enabled;
      this.scheme = request.scheme;
      this.update();
    }
  }

  /**
   * KeyDown event handler
   */
  onKeyDown(evt) {
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

  /** @private */
  init_() {
    if (window == window.top) {
      this.mode = 'a';
    } else {
      this.mode = 'b';
    }
    chrome.extension.onRequest.addListener(this.onExtensionMessage.bind(this));
    chrome.extension.sendRequest(
        {'init': true}, this.onExtensionMessage.bind(this));
    document.addEventListener('keydown', this.onKeyDown.bind(this), false);

    // Update again after a few seconds and again after load so that
    // the background isn't wrong for long.
    window.setTimeout(this.addOrUpdateExtraElements.bind(this), 2000);
    window.addEventListener('load', () => {
      this.addOrUpdateExtraElements();

      // Also update when the document body attributes change.
      const config = {attributes: true, childList: false, characterData: false};
      const observer = new MutationObserver((mutations) => {
        this.addOrUpdateExtraElements();
      });
      observer.observe(document.body, config);
    });
  }

  /** @const {string} */
  static svgContent = `
      <svg xmlns="http://www.w3.org/2000/svg" version="1.1">
        <defs>
          <filter x="0" y="0" width="99999" height="99999"
              id="hc_extension_off">
            <feComponentTransfer>
              <feFuncR type="table" tableValues="0 1"/>
              <feFuncG type="table" tableValues="0 1"/>
              <feFuncB type="table" tableValues="0 1"/>
            </feComponentTransfer>
          </filter>
          <filter x="0" y="0" width="99999" height="99999"
              id="hc_extension_highcontrast">
            <feComponentTransfer>
              <feFuncR type="gamma" exponent="3.0"/>
              <feFuncG type="gamma" exponent="3.0"/>
              <feFuncB type="gamma" exponent="3.0"/>
            </feComponentTransfer>
          </filter>
          <filter x="0" y="0" width="99999" height="99999"
              id="hc_extension_highcontrast_back">
            <feComponentTransfer>
              <feFuncR type="gamma" exponent="0.33"/>
              <feFuncG type="gamma" exponent="0.33"/>
              <feFuncB type="gamma" exponent="0.33"/>
            </feComponentTransfer>
          </filter>
          <filter x="0" y="0" width="99999" height="99999"
              id="hc_extension_grayscale">
            <feColorMatrix type="matrix"
                values="0.2126 0.7152 0.0722 0 0 0.2126 0.7152 0.0722 0 0 0.2126 0.7152 0.0722 0 0 0 0 0 1 0"/>
            <feComponentTransfer>
              <feFuncR type="gamma" exponent="3"/>
              <feFuncG type="gamma" exponent="3"/>
              <feFuncB type="gamma" exponent="3"/>
            </feComponentTransfer>
          </filter>
          <filter x="0" y="0" width="99999" height="99999"
              id="hc_extension_grayscale_back">
            <feComponentTransfer>
              <feFuncR type="gamma" exponent="0.33"/>
              <feFuncG type="gamma" exponent="0.33"/>
              <feFuncB type="gamma" exponent="0.33"/>
            </feComponentTransfer>
          </filter>
          <filter x="0" y="0" width="99999" height="99999"
              id="hc_extension_invert">
            <feComponentTransfer>
              <feFuncR type="gamma" amplitude="-1" exponent="3" offset="1"/>
              <feFuncG type="gamma" amplitude="-1" exponent="3" offset="1"/>
              <feFuncB type="gamma" amplitude="-1" exponent="3" offset="1"/>
            </feComponentTransfer>
          </filter>
          <filter x="0" y="0" width="99999" height="99999"
              id="hc_extension_invert_back">
            <feComponentTransfer>
              <feFuncR type="table" tableValues="1 0"/>
              <feFuncG type="table" tableValues="1 0"/>
              <feFuncB type="table" tableValues="1 0"/>
            </feComponentTransfer>
            <feComponentTransfer>
              <feFuncR type="gamma" exponent="1.7"/>
              <feFuncG type="gamma" exponent="1.7"/>
              <feFuncB type="gamma" exponent="1.7"/>
            </feComponentTransfer>
          </filter>
          <filter x="0" y="0" width="99999" height="99999"
              id="hc_extension_invert_grayscale">
            <feColorMatrix type="matrix"
                values="0.2126 0.7152 0.0722 0 0 0.2126 0.7152 0.0722 0 0 0.2126 0.7152 0.0722 0 0 0 0 0 1 0"/>
            <feComponentTransfer>
              <feFuncR type="gamma" amplitude="-1" exponent="3" offset="1"/>
              <feFuncG type="gamma" amplitude="-1" exponent="3" offset="1"/>
              <feFuncB type="gamma" amplitude="-1" exponent="3" offset="1"/>
            </feComponentTransfer>
          </filter>
          <filter x="0" y="0" width="99999" height="99999"
              id="hc_extension_yellow_on_black">
            <feComponentTransfer>
              <feFuncR type="gamma" amplitude="-1" exponent="3" offset="1"/>
              <feFuncG type="gamma" amplitude="-1" exponent="3" offset="1"/>
              <feFuncB type="gamma" amplitude="-1" exponent="3" offset="1"/>
            </feComponentTransfer>
            <feColorMatrix type="matrix"
                values="0.3 0.5 0.2 0 0 0.3 0.5 0.2 0 0 0 0 0 0 0 0 0 0 1 0"/>
          </filter>
          <filter x="0" y="0" width="99999" height="99999"
              id="hc_extension_yellow_on_black_back">
            <feComponentTransfer>
              <feFuncR type="table" tableValues="1 0"/>
              <feFuncG type="table" tableValues="1 0"/>
              <feFuncB type="table" tableValues="1 0"/>
            </feComponentTransfer>
            <feComponentTransfer>
              <feFuncR type="gamma" exponent="0.33"/>
              <feFuncG type="gamma" exponent="0.33"/>
              <feFuncB type="gamma" exponent="0.33"/>
            </feComponentTransfer>
          </filter>
        </defs>
      </svg>
      `;

  /** @const {string} */
  static cssTemplate = `
      html[hc="a0"] {
        -webkit-filter: url("#hc_extension_off");
      }

      html[hcx="0"] img[src*="jpg"],
      html[hcx="0"] img[src*="jpeg"],
      html[hcx="0"] svg image,
      html[hcx="0"] img.rg_i,
      html[hcx="0"] embed,
      html[hcx="0"] object,
      html[hcx="0"] video {
        -webkit-filter: url("#hc_extension_off");
      }

      html[hc="a1"] {
        -webkit-filter: url("#hc_extension_highcontrast");
      }

      html[hcx="1"] img[src*="jpg"],
      html[hcx="1"] img[src*="jpeg"],
      html[hcx="1"] img.rg_i,
      html[hcx="1"] svg image,
      html[hcx="1"] embed,
      html[hcx="1"] object,
      html[hcx="1"] video {
        -webkit-filter: url("#hc_extension_highcontrast_back");
      }

      html[hc="a2"] {
        -webkit-filter: url("#hc_extension_grayscale");
      }

      html[hcx="2"] img[src*="jpg"],
      html[hcx="2"] img[src*="jpeg"],
      html[hcx="2"] img.rg_i,
      html[hcx="2"] svg image,
      html[hcx="2"] embed,
      html[hcx="2"] object,
      html[hcx="2"] video {
        -webkit-filter: url("#hc_extension_grayscale_back");
      }

      html[hc="a3"] {
        -webkit-filter: url("#hc_extension_invert");
      }

      html[hcx="3"] img[src*="jpg"],
      html[hcx="3"] img[src*="jpeg"],
      html[hcx="3"] img.rg_i,
      html[hcx="3"] svg image,
      html[hcx="3"] embed,
      html[hcx="3"] object,
      html[hcx="3"] video {
        -webkit-filter: url("#hc_extension_invert_back");
      }

      html[hc="a4"] {
        -webkit-filter: url("#hc_extension_invert_grayscale");
      }

      html[hcx="4"] img[src*="jpg"],
      html[hcx="4"] img[src*="jpeg"],
      html[hcx="4"] img.rg_i,
      html[hcx="4"] svg image,
      html[hcx="4"] embed,
      html[hcx="4"] object,
      html[hcx="4"] video {
        -webkit-filter: url("#hc_extension_invert_back");
      }

      html[hc="a5"] {
        -webkit-filter: url("#hc_extension_yellow_on_black");
      }

      html[hcx="5"] img[src*="jpg"],
      html[hcx="5"] img[src*="jpeg"],
      html[hcx="5"] img.rg_i,
      html[hcx="5"] svg image,
      html[hcx="5"] embed,
      html[hcx="5"] object,
      html[hcx="5"] video {
        -webkit-filter: url("#hc_extension_yellow_on_black_back");
      }`;
}

const highContrast = new HighContrast();
