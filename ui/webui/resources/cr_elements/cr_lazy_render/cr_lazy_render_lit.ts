// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview
 * cr-lazy-render-lit helps with lazy rendering elements only when they are
 * actually needed (requested to be shown by the user). The lazy rendered
 * element is placed on the light DOM of cr-lazy-render-lit, such that it can
 * be fully styled by the parent, or use Lit bindings referring to the parent's
 * reactive properties.
 *
 * Example usage:
 *   <cr-lazy-render-lit id="menu"
 *       .template="${() => html`<heavy-menu></heavy-menu>`}">
 *   </cr-lazy-render-lit>
 *
 *   this.$.menu.get().show();
 */

import {assert} from '//resources/js/assert.js';
import {CrLitElement, html, render} from '//resources/lit/v3_0/lit.rollup.js';
import type {TemplateResult} from '//resources/lit/v3_0/lit.rollup.js';

export class CrLazyRenderLitElement<T extends HTMLElement> extends
    CrLitElement {
  static get is() {
    return 'cr-lazy-render-lit';
  }

  static override get properties() {
    return {
      template: {type: Object},

      rendered_: {
        type: Boolean,
        state: true,
      },
    };
  }

  private rendered_: boolean = false;

  template: () => TemplateResult = () => html``;
  private child_: T|null = null;

  override render() {
    if (this.rendered_) {
      // Render items into light DOM using the client provided template
      render(this.template(), this, {
        host: (this.getRootNode() as ShadowRoot).host,
      });
    }

    return html`<slot></slot>`;
  }

  /**
   * Stamp the template into the DOM tree synchronously
   * @return Child element which has been stamped into the DOM tree.
   */
  get(): T {
    if (!this.rendered_) {
      this.rendered_ = true;
      this.performUpdate();
      const slot = this.shadowRoot!.querySelector('slot');
      assert(slot);
      const slotted = slot.assignedElements();
      assert(slotted.length === 1);
      this.child_ = slotted[0] as T;
    }

    assert(this.child_);
    return this.child_;
  }

  /**
   * @return The element contained in the template, if it has
   *   already been stamped.
   */
  getIfExists(): (T|null) {
    return this.child_;
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'cr-lazy-render-lit': CrLazyRenderLitElement<HTMLElement>;
  }
}

customElements.define(CrLazyRenderLitElement.is, CrLazyRenderLitElement);
