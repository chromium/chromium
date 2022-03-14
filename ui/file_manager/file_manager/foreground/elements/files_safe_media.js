// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {html, Polymer} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {toSandboxedURL} from '../../common/js/url_constants.js';

/**
 * Polymer element to render a media securely inside a webview or a
 * chrome-untrusted:// iframe. When tapped, files-safe-media-tap-inside or
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
    switch (this.type) {
      case 'image':
        return toSandboxedURL('untrusted_resources/files_img_content.html')
            .toString();
      case 'audio':
        return toSandboxedURL('untrusted_resources/files_audio_content.html')
            .toString();
      case 'video':
        return toSandboxedURL('untrusted_resources/files_video_content.html')
            .toString();
      case 'html':
        return toSandboxedURL('untrusted_resources/files_text_content.html')
            .toString();
      default:
        console.warn('Unsupported type: ' + this.type);
        return '';
    }
  },

  onSrcChange_: function() {
    const hasContent = this.src.dataType !== '';
    if (!hasContent && this.contentsNode_) {
      // Remove webview or iframe to clean up unnecessary processes.
      this.$.content.removeChild(this.contentsNode_);
      this.contentsNode_ = null;
    } else if (hasContent && !this.contentsNode_) {
      // Create node only if src exists to save resources.
      if (window.isSWA) {
        this.createUntrustedContents_();
      } else {
        this.createWebviewContents_();
      }
    } else if (hasContent && this.contentsNode_.contentWindow) {
      /** @type {!UntrustedPreviewData} */
      const data = {
        type: this.type,
        sourceContent: /** @type {!FilePreviewContent} */ (this.src),
      };
      window.setTimeout(() => {
        this.contentsNode_.contentWindow.postMessage(
            data, toSandboxedURL().origin);
      });
    }
  },

  createWebviewContents_: function() {
    const webview =
        /** @type {!HTMLElement} */ (document.createElement('webview'));
    this.contentsNode_ = webview;
    webview.partition = 'trusted';
    webview.allowtransparency = 'true';
    this.$.content.appendChild(webview);
    webview.addEventListener('contentload', () => this.onSrcChange_());
    webview.src = this.sourceFile_();
  },

  createUntrustedContents_: function() {
    const node =
        /** @type {!HTMLElement} */ (document.createElement('iframe'));
    this.contentsNode_ = node;
    node.style.width = '100%';
    node.style.height = '100%';
    node.style.border = '0px';
    // Allow autoplay for audio files.
    if (this.type === 'audio') {
      node.setAttribute('allow', 'autoplay');
    }
    this.$.content.appendChild(node);
    node.addEventListener('load', () => this.onSrcChange_());
    node.src = this.sourceFile_();
  },

  created: function() {
    /**
     * @private {?HTMLElement} Holds the untrusted frame when a source to
     *     preview is set. Set to null otherwise.
     */
    this.contentsNode_ = null;
  },

  ready: function() {
    this.addEventListener('focus', (event) => {
      if (this.type === 'audio' || this.type === 'video') {
        // Avoid setting the focus on the files-safe-media itself, rather sends
        // it down to its webview element.
        if (this.contentsNode_) {
          this.contentsNode_.focus();
        }
      }
    });
    window.addEventListener('message', event => {
      if (event.origin !== toSandboxedURL().origin) {
        console.warn('Unknown origin: ' + event.origin);
        return;
      }
      if (event.data === 'tap-inside') {
        this.fire('files-safe-media-tap-inside');
      } else if (event.data === 'tap-outside') {
        this.fire('files-safe-media-tap-outside');
      } else if (event.data === 'webview-loaded') {
        if (this.contentsNode_) {
          this.contentsNode_.setAttribute('loaded', '');
        }
      } else if (event.data === 'webview-cleared') {
        if (this.contentsNode_) {
          this.contentsNode_.removeAttribute('loaded');
        }
      } else if (event.data === 'content-decode-failed') {
        this.fire('files-safe-media-load-error');
      }
    });
  }
});

//# sourceURL=//ui/file_manager/file_manager/foreground/elements/files_safe_media.js
