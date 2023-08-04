// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

interface WebshellServices {
  allowWebviewElementRegistration(callback: ()=>void): void;
}

declare var webshell: WebshellServices;

class WebviewElement extends HTMLElement {
  public iframeElement: HTMLIFrameElement;

  constructor() {
    super();
    this.iframeElement = document.createElement('iframe');
    this.iframeElement.style.width = '100%';
    this.iframeElement.style.height = '100%';
    this.iframeElement.style.border = '0px';
    this.iframeElement.style.margin = "0";
    this.iframeElement.style.padding = "0";
    this.appendChild(this.iframeElement);
  }
}

webshell.allowWebviewElementRegistration(() => {
  customElements.define("webview", WebviewElement);
});
