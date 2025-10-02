// Copyright (c) 2025 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {$, DEFAULT_SCHEME, getEnabled, getSiteScheme, siteFromUrl,} from './common.js';

let mode: string = '';
let enabled: boolean = false;
let scheme: number = DEFAULT_SCHEME;

const svgContent: string =
    '<svg xmlns="http://www.w3.org/2000/svg" version="1.1"><defs><filter x="0" y="0" width="99999" height="99999" id="hc_extension_off"><feComponentTransfer><feFuncR type="table" tableValues="0 1"/><feFuncG type="table" tableValues="0 1"/><feFuncB type="table" tableValues="0 1"/></feComponentTransfer></filter><filter x="0" y="0" width="99999" height="99999" id="hc_extension_highcontrast"><feComponentTransfer><feFuncR type="gamma" exponent="3.0"/><feFuncG type="gamma" exponent="3.0"/><feFuncB type="gamma" exponent="3.0"/></feComponentTransfer></filter><filter x="0" y="0" width="99999" height="99999" id="hc_extension_highcontrast_back"><feComponentTransfer><feFuncR type="gamma" exponent="0.33"/><feFuncG type="gamma" exponent="0.33"/><feFuncB type="gamma" exponent="0.33"/></feComponentTransfer></filter><filter x="0" y="0" width="99999" height="99999" id="hc_extension_grayscale"><feColorMatrix type="matrix" values="0.2126 0.7152 0.0722 0 0 0.2126 0.7152 0.0722 0 0 0.2126 0.7152 0.0722 0 0 0 0 0 1 0"/><feComponentTransfer><feFuncR type="gamma" exponent="3"/><feFuncG type="gamma" exponent="3"/><feFuncB type="gamma" exponent="3"/></feComponentTransfer></filter><filter x="0" y="0" width="99999" height="99999" id="hc_extension_grayscale_back"><feComponentTransfer><feFuncR type="gamma" exponent="0.33"/><feFuncG type="gamma" exponent="0.33"/><feFuncB type="gamma" exponent="0.33"/></feComponentTransfer></filter><filter x="0" y="0" width="99999" height="99999" id="hc_extension_invert"><feComponentTransfer><feFuncR type="gamma" amplitude="-1" exponent="3" offset="1"/><feFuncG type="gamma" amplitude="-1" exponent="3" offset="1"/><feFuncB type="gamma" amplitude="-1" exponent="3" offset="1"/></feComponentTransfer></filter><filter x="0" y="0" width="99999" height="99999" id="hc_extension_invert_back"><feComponentTransfer><feFuncR type="table" tableValues="1 0"/><feFuncG type="table" tableValues="1 0"/><feFuncB type="table" tableValues="1 0"/></feComponentTransfer><feComponentTransfer><feFuncR type="gamma" exponent="1.7"/><feFuncG type="gamma" exponent="1.7"/><feFuncB type="gamma" exponent="1.7"/></feComponentTransfer></filter><filter x="0" y="0" width="99999" height="99999" id="hc_extension_invert_grayscale"><feColorMatrix type="matrix" values="0.2126 0.7152 0.0722 0 0 0.2126 0.7152 0.0722 0 0 0.2126 0.7152 0.0722 0 0 0 0 0 1 0"/><feComponentTransfer><feFuncR type="gamma" amplitude="-1" exponent="3" offset="1"/><feFuncG type="gamma" amplitude="-1" exponent="3" offset="1"/><feFuncB type="gamma" amplitude="-1" exponent="3" offset="1"/></feComponentTransfer></filter><filter x="0" y="0" width="99999" height="99999" id="hc_extension_yellow_on_black"><feComponentTransfer><feFuncR type="gamma" amplitude="-1" exponent="3" offset="1"/><feFuncG type="gamma" amplitude="-1" exponent="3" offset="1"/><feFuncB type="gamma" amplitude="-1" exponent="3" offset="1"/></feComponentTransfer><feColorMatrix type="matrix" values="0.3 0.5 0.2 0 0 0.3 0.5 0.2 0 0 0 0 0 0 0 0 0 0 1 0"/></filter><filter x="0" y="0" width="99999" height="99999" id="hc_extension_yellow_on_black_back"><feComponentTransfer><feFuncR type="table" tableValues="1 0"/><feFuncG type="table" tableValues="1 0"/><feFuncB type="table" tableValues="1 0"/></feComponentTransfer><feComponentTransfer><feFuncR type="gamma" exponent="0.33"/><feFuncG type="gamma" exponent="0.33"/><feFuncB type="gamma" exponent="0.33"/></feComponentTransfer></filter></defs></svg>';

