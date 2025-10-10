// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageHandlerFactory, PageHandlerRemote} from './browser.mojom-webui.js';
import {loadTimeData} from '//resources/js/load_time_data.js';
import {Url as MojoUrl} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import '/strings.m.js';

interface WebshellServices {
  allowWebviewElementRegistration(callback: ()=>void): void;
  attachIframeGuest(guestContentsId: number,
                    contentWindow: Window | null): void
}

declare var webshell: WebshellServices;

class BrowserProxy {
  private handler: PageHandlerRemote;
  static instance_: BrowserProxy ;

  constructor() {
    this.handler = new PageHandlerRemote();
    const factory = PageHandlerFactory.getRemote();
    factory.createPageHandler(this.handler.$.bindNewPipeAndPassReceiver());
  }

  getHandler(): PageHandlerRemote {
    return this.handler;
  }

  static getInstance(): PageHandlerRemote {
    if (!BrowserProxy.instance_) {
      BrowserProxy.instance_ = new BrowserProxy();
    }
    return BrowserProxy.instance_.getHandler();
  }
}

class WebviewElement extends HTMLElement {
  public iframeElement: HTMLIFrameElement;
  private guestContentsId: number;
  private attached: boolean = false;

  constructor() {
    super();
    this.iframeElement = document.createElement('iframe');
    this.iframeElement.style.border = '0px';
    this.iframeElement.style.margin = "0";
    this.iframeElement.style.padding = "0";
    this.iframeElement.style.flex = '1';
    this.guestContentsId = loadTimeData.getInteger('guest-contents-id');
    console.log('guest-contents-id', this.guestContentsId);
  }

  connectedCallback() {
    if (!this.attached) {
      this.appendChild(this.iframeElement);
      this.attached = true;
      const iframeContentWindow = this.iframeElement.contentWindow;
      webshell.attachIframeGuest(this.guestContentsId,
                                iframeContentWindow);
    }
  }

  navigate(src: MojoUrl) {
    BrowserProxy.getInstance().navigate(this.guestContentsId, src);
  }

  goBack() {
    BrowserProxy.getInstance().goBack(this.guestContentsId);
  }

  goForward() {
    BrowserProxy.getInstance().goForward(this.guestContentsId);
  }
}

class SecureEmbedElement extends HTMLElement {
  public embedElement: HTMLEmbedElement;
  private guestContentsId: number;
  private attached: boolean = false;

  constructor() {
    super();
    this.embedElement = document.createElement('embed');
    this.embedElement.style.border = '0px';
    this.embedElement.style.margin = "0";
    this.embedElement.style.padding = "0";
    this.embedElement.style.flex = '1';
    this.guestContentsId = loadTimeData.getInteger('guest-contents-id');
    console.log('secure-embed guest-contents-id', this.guestContentsId);
    this.embedElement.setAttribute('data-content-id', this.guestContentsId.toString());
    this.embedElement.setAttribute('type', 'application/x-google-chrome-secure-embed');
  }

  connectedCallback() {
    if (!this.attached) {
      this.appendChild(this.embedElement);
      this.attached = true;
    }
  }

  navigate(src: MojoUrl) {
    BrowserProxy.getInstance().navigate(this.guestContentsId, src);
  }

  goBack() {
    BrowserProxy.getInstance().goBack(this.guestContentsId);
  }

  goForward() {
    BrowserProxy.getInstance().goForward(this.guestContentsId);
  }
}

webshell.allowWebviewElementRegistration(() => {
  customElements.define("webview", WebviewElement);
  customElements.define("secureembed", SecureEmbedElement);
});

let currentEmbedElement: WebviewElement | SecureEmbedElement | null = null;

function attachViaGuestContents() {
  if (currentEmbedElement) {
    currentEmbedElement.remove();
  }

  // Create and attach WebviewElement
  const webview = document.createElement("webview") as WebviewElement;
  webview.id = "webview";
  document.body.appendChild(webview);
  currentEmbedElement = webview;
}

function attachViaSecureEmbed() {
  if (currentEmbedElement) {
    currentEmbedElement.remove();
  }

  // Create and attach SecureEmbedElement
  const secureEmbed = document.createElement("secureembed") as SecureEmbedElement;
  secureEmbed.id = "webview";
  document.body.appendChild(secureEmbed);
  currentEmbedElement = secureEmbed;
}

const attachGuestButton = document.getElementById("attach-guest");
if (attachGuestButton) {
  attachGuestButton.addEventListener("click", attachViaGuestContents);
}

const attachSecureEmbedButton = document.getElementById("attach-secure-embed");
if (attachSecureEmbedButton) {
  attachSecureEmbedButton.addEventListener("click", attachViaSecureEmbed);
}

function navigateToAddressBarUrl() {
  const addressBar = document.getElementById("address") as HTMLInputElement;
  if (currentEmbedElement && addressBar) {
    try {
      // Validate the URL before converting it to a Mojo URL to avoid a Mojo
      // validation error when sending this call to the browser. Successful
      // construction indicates a valid URL.
      const src = new URL(addressBar.value);
      const mojoSrc: MojoUrl = {url: src.toString()};
      currentEmbedElement.navigate(mojoSrc);
    } catch (error) {
      console.error(error);
    }
  }
}

const goButton = document.getElementById("go");
if (goButton) {
  goButton.addEventListener("click", navigateToAddressBarUrl);
}

const addressBar = document.getElementById("address") as HTMLInputElement;
if (addressBar) {
  addressBar.addEventListener("keypress", (event) => {
    if (event.key === "Enter") {
      event.preventDefault();
      navigateToAddressBarUrl();
    }
  });
}

const backButton = document.getElementById("back");
if (backButton) {
  backButton.addEventListener("click", ()=>{
    if (currentEmbedElement) {
      currentEmbedElement.goBack();
    }
  });
}

const forwardButton = document.getElementById("forward");
if (forwardButton) {
  forwardButton.addEventListener("click", ()=>{
    if (currentEmbedElement) {
      currentEmbedElement.goForward();
    }
  });
}
