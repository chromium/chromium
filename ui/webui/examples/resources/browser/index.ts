// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PageHandlerFactory, PageHandlerRemote} from './browser.mojom-webui.js';
import {loadTimeData} from '//resources/js/load_time_data.js';

import '/strings.m.js';

const SURFACE_EMBED_MIME_TYPE = 'application/x-chromium-surface-embed';

interface WebshellServices {
  allowWebviewElementRegistration(callback: ()=>void): void;
  attachIframeGuest(guestContentsId: string, contentWindow: Window|null): void
  surfaceEmbedEnabled: boolean;
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

abstract class EmbeddableElement extends HTMLElement {
  protected childElement: HTMLElement;
  protected guestContentsId: string;
  private attached: boolean = false;

  constructor(childElement: HTMLElement) {
    super();
    this.childElement = childElement;
    this.childElement.style.border = '0px';
    this.childElement.style.margin = '0';
    this.childElement.style.padding = '0';
    this.childElement.style.flex = '1';
    this.guestContentsId = loadTimeData.getString('guest-contents-id');
  }

  connectedCallback() {
    if (!this.attached) {
      this.appendChild(this.childElement);
      this.attached = true;
      this.onAttached();
    }
  }

  protected onAttached(): void {}

  navigate(src: string) {
    BrowserProxy.getInstance().navigate(this.guestContentsId, src);
  }

  goBack() {
    BrowserProxy.getInstance().goBack(this.guestContentsId);
  }

  goForward() {
    BrowserProxy.getInstance().goForward(this.guestContentsId);
  }
}

class WebviewElement extends EmbeddableElement {
  constructor() {
    super(document.createElement('iframe'));
    console.log('webview guest-contents-id', this.guestContentsId);
  }

  protected override onAttached(): void {
    const iframe = this.childElement as HTMLIFrameElement;
    webshell.attachIframeGuest(this.guestContentsId, iframe.contentWindow);
  }
}

class SurfaceEmbedElement extends EmbeddableElement {
  constructor() {
    super(document.createElement('embed'));
    console.log('surface-embed guest-contents-id', this.guestContentsId);
    this.childElement.setAttribute('data-content-id', this.guestContentsId);
    this.childElement.setAttribute('type', SURFACE_EMBED_MIME_TYPE);
  }
}

webshell.allowWebviewElementRegistration(() => {
  customElements.define('webview', WebviewElement);
  if (webshell.surfaceEmbedEnabled) {
    customElements.define('surfaceembed', SurfaceEmbedElement);
  }
});

let embeddedElement: EmbeddableElement | null = null;
function attachElement(tagName: 'webview' | 'surfaceembed') {
  if (embeddedElement) {
    embeddedElement.remove();
  }

  embeddedElement = document.createElement(tagName) as EmbeddableElement;
  document.body.appendChild(embeddedElement);
}

attachElement(webshell.surfaceEmbedEnabled ? 'surfaceembed' : 'webview');

function navigateToAddressBarUrl() {
  const addressBar = document.getElementById('address') as HTMLInputElement;
  if (embeddedElement && addressBar) {
    try {
      // Validate the URL before converting it to a Mojo URL to avoid a Mojo
      // validation error when sending this call to the browser. Successful
      // construction indicates a valid URL.
      const src = new URL(addressBar.value);
      const mojoSrc = src.toString();
      embeddedElement.navigate(mojoSrc);
    } catch (error) {
      console.error(error);
    }
  }
}

const goButton = document.getElementById('go');
if (goButton) {
  goButton.addEventListener('click', navigateToAddressBarUrl);
}

const addressBar = document.getElementById('address') as HTMLInputElement;
if (addressBar) {
  addressBar.addEventListener('keypress', (event) => {
    if (event.key === 'Enter') {
      event.preventDefault();
      navigateToAddressBarUrl();
    }
  });
}

const backButton = document.getElementById('back');
if (backButton) {
  backButton.addEventListener('click', ()=>{
    embeddedElement?.goBack();
  });
}

const forwardButton = document.getElementById('forward');
if (forwardButton) {
  forwardButton.addEventListener('click', ()=>{
    embeddedElement?.goForward();
  });
}