const cssTemplate: string = `
  html[hc="a0"] {
    filter: url("#hc_extension_off");
  }
  html[hcx="0"] img[src*="jpg"],
  html[hcx="0"] img[src*="jpeg"],
  html[hcx="0"] svg image,
  html[hcx="0"] img.rg_i,
  html[hcx="0"] embed,
  html[hcx="0"] object,
  html[hcx="0"] video {
    filter: url("#hc_extension_off");
  }

  html[hc="a1"] {
    filter: url("#hc_extension_highcontrast");
  }
  html[hcx="1"] img[src*="jpg"],
  html[hcx="1"] img[src*="jpeg"],
  html[hcx="1"] img.rg_i,
  html[hcx="1"] svg image,
  html[hcx="1"] embed,
  html[hcx="1"] object,
  html[hcx="1"] video {
    filter: url("#hc_extension_highcontrast_back");
  }

  html[hc="a2"] {
    filter: url("#hc_extension_grayscale");
  }
  html[hcx="2"] img[src*="jpg"],
  html[hcx="2"] img[src*="jpeg"],
  html[hcx="2"] img.rg_i,
  html[hcx="2"] svg image,
  html[hcx="2"] embed,
  html[hcx="2"] object,
  html[hcx="2"] video {
    filter: url("#hc_extension_grayscale_back");
  }

  html[hc="a3"] {
    filter: url("#hc_extension_invert");
  }
  html[hcx="3"] img[src*="jpg"],
  html[hcx="3"] img[src*="jpeg"],
  html[hcx="3"] img.rg_i,
  html[hcx="3"] svg image,
  html[hcx="3"] embed,
  html[hcx="3"] object,
  html[hcx="3"] video {
    filter: url("#hc_extension_invert_back");
  }

  html[hc="a4"] {
    filter: url("#hc_extension_invert_grayscale");
  }
  html[hcx="4"] img[src*="jpg"],
  html[hcx="4"] img[src*="jpeg"],
  html[hcx="4"] img.rg_i,
  html[hcx="4"] svg image,
  html[hcx="4"] embed,
  html[hcx="4"] object,
  html[hcx="4"] video {
    filter: url("#hc_extension_invert_back");
  }

  html[hc="a5"] {
    filter: url("#hc_extension_yellow_on_black");
  }
  html[hcx="5"] img[src*="jpg"],
  html[hcx="5"] img[src*="jpeg"],
  html[hcx="5"] img.rg_i,
  html[hcx="5"] svg image,
  html[hcx="5"] embed,
  html[hcx="5"] object,
  html[hcx="5"] video {
    filter: url("#hc_extension_yellow_on_black_back");
  }

  html[hc] .docs-menubar,
  html[hc] .docs-titlebar-buttons,
  html[hc] .kix-appview-editor-toolbar,
  html[hc] .goog-toolbar,
  html[hc] header[role="banner"],
  html[hc] [role="toolbar"] {
    z-index: 2147483647 !important;
    will-change: transform;
    transform: translateZ(0);
    contain: paint;
    pointer-events: auto !important;
  }
`;

/**
 * Add the elements to the page that make high-contrast adjustments possible.
 */
