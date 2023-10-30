// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../cr_shared_vars.css.js';

import {assertNotReached} from '//resources/js/assert.js';
import {listenOnce} from '//resources/js/util.js';
import {PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {getTemplate} from './cr_drawer.html.js';

export interface CrDrawerElement {
  $: {
    dialog: HTMLDialogElement,
  };
}

export class CrDrawerElement extends PolymerElement {
  static get is() {
    return 'cr-drawer';
  }

  static get template() {
    return getTemplate();
  }

  static get properties() {
    return {
      heading: String,

      show_: {
        type: Boolean,
        reflectToAttribute: true,
      },

      /** The alignment of the drawer on the screen ('ltr' or 'rtl'). */
      align: {
        type: String,
        value: 'ltr',
        reflectToAttribute: true,
      },
    };
  }

  heading: string;
  align: 'ltr'|'rtl';
  private show_: boolean;

  private fire_(eventName: string, detail?: any) {
    this.dispatchEvent(
        new CustomEvent(eventName, {bubbles: true, composed: true, detail}));
  }

  get open(): boolean {
    return this.$.dialog.open;
  }

  set open(_value: boolean) {
    assertNotReached('Cannot set |open|.');
  }

  /** Toggles the drawer open and close. */
  toggle() {
    if (this.open) {
      this.cancel();
    } else {
      this.openDrawer();
    }
  }

  /** Shows drawer and slides it into view. */
  openDrawer() {
    if (this.open) {
      return;
    }
    this.$.dialog.showModal();
    this.show_ = true;
    this.fire_('cr-drawer-opening');
    listenOnce(this.$.dialog, 'transitionend', () => {
      this.fire_('cr-drawer-opened');
    });
  }

  /**
   * Slides the drawer away, then closes it after the transition has ended. It
   * is up to the owner of this component to differentiate between close and
   * cancel.
   */
  private dismiss_(cancel: boolean) {
    if (!this.open) {
      return;
    }
    this.show_ = false;
    listenOnce(this.$.dialog, 'transitionend', () => {
      this.$.dialog.close(cancel ? 'canceled' : 'closed');
    });
  }

  cancel() {
    this.dismiss_(true);
  }

  close() {
    this.dismiss_(false);
  }

  wasCanceled(): boolean {
    return !this.open && this.$.dialog.returnValue === 'canceled';
  }

  /**
   * Stop propagation of a tap event inside the container. This will allow
   * |onDialogClick_| to only be called when clicked outside the container.
   */
  private onContainerClick_(event: Event) {
    event.stopPropagation();
  }

  /**
   * Close the dialog when tapped outside the container.
   */
  private onDialogClick_() {
    this.cancel();
  }

  /**
   * Overrides the default cancel machanism to allow for a close animation.
   */
  private onDialogCancel_(event: Event) {
    event.preventDefault();
    this.cancel();
  }

  private onDialogClose_() {
    // Catch and re-fire the 'close' event such that it bubbles across Shadow
    // DOM v1.
    this.fire_('close');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-drawer': CrDrawerElement;
  }
}

customElements.define(CrDrawerElement.is, CrDrawerElement);
