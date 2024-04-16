// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {EventTracker} from '//resources/js/event_tracker.js';
import type {PropertyValues} from '//resources/lit/v3_0/lit.rollup.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './cr_icon.css.js';
import {IconsetMap} from './iconset_map.js';
import type {Iconset} from './iconset_map.js';

export class CrIconElement extends CrLitElement {
  static get is() {
    return 'cr-icon';
  }

  static override get styles() {
    return getCss();
  }

  static override get properties() {
    return {
      /**
       * The name of the icon to use. The name should be of the form:
       * `iconset_name:icon_name`.
       */
      icon: {type: String},
    };
  }

  icon: string = '';
  private iconsetName_: string = '';
  private iconName_: string = '';
  private iconset_: Iconset|null = null;
  private tracker_: EventTracker = new EventTracker();

  override updated(changedProperties: PropertyValues<this>) {
    super.updated(changedProperties);

    if (changedProperties.has('icon')) {
      const [iconsetName, iconName] = this.icon.split(':');
      this.iconName_ = iconName;
      this.iconsetName_ = iconsetName;
      this.updateIcon();
    }
  }

  updateIcon() {
    if (this.iconName_ === '' && this.iconset_) {
      this.iconset_.removeIcon(this);
    } else if (this.iconsetName_) {
      const iconsetMap = IconsetMap.getInstance();
      this.iconset_ = iconsetMap.get(this.iconsetName_);
      if (this.iconset_) {
        this.iconset_.applyIcon(this, this.iconName_);
        this.tracker_.remove(iconsetMap, 'cr-iconset-added');
      } else {
        this.tracker_.add(
            iconsetMap, 'cr-iconset-added', () => this.updateIcon());
      }
    }
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-icon': CrIconElement;
  }
}

customElements.define(CrIconElement.is, CrIconElement);
