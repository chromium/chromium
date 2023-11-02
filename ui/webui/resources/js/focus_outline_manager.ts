// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * The class name to set on the document element.
 */
const CLASS_NAME = 'focus-outline-visible';

const docsToManager: Map<Document, FocusOutlineManager> = new Map();

/**
 * This class sets a CSS class name on the HTML element of |doc| when the user
 * presses a key. It removes the class name when the user clicks anywhere.
 *
 * This allows you to write CSS like this:
 *
 * html.focus-outline-visible my-element:focus {
 *   outline: 5px auto -webkit-focus-ring-color;
 * }
 *
 * And the outline will only be shown if the user uses the keyboard to get to
 * it.
 *
 */
export class FocusOutlineManager {
  // Whether focus change is triggered by a keyboard event.
  private focusByKeyboard_: boolean = true;
  private classList_: DOMTokenList;

  /**
   * @param doc The document to attach the focus outline manager to.
   */
  constructor(doc: Document) {
    this.classList_ = doc.documentElement.classList;

    doc.addEventListener('keydown', () => this.onEvent_(true), true);
    doc.addEventListener('mousedown', () => this.onEvent_(false), true);

    this.updateVisibility();
  }

  private onEvent_(focusByKeyboard: boolean) {
    if (this.focusByKeyboard_ === focusByKeyboard) {
      return;
    }
    this.focusByKeyboard_ = focusByKeyboard;
    this.updateVisibility();
  }

  updateVisibility() {
    this.visible = this.focusByKeyboard_;
  }

  /**
   * Whether the focus outline should be visible.
   */
  set visible(visible: boolean) {
    this.classList_.toggle(CLASS_NAME, visible);
  }

  get visible(): boolean {
    return this.classList_.contains(CLASS_NAME);
  }

  /**
   * Gets a per document singleton focus outline manager.
   * @param doc The document to get the |FocusOutlineManager| for.
   * @return The per document singleton focus outline manager.
   */
  static forDocument(doc: Document): FocusOutlineManager {
    let manager = docsToManager.get(doc);
    if (!manager) {
      manager = new FocusOutlineManager(doc);
      docsToManager.set(doc, manager);
    }
    return manager;
  }
}
