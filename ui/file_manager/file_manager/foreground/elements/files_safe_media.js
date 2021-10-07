// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

const FILES_APP_ORIGIN = 'chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj';

/**
 * Polymer element to render a media securely inside webview.
 * When tapped, files-safe-media-tap-inside or
 * files-safe-media-tap-outside events are fired depending on the position
 * of the tap.
 */
const FilesSafeMedia = Polymer({
  _template: html`{__html_template__}`,

  is: 'files-safe-media',

  properties: {
    // Source content accessible from the sandboxed environment.
    src: {
      type: Object,
      observer: 'onSrcChange_',
      reflectToAttribute: true,
    },
    type: {
      type: String,
      readonly: true,
    }
  },

  listeners: {
    'src-changed': 'onSrcChange_',
  },

  /**
   * @return {string}
   */
  sourceFile_: function() {
    const sandboxedRelativePath = 'foreground/elements/sandboxed/';
    switch (this.type) {
      case 'image':
        return sandboxedRelativePath + 'files_img_content.html';
      case 'audio':
        return sandboxedRelativePath + 'files_audio_content.html';
      case 'video':
        return sandboxedRelativePath + 'files_video_content.html';
      case 'html':
        return sandboxedRelativePath + 'files_text_content.html';
      default:
        console.error('Unsupported type: ' + this.type);
        return '';
    }
  },

  onSrcChange_: function() {
    const hasContent = this.src.dataType !== '';
    if (!hasContent && this.webview_) {
      // Remove webview to clean up unnecessary processes.
      this.$.content.removeChild(this.webview_);
      this.webview_ = null;
    } else if (hasContent && !this.webview_) {
      // Create webview node only if src exists to save resources.
      const webview =
          /** @type {!HTMLElement} */ (document.createElement('webview'));
      this.webview_ = webview;
      webview.partition = 'trusted';
      webview.allowtransparency = 'true';
      this.$.content.appendChild(webview);
      webview.addEventListener('contentload', () => this.onSrcChange_());
      webview.src = this.sourceFile_();
    } else if (hasContent && this.webview_.contentWindow) {
      /** @type {!UntrustedPreviewData} */
      const data = {
        type: this.type,
        sourceContent: /** @type {!FilePreviewContent} */ (this.src),
      };
      window.setTimeout(() => {
        this.webview_.contentWindow.postMessage(data, FILES_APP_ORIGIN);
      });
    }
  },

  created: function() {
    /**
     * @type {HTMLElement}
     */
    this.webview_ = null;
  },

  ready: function() {
    this.addEventListener('focus', (event) => {
      if (this.type === 'audio' || this.type === 'video') {
        // Avoid setting the focus on the files-safe-media itself, rather sends
        // it down to its webview element.
        if (this.webview_) {
          this.webview_.focus();
        }
      }
    });
    window.addEventListener('message', event => {
      if (event.origin !== FILES_APP_ORIGIN) {
        console.log('Unknown origin.');
        return;
      }
      if (event.data === 'tap-inside') {
        this.fire('files-safe-media-tap-inside');
      } else if (event.data === 'tap-outside') {
        this.fire('files-safe-media-tap-outside');
      } else if (event.data === 'webview-loaded') {
        if (this.webview_) {
          this.webview_.setAttribute('loaded', '');
        }
      } else if (event.data === 'webview-cleared') {
        if (this.webview_) {
          this.webview_.removeAttribute('loaded');
        }
      } else if (event.data === 'content-decode-failed') {
        this.fire('files-safe-media-load-error');
      }
    });
  }
});

//# sourceURL=//ui/file_manager/file_manager/foreground/elements/files_safe_media.js