function addOrUpdateExtraElements(): void {
  if (!enabled)
    return;

  if (!document.body) {
    setTimeout(addOrUpdateExtraElements, 100);
    return;
  }

  // We used to include the CSS, but that doesn't work when the document
  // uses the <base> element to set a relative url. So instead we
  // add a <style> element directly to the document with the right
  // urls hard-coded into it.
  let style = $('hc_style') as HTMLStyleElement | null;
  if (!style) {
    const baseUrl = window.location.href.replace(window.location.hash, '');
    const css = cssTemplate.replace(/#/g, baseUrl + '#');
    style = document.createElement('style');
    style.id = 'hc_style';
    style.setAttribute('type', 'text/css');
    style.innerHTML = css;
    document.head.appendChild(style);
  }

  // Starting in Chrome 45 we can't apply a filter to the html element,
  // so instead we create an element with low z-index that copies the
  // body's background.
  let bg = $('hc_extension_bkgnd') as HTMLDivElement | null;
  if (!bg) {
    bg = document.createElement('div');
    bg.id = 'hc_extension_bkgnd';
    Object.assign(bg.style, {
      position: 'fixed',
      left: '0px',
      top: '0px',
      right: '0px',
      bottom: '0px',
      zIndex: '-1999999999',
      pointerEvents: 'none',
    });
    document.body.appendChild(bg);
  }
  bg.style.display = 'block';
  bg.style.background = window.getComputedStyle(document.body).background;

  // As a special case, replace a zero-alpha background with white,
  // otherwise we can't invert it.
  const bgColor = bg.style.backgroundColor.replace(/\s+/g, '');
  const m = /^rgba\((\d+),(\d+),(\d+),(\d*\.?\d+)\)/.exec(bgColor);
  if (m && m[4] === '0') {
    bg.style.backgroundColor = '#fff';
  }

  // Add a hidden element with the SVG filters.
  let wrap = $('hc_extension_svg_filters');
  if (!wrap) {
    wrap = document.createElement('span');
    wrap.id = 'hc_extension_svg_filters';
    wrap.hidden = true;
    wrap.innerHTML = svgContent;
    document.body.appendChild(wrap);
  }
}

/**
 * This is called on load and every time the mode might have changed
 * (i.e. enabling/disabling, or changing the type of contrast adjustment
 * for this page).
 */
function update(): void {
  const html = document.documentElement;
  if (!html) {
    setTimeout(update, 100);
    return;
  }

  if (enabled) {
    addOrUpdateExtraElements();

    const stringScheme = String(scheme)
    const modeAndScheme = mode + stringScheme;
    if (html.getAttribute('hc') !== modeAndScheme)
      html.setAttribute('hc', modeAndScheme);
    if (html.getAttribute('hcx') !== stringScheme)
      html.setAttribute('hcx', stringScheme);
    html.style.backfaceVisibility = 'hidden';
  } else {
    html.setAttribute('hc', mode + '0');
    html.setAttribute('hcx', '0');
    setTimeout(() => {
      html.removeAttribute('hc');
      html.removeAttribute('hcx');
      html.style.backfaceVisibility = '';
      const bg =
          document.getElementById('hc_extension_bkgnd') as HTMLDivElement |
          null;
      if (bg)
        bg.style.display = 'none';
    }, 0);
  }
}

/**
 * Called when a pref changes.
 */
async function maybeUpdate(): Promise<void> {
  const [newEnabled, newScheme] = await Promise.all([
    getEnabled(),
    getSiteScheme(siteFromUrl(location.href)),
  ]);
  if (enabled !== newEnabled || scheme !== newScheme) {
    enabled = newEnabled;
    scheme = newScheme;
    update();
  }
}

/**
 * KeyDown event handler
 */
function onKeyDown(evt: KeyboardEvent): boolean {
  if (evt.shiftKey && evt.key === 'F11') {
    chrome.runtime.sendMessage(null, {toggle_global: true});
    evt.stopPropagation();
    evt.preventDefault();
    return false;
  }
  if (evt.shiftKey && evt.key === 'F12') {
    chrome.runtime.sendMessage(null, {toggle_site: true});
    evt.stopPropagation();
    evt.preventDefault();
    return false;
  }
  return true;
}

function init(): void {
  mode = window === window.top ? 'a' : 'b';
  chrome.storage.local.onChanged.addListener(maybeUpdate);
  chrome.storage.local.get({}).then(async () => {
    enabled = await getEnabled();
    scheme = await getSiteScheme(siteFromUrl(location.href));
    update();
  });

  document.addEventListener('keydown', onKeyDown, false);

  // Update again after a few seconds and again after load so that
  // the background isn't wrong for long.
  window.setTimeout(addOrUpdateExtraElements, 2000);
  window.addEventListener('load', () => {
    addOrUpdateExtraElements();

    // Also update when the document body attributes change.
    const config: MutationObserverInit = {attributes: true};
    const observer = new MutationObserver(() => {
      addOrUpdateExtraElements();
    });
    if (document.body)
      observer.observe(document.body, config);
  });
}

init();
