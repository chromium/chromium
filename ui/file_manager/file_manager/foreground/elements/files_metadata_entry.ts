// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {PolymerElement} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './files_metadata_entry.html.js';

export interface FilesMetadataEntry {
  $: {
    valueContainer: HTMLDivElement,
  };
  isPath: boolean;
  loading: boolean;
  value: string;
  key: string;
}

export class  FilesMetadataEntry extends PolymerElement {
  static get is() {
    return 'files-metadata-entry';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      key: {
        type: String,
        reflectToAttribute: true,
      },

      // If |value| is empty, the entire entry will be hidden.
      value: {
        type: String,
        reflectToAttribute: true,
        observer: 'valueChanged',
      },

      loading: {
        type: Boolean,
        reflectToAttribute: true,
        value: false,
      },

      isPath: {
        type: Boolean,
        value: false,
      },
    };
  }

  /**
   * When value is changed, it is displayed in the #valueContainer element.
   * How the value is represented depends on [[isPath]] value.
   */
  valueChanged(newValue: string) {
    const container = this.$.valueContainer;
    if (!newValue) {
      container.textContent = '';
      return;
    }
    if (this.isPath) {
      // Divide path 'foo/bar/baz.png' to ['foo', 'bar', 'baz.png'] and
      // append corresponding span elements (<span>foo/</span> etc...) in the
      // container.
      //
      // Note that, if the container's children are
      // <span>foo/</span><span>bar/</span><span>baz.png</span>,
      // container.textContent evaluates to 'foo/bar/baz.png'. That's why the
      // container.textContent is still equal to [[value]] regardless of
      // [[isPath]] and integration tests verifying element's textContent won't
      // be affected.
      container.textContent = '';
      const components = newValue.split('/');
      for (let i = 0; i < components.length; i++) {
        const span = document.createElement('span');
        span.textContent =
            i < components.length - 1 ? (components[i] + '/') : components[i]!;
        container.appendChild(span);
      }
    } else {
      container.textContent = newValue;
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'files-metadata-entry': FilesMetadataEntry;
  }
}

customElements.define(FilesMetadataEntry.is, FilesMetadataEntry);
