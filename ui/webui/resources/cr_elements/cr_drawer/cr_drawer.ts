// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import '../cr_shared_vars.css.js';

import {assertNotReached} from '//resources/js/assert.js';
import {listenOnce} from '//resources/js/util.js';
import {CrLitElement} from '//resources/lit/v3_0/lit.rollup.js';

import {getCss} from './cr_drawer.css.js';
import {getHtml} from './cr_drawer.html.js';

export interface CrDrawerElement {
  $: {
    dialog: HTMLDialogElement,
  };
}

export class CrDrawerElement extends CrLitElement {
  static get is() {
    return 'cr-drawer';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      heading: {type: String},

      show_: {
        type: Boolean,
        reflect: true,
      },

      /** The alignment of the drawer on the screen ('ltr' or 'rtl'). */
      align: {
        type: String,
        reflect: true,
      },
    };
  }

  heading: string = '';
  align: 'ltr'|'rtl' = 'ltr';
  protected show_: boolean = false;

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
  async openDrawer() {
    if (this.open) {
      return;
    }
    this.$.dialog.showModal();
    this.show_ = true;
    await this.updateComplete;
    this.fire('cr-drawer-opening');
    listenOnce(this.$.dialog, 'transitionend', () => {
      this.fire('cr-drawer-opened');
    });
  }

  /**
   * Slides the drawer away, then closes it after the transition has ended. It
   * is up to the owner of this component to differentiate between close and
   * cancel.
   */
  private async dismiss_(cancel: boolean) {
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
  protected onContainerClick_(event: Event) {
    event.stopPropagation();
  }

  /**
   * Close the dialog when tapped outside the container.
   */
  protected onDialogClick_() {
    this.cancel();
  }

  /**
   * Overrides the default cancel machanism to allow for a close animation.
   */
  protected onDialogCancel_(event: Event) {
    event.preventDefault();
    this.cancel();
  }

  protected onDialogClose_() {
    // Catch and re-fire the 'close' event such that it bubbles across Shadow
    // DOM v1.
    this.fire('close');
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-drawer': CrDrawerElement;
  }
}

customElements.define(CrDrawerElement.is, CrDrawerElement);
