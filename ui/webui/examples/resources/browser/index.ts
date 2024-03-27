// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageHandlerFactory, PageHandlerRemote} from './browser.mojom-webui.js';
import {Url as MojoUrl} from '//resources/mojo/url/mojom/url.mojom-webui.js';

import {
  DictionaryValue as mojoBase_mojom_DictionaryValue
} from '//resources/mojo/mojo/public/mojom/base/values.mojom-webui.js';

interface WebshellServices {
  allowWebviewElementRegistration(callback: ()=>void): void;
  getNextId(): number;
  registerWebView(viewInstanceId: number): void;
  attachIframeGuest(containerId: number,
                    guestInstanceId: number,
                    attachParams: object,
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
  private viewInstanceId: number;
  private containerId: number;
  private guestInstanceId: number;

  constructor() {
    super();
    this.iframeElement = document.createElement('iframe');
    this.iframeElement.style.border = '0px';
    this.iframeElement.style.margin = "0";
    this.iframeElement.style.padding = "0";
    this.iframeElement.style.flex = '1';
    this.appendChild(this.iframeElement);
    this.viewInstanceId = webshell.getNextId();
    this.containerId = webshell.getNextId();
    this.guestInstanceId = -1;
    const instance = this;
    webshell.registerWebView(this.viewInstanceId);
    const createParams: mojoBase_mojom_DictionaryValue = {
      storage: {"instanceId": {"intValue": this.viewInstanceId}}
    };
    BrowserProxy.getInstance().createGuestView(
        createParams).then((result) => {
          instance.onGuestViewCreated(result.guestInstanceId);
        });
  }

  onGuestViewCreated(guestInstanceId: number) {
    this.guestInstanceId = guestInstanceId;
    const createParams: mojoBase_mojom_DictionaryValue = {
      storage: {"instanceId": {"intValue": this.viewInstanceId}}
    };
    const iframeContentWindow = this.iframeElement.contentWindow;
    webshell.attachIframeGuest(this.containerId,
                               this.guestInstanceId,
                               createParams,
                               iframeContentWindow);
  }

  navigate(src: MojoUrl) {
    BrowserProxy.getInstance().navigate(this.guestInstanceId, src);
  }

  goBack() {
    BrowserProxy.getInstance().goBack(this.guestInstanceId);
  }

  goForward() {
    BrowserProxy.getInstance().goForward(this.guestInstanceId);
  }
}

webshell.allowWebviewElementRegistration(() => {
  customElements.define("webview", WebviewElement);
});

function navigateToAddressBarUrl() {
  const webview = document.getElementById("webview") as WebviewElement;
  const addressBar = document.getElementById("address") as HTMLInputElement;
  if (webview && addressBar) {
    try {
      // Validate the URL before converting it to a Mojo URL to avoid a Mojo
      // validation error when sending this call to the browser. Successful
      // construction indicates a valid URL.
      const src = new URL(addressBar.value);
      const mojoSrc: MojoUrl = {url: src.toString()};
      webview.navigate(mojoSrc);
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
