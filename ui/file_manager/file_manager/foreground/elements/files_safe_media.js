// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const FILES_APP_ORIGIN = 'chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj';

/**
 * Polymer element to render a media securely inside webview.
 * When tapped, files-safe-media-tap-inside or
 * files-safe-media-tap-outside events are fired depending on the position
 * of the tap.
 */
var FilesSafeMedia = Polymer({
  is: 'files-safe-media',

  properties: {
    // URL accessible from webview.
    src: {
      type: String,
      observer: 'onSrcChange_',
      reflectToAttribute: true
    },
    type: {
      type: String,
      readonly: true,
    }
  },

  listeners: {'src-changed': 'onSrcChange_'},

  /**
   * @return {string}
   */
  sourceFile_: function() {
    switch (this.type) {
      case 'image':
         return 'foreground/elements/files_safe_img_webview_content.html';
      case 'audio':
         return 'foreground/elements/files_safe_audio_webview_content.html';
      case 'video':
         return 'foreground/elements/files_safe_video_webview_content.html';
      case 'html':
        return 'foreground/elements/files_safe_text_webview_content.html';
      default:
        console.error('Unsupported type: ' + this.type);
        return '';
    }
  },

  onSrcChange_: function() {
    if (!this.src && this.webview_) {
      // Remove webview to clean up unnecessary processes.
      this.$.content.removeChild(this.webview_);
      this.webview_ = null;
    } else if (this.src && !this.webview_) {
      // Create webview node only if src exists to save resources.
      const webview =
          /** @type {!HTMLElement} */ (document.createElement('webview'));
      this.webview_ = webview;
      webview.partition = 'trusted';
      webview.allowtransparency = 'true';
      this.$.content.appendChild(webview);
      webview.addEventListener(
          'contentload', this.onSrcChange_.bind(this));
      webview.src = this.sourceFile_();
    } else if (this.src && this.webview_.contentWindow) {
      const data = {};
      data.type = this.type;
      data.src = this.src;
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
      }
    });
  }
});
