// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import type {CrButtonElement} from 'chrome://resources/ash/common/cr_elements/cr_button/cr_button.js';
import type {CrCheckboxElement} from 'chrome://resources/ash/common/cr_elements/cr_checkbox/cr_checkbox.js';
import type {CrDialogElement} from 'chrome://resources/ash/common/cr_elements/cr_dialog/cr_dialog.js';

import {AsyncQueue} from '../common/js/async_util.js';
import {str, strf} from '../common/js/translations.js';

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
    this.resolve_ = console.info;
    this.reject_ = console.info;
  }

  /**
   * DOM connected callback.
   */
  connectedCallback() {
    this.dialog_.addEventListener('close', this.closed_.bind(this));

    this.getCheckboxElement().onchange = this.checked_.bind(this);
    this.getCancelButton().onclick = this.cancel_.bind(this);
    this.getKeepbothButton().onclick = this.keepboth_.bind(this);
    this.getReplaceButton().onclick = this.replace_.bind(this);
  }

  /**
   * Open the modal dialog to ask the user to resolve a conflict for the given
   * |filename|. The default parameters after |filename| are as follows:
   *
   * Set |checkbox| true to display the 'Apply to all' checkbox in the dialog,
   *   and should be set true if there are potentially, multiple file names in
   *   a copy or move operation that conflict. The default is false.
   *
   * Set |directory| true if the |filename| is a directory (aka a folder). The
   *   default is false.
   */
  async show(
      filename: string, checkbox: boolean = false,
      directory: boolean = false): Promise<ConflictResult> {
    const unlock = await this.mutex_.lock();
    try {
      return await new Promise<ConflictResult>((resolve, reject) => {
        this.resolve_ = resolve;
        this.reject_ = reject;
        this.showModal_(filename, checkbox, directory);
      });
    } finally {
      unlock();
    }
  }

  /**
   * Resets the dialog for the given |filename| |checkbox| and |folder| values
   * and then shows the modal dialog.
   */
  private showModal_(filename: string, checkbox: boolean, folder: boolean) {
    const message =  // 'A folder named ...' or 'A file named ...'
        folder ? 'CONFLICT_DIALOG_FOLDER_MESSAGE' : 'CONFLICT_DIALOG_MESSAGE';
    this.getMessageElement().innerText = strf(message, filename);

    const applyToAll = this.getCheckboxElement();
    applyToAll.hidden = !checkbox;
    applyToAll.checked = false;

    this.action_ = '';
    this.checked_();
    this.dialog_.showModal();
    this.setFocus_();
  }

  /**
   * The conflict dialog has no title. Remove the <cr-dialog> title child that
   * would focus, remove its <dialog> aria-labelledby and aria-describedby, so
   * ARIA announces the #message (that is the initial focus) once.
   *
   * Per https://w3c.github.io/aria-practices/#dialog_roles_states_props, adds
   * aria-modal='true' meaning all content outside the <dialog> is inert.
   */
  private setFocus_() {
    this.dialog_.shadowRoot!.querySelector('#title')?.remove();

    const element = this.getHtmlDialogElement();
    element.setAttribute('aria-modal', 'true');
    element.removeAttribute('aria-labelledby');
    element.removeAttribute('aria-describedby');

    this.getMessageElement().focus();
  }

  /**
   * Returns 'dialog' element.
   */
  getDialogElement(): CrDialogElement {
    return this.shadowRoot!.querySelector('#conflict-dialog')!;
  }

  /**
   * Returns 'dialog' <dialog> element.
   */
  getHtmlDialogElement(): HTMLDialogElement {
    return this.dialog_.getNative()!;
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
   * 'Apply to all' checkbox value changed.
   */
  private checked_() {
    const checked = this.getCheckboxElement().checked;

    if (checked) {
      this.getKeepbothButton().innerText = str('CONFLICT_DIALOG_KEEP_ALL');
      this.getReplaceButton().innerText = str('CONFLICT_DIALOG_REPLACE_ALL');
    } else {
      this.getKeepbothButton().innerText = str('CONFLICT_DIALOG_KEEP_BOTH');
      this.getReplaceButton().innerText = str('CONFLICT_DIALOG_REPLACE');
    }

    this.getCheckboxElement().focus();
    this.toggleAttribute('checked', checked);
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
   * Triggered by the modal dialog close(): rejects the Promise if the dialog
   * was cancelled or resolves it with the dialog result.
   */
  private closed_() {
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

export enum ConflictResolveType {
  KEEPBOTH = 'keepboth',
  REPLACE = 'replace',
}

declare global {
  interface HTMLElementTagNameMap {
    'xf-conflict-dialog': XfConflictDialog;
  }
}

customElements.define('xf-conflict-dialog', XfConflictDialog);
