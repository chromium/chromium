// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {toSandboxedURL} from '../../common/js/url_constants.js';

import {getTemplate} from './files_safe_media.html.js';

export interface FilesSafeMedia {
  $: {content: HTMLDivElement};
  type: string;
  src: FilePreviewContent;
  fire: (eventName: string) => void;
}

type ContentsIframeNode = HTMLIFrameElement&{isVideoMedia_: boolean};

/**
 * Polymer element to render media securely in a chrome-untrusted:// <iframe>
 * element.
 *
 * When tapped, 'files-safe-media-tap-inside', 'files-safe-media-tap-outside'
 * events are fired depending on the position of the tap.
 */
export class FilesSafeMedia extends PolymerElement {
  static get is() {
    return 'files-safe-media';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      /**
       * Source content accessible from the sandboxed environment.
       */
      src: {
        type: Object,
        observer: 'onSrcChange_',
        reflectToAttribute: true,
      },

      /**
       * <files-safe-media> media type: e.g. audio, image, video, html.
       */
      type: {
        type: String,
        readonly: true,
      },
    };
  }

  private contentsNode_: ContentsIframeNode|null = null;

  private sourceFile_() {
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
  }

  private onSrcChange_() {
    const hasContent = this.src.dataType !== '';

    if (!hasContent) {
      // Remove untrusted iframe to clean up unnecessary processes.
      if (this.contentsNode_) {
        this.$.content.removeChild(this.contentsNode_);
        this.contentsNode_ = null;
      }
      return;
    }

    if (this.contentsNode_ && this.contentsNode_.isVideoMedia_) {
      // Remove old video item first to stop UI flicker when drawing the new
      // video content: bug b/260619403
      this.$.content.removeChild(this.contentsNode_);
      this.contentsNode_ = null;
    }

    if (!this.contentsNode_) {
      // Create the node, which will callback here (onSrcChange_) when done.
      this.createUntrustedContents_();
      return;
    }

    const data: UntrustedPreviewData = {
      type: this.type,
      sourceContent: this.src,
    };

    if (this.contentsNode_.contentWindow) {
      this.contentsNode_.isVideoMedia_ = (this.type === 'video');
      // Send the data to preview to the untrusted <iframe>.
      this.contentsNode_.contentWindow.postMessage(
          data, toSandboxedURL().origin);
    }
  }

  private createUntrustedContents_() {
    const node = document.createElement('iframe') as ContentsIframeNode;
    this.contentsNode_ = node;
    // Allow autoplay for audio files.
    if (this.type === 'audio') {
      node.setAttribute('allow', 'autoplay');
    }
    this.$.content.appendChild(node);
    node.addEventListener('load', () => this.onSrcChange_());
    node.src = this.sourceFile_();
  }

  created() {
    /**
     * Holds the untrusted iframe when a source to preview is set. Set to null
     * otherwise.
     */
    this.contentsNode_ = null;
  }

  override ready() {
    super.ready();
    this.addEventListener('focus', () => {
      if (this.type === 'audio' || this.type === 'video') {
        // Avoid setting the focus on the files-safe-media itself, rather sends
        // it down to its untrusted iframe element.
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
        this.dispatchEvent(new CustomEvent(
            'files-safe-media-tap-inside', {bubbles: true, composed: true}));
      } else if (event.data === 'tap-outside') {
        this.dispatchEvent(new CustomEvent(
            'files-safe-media-tap-outside', {bubbles: true, composed: true}));
      } else if (event.data === 'webview-loaded') {
        if (this.contentsNode_) {
          this.contentsNode_.setAttribute('loaded', '');
        }
      } else if (event.data === 'webview-cleared') {
        if (this.contentsNode_) {
          this.contentsNode_.removeAttribute('loaded');
        }
      } else if (event.data === 'content-decode-failed') {
        this.dispatchEvent(new CustomEvent(
            'files-safe-media-load-error', {bubbles: true, composed: true}));
      }
    });
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'files-safe-media': FilesSafeMedia;
  }
}

customElements.define(FilesSafeMedia.is, FilesSafeMedia);
