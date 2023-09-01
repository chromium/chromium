// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageHandlerFactory, PageHandlerRemote} from './browser.mojom-webui.js';
import {Url} from 'chrome://resources/mojo/url/mojom/url.mojom-webui.js';

interface WebshellServices {
  allowWebviewElementRegistration(callback: ()=>void): void;
  getNextId(): number;
  registerWebView(viewInstanceId: number): void;
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
  private viewInstanceId: number;

  constructor() {
    super();
    this.iframeElement = document.createElement('iframe');
    this.iframeElement.style.width = '100%';
    this.iframeElement.style.height = '100%';
    this.iframeElement.style.border = '0px';
    this.iframeElement.style.margin = "0";
    this.iframeElement.style.padding = "0";
    this.appendChild(this.iframeElement);
    this.viewInstanceId = webshell.getNextId();
    webshell.registerWebView(this.viewInstanceId);
  }

  navigate(src: Url) {
    BrowserProxy.getInstance().navigate(this.viewInstanceId, src);
  }

  goBack() {
    BrowserProxy.getInstance().goBack(this.viewInstanceId);
  }

  goForward() {
    BrowserProxy.getInstance().goForward(this.viewInstanceId);
  }
}

webshell.allowWebviewElementRegistration(() => {
  customElements.define("webview", WebviewElement);
});

function navigateToAddressBarUrl() {
  const webview = document.getElementById("webview") as WebviewElement;
  const addressBar = document.getElementById("address") as HTMLInputElement;
  if (webview && addressBar) {
    const src = new Url();
    src.url = addressBar.value;
    webview.navigate(src);
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
    const webview = document.getElementById("webview") as WebviewElement;
    if (webview) {
      webview.goBack();
    }
  });
}

const forwardButton = document.getElementById("forward");
if (forwardButton) {
  forwardButton.addEventListener("click", ()=>{
    const webview = document.getElementById("webview") as WebviewElement;
    if (webview) {
      webview.goForward();
    }
  });
}
