// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {CrButtonElement} from 'chrome://resources/cr_elements/cr_button/cr_button.js';
import {CrCheckboxElement} from 'chrome://resources/cr_elements/cr_checkbox/cr_checkbox.js';
import {CrDialogElement} from 'chrome://resources/cr_elements/cr_dialog/cr_dialog.js';

import {AsyncQueue} from '../common/js/async_util.js';
import {strf} from '../common/js/util.js';

import {getTemplate} from './xf_conflict_dialog.html.js';

/**
 * Files Conflict Dialog: if the target file of a copy/move operation exists,
 * the conflict dialog can be used to ask the user what to do in that case.
 *
 * The user can choose to 'cancel' the copy/move operation by cancelling the
 * dialog. Otherwise, the user can choose to 'replace' the file or 'keepboth'
 * to keep the file.
 */
export class XfConflictDialog extends HTMLElement {
  /**
   * Mutex used to serialize conflict modal dialog use.
   */
  private mutex_ = new AsyncQueue();

  /**
   * Modal dialog element.
   */
  private dialog_: CrDialogElement;

  /**
   * Either 'keepboth' or 'replace' on dialog success, or an empty string if
   * the dialog was cancelled.
   */
  private action_: string = '';

  /**
   * Returns dialog success result using the Promise.resolve method.
   */
  private resolve_: (result: ConflictResult) => void;

  /**
   * Returns dialog cancelled error using the Promise.reject method.
   */
  private reject_: (error: Error) => void;

  /**
   * Construct.
   */
  constructor() {
    super();

    // Create element content.
    const template = document.createElement('template');
    template.innerHTML = getTemplate() as unknown as string;
    const fragment = template.content.cloneNode(true);
    this.attachShadow({mode: 'open'}).appendChild(fragment);

    this.dialog_ = this.getDialogElement();
    this.resolve_ = console.log;
    this.reject_ = console.log;
  }

  /**
   * DOM connected callback.
   */
  connectedCallback() {
    this.dialog_.addEventListener('close', this.onClose_.bind(this));

    this.getCancelButton().onclick = this.cancel_.bind(this);
    this.getKeepbothButton().onclick = this.keepboth_.bind(this);
    this.getReplaceButton().onclick = this.replace_.bind(this);
  }

  /**
   * Open the modal dialog to ask the user to resolve a conflict for the given
   * |filename|. Set |checkbox| true to display the 'Apply to all' checkbox.
   */
  async show(filename: string, checkbox?: boolean): Promise<ConflictResult> {
    const unlock = await this.mutex_.lock();
    try {
      return await new Promise<ConflictResult>((resolve, reject) => {
        this.resolve_ = resolve;
        this.reject_ = reject;
        this.showModal_(filename, !!checkbox);
      });
    } finally {
      unlock();
    }
  }

  /**
   * Resets the dialog message content for the given |filename| and |checkbox|
   * display state, and then shows the modal dialog.
   */
  private showModal_(filename: string, checkbox: boolean) {
    const message = strf('CONFLICT_DIALOG_MESSAGE', filename);
    this.getMessageElement().innerText = message;

    const applyToAll = this.getCheckboxElement();
    applyToAll.hidden = !checkbox;
    applyToAll.checked = false;

    this.action_ = '';
    this.dialog_.showModal();
  }

  /**
   * Returns 'dialog' element.
   */
  getDialogElement(): CrDialogElement {
    return this.shadowRoot!.querySelector('#conflict-dialog')!;
  }

  /**
   * Returns 'message' element.
   */
  getMessageElement(): HTMLElement {
    return this.dialog_.querySelector('#message')!;
  }

  /**
   * Returns 'Apply to all' checkbox element.
   */
  getCheckboxElement(): CrCheckboxElement {
    return this.shadowRoot!.querySelector('#checkbox')!;
  }

  /**
   * Returns 'cancel' button element.
   */
  getCancelButton(): CrButtonElement {
    return this.shadowRoot!.querySelector('#cancel')!;
  }

  /**
   * Dialog was cancelled.
   */
  private cancel_() {
    this.action_ = '';
    this.dialog_.close();
  }

  /**
   * Returns 'keepboth' button element.
   */
  getKeepbothButton(): CrButtonElement {
    return this.shadowRoot!.querySelector('#keepboth')!;
  }

  /**
   * Dialog 'keepboth' button was clicked.
   */
  private keepboth_() {
    this.action_ = ConflictResolveType.KEEPBOTH;
    this.dialog_.close();
  }

  /*
   * Returns 'replace' button element.
   */
  getReplaceButton(): CrButtonElement {
    return this.shadowRoot!.querySelector('#replace')!;
  }

  /**
   * Dialog 'replace' button was clicked.
   */
  private replace_() {
    this.action_ = ConflictResolveType.REPLACE;
    this.dialog_.close();
  }

  /*
   * Triggered by <cr-dialog>.close(): reject the Promise if the dialog was
   * cancelled, or resolve the Promise with the dialog result.
   */
  private onClose_() {
    if (!this.action_) {
      this.reject_(new Error('dialog cancelled'));
      return;
    }

    const applyToAll = this.getCheckboxElement().checked;
    this.resolve_({
      resolve: this.action_,  // Either 'keepboth' or 'replace'.
      checked: applyToAll,    // True or False.
    } as ConflictResult);
  }
}

export interface ConflictResult {
  resolve: ConflictResolveType;
  checked: boolean;
}

export const enum ConflictResolveType {
  KEEPBOTH = 'keepboth',
  REPLACE = 'replace',
}

declare global {
  interface HTMLElementTagNameMap {
    'xf-conflict-dialog': XfConflictDialog;
  }
}

customElements.define('xf-conflict-dialog', XfConflictDialog);
