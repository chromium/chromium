// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/**
 * @fileoverview
 * Constants, utilities, and objects for DOM access.
 */

/** @enum {string} Keys in query string and names of input elements. */
const STATE_KEY = {
  LOAD_URL: 'load_url',
  BEFORE_URL: 'before_url',
  BYTE_UNIT: 'byteunit',
  METHOD_COUNT: 'method_count',
  MIN_SIZE: 'min_size',
  GROUP_BY: 'group_by',
  INCLUDE: 'include',
  EXCLUDE: 'exclude',
  TYPE: 'type',
  FLAG_FILTER: 'flag_filter',
  FOCUS: 'focus',
};

/** Utilities for working with the DOM */
const dom = {
  /**
   * Creates a document fragment from the given nodes.
   * @param {Iterable<Node>} nodes
   * @return {!DocumentFragment}
   */
  createFragment(nodes) {
    const fragment = document.createDocumentFragment();
    for (const node of nodes)
      fragment.appendChild(node);
    return fragment;
  },
  /**
   * Removes all the existing children of `parent` and inserts `newChild` in
   * their place.
   * @param {!Element} parent
   * @param {Node | null} newChild
   */
  replace(parent, newChild) {
    parent.innerHTML = '';
    if (newChild)
      parent.appendChild(newChild);
  },
  /**
   * Builds a text element in a single statement.
   * @param {string} tagName Type of the element, such as "span".
   * @param {string} text Text content for the element.
   * @param {string} [className] Class to apply to the element.
   */
  textElement(tagName, text, className) {
    const element = document.createElement(tagName);
    element.textContent = text;
    if (className)
      element.className = className;
    return element;
  },
  /**
   * Schedule a one-time |task| call on next animation frame when |node| is
   * added to the DOM, or if |node| is already in the DOM.
   * @param {!Node} node
   * @param {!function(): *} task
   */
  onNodeAdded(node, task) {
    if (document.contains(node)) {
      requestAnimationFrame(task);
      return;
    }
    let found = false;
    const observer = new MutationObserver((mutations) => {
      for (const mutation of mutations) {
        for (const node of mutation.addedNodes) {
          if (node.contains(node)) {
            observer.disconnect();
            found = true;
            requestAnimationFrame(task);
            return;
          }
        }
      }
    });
    observer.observe(document, {subtree: true, childList: true});
  },
};

/** Centralized object for element access. */
class MainElements {
  constructor() {
    /** @public {!NodeList} Elements that toggle body.show-options on click. */
    this.nlShowOptions =
        /** @type {!NodeList} */ (document.querySelectorAll('.toggle-options'));

    /** @public {!HTMLDivElement} */
    this.divReviewInfo =
        /** @type {!HTMLDivElement} */ (this.query('#div-review-info'));

    /** @type {!HTMLAnchorElement} */
    this.linkReviewText =
        /** @type {!HTMLAnchorElement} */ (this.query('#link-review-text'));

    /** @type {!HTMLAnchorElement} */
    this.linkDownloadBefore =
        /** @type {!HTMLAnchorElement} */ (this.query('#link-download-before'));

    /** @type {!HTMLAnchorElement} */
    this.linkDownloadLoad =
        /** @type {!HTMLAnchorElement} */ (this.query('#link-download-load'));

    /** @type {!HTMLInputElement} */
    this.fileUpload =
        /** @type {!HTMLInputElement} */ (this.query('#file-upload'));

    /** @type {!HTMLAnchorElement} */
    this.linkFaq =
        /** @type {!HTMLAnchorElement} */ (this.query('#link-faq'));

    /** @type {!HTMLProgressElement} */
    this.progAppbar =
        /** @type {!HTMLProgressElement} */ (this.query('#prog-appbar'));

    /** @public {!HTMLFormElement} Form with options and filters. */
    this.frmOptions =
        /** @type {!HTMLFormElement} */ (this.query('#frm-options'));

    /** @public {!HTMLInputElement} */
    this.cbMethodCount =
        /** @type {!HTMLInputElement} */ (this.query('#cb-method-count'));

    /** @public {!HTMLSelectElement} */
    this.selByteUnit =
        /** @type {!HTMLSelectElement} */ (this.query('#sel-byte-unit'));

    /** @public {!HTMLInputElement} */
    this.nbMinSize =
        /** @type {!HTMLInputElement} */ (this.query('#nb-min-size'));

    /** @public {!RadioNodeList} */
    this.rnlGroupBy = /** @type {!RadioNodeList} */ (
        this.frmOptions.elements.namedItem(STATE_KEY.GROUP_BY));
    assert(this.rnlGroupBy.length > 0);

    /** @public {!HTMLInputElement} */
    this.tbIncludeRegex =
        /** @type {!HTMLInputElement} */ (this.query('#tb-include-regex'));

    /** @public {!HTMLInputElement} */
    this.tbExcludeRegex =
        /** @type {!HTMLInputElement} */ (this.query('#tb-exclude-regex'));

    /** @public {!RadioNodeList} */
    this.rnlType = /** @type {!RadioNodeList} */ (
        this.frmOptions.elements.namedItem(STATE_KEY.TYPE));
    assert(this.rnlType.length > 0);

    /** @type {!HTMLFieldSetElement} */
    this.fsTypesFilter =
        /** @type {!HTMLFieldSetElement} */ (this.query('#fs-types-filter'));

    /** @type {!HTMLButtonElement} */
    this.btnTypeAll =
        /** @type {!HTMLButtonElement} */ (this.query('#btn-type-all'));

    /** @type {!HTMLButtonElement} */
    this.btnTypeNone =
        /** @type {!HTMLButtonElement} */ (this.query('#btn-type-none'));

    /** @public {!RadioNodeList} */
    this.rnlFlagFilter = /** @type {!RadioNodeList} */ (
        this.frmOptions.elements.namedItem(STATE_KEY.FLAG_FILTER));
    assert(this.rnlFlagFilter.length > 0);

    /** @public {!HTMLDivElement} */
    this.divIcons =
        /** @type {!HTMLDivElement} */ (this.query('#div-icons'));

    /** @public {!HTMLDivElement} */
    this.divDiffStatusIcons =
        /** @type {!HTMLDivElement} */ (this.query('#div-diff-status-icons'));

    /** @public {!HTMLDivElement} */
    this.divMiscIcons =
        /** @type {!HTMLDivElement} */ (this.query('#div-misc-icons'));

    /** @type {!HTMLTemplateElement} Template for groups in the Symbol Tree. */
    this.tmplSymbolTreeGroup = /** @type {!HTMLTemplateElement} */ (
        this.query('#tmpl-symbol-tree-group'));

    /** @type {!HTMLTemplateElement} Template for leaves in the Symbol Tree. */
    this.tmplSymbolTreeLeaf = /** @type {!HTMLTemplateElement} */ (
        this.query('#tmpl-symbol-tree-leaf'));

    /** @type {!HTMLSpanElement} */
    this.spanSizeHeader =
        /** @type {!HTMLSpanElement} */ (this.query('#span-size-header'));

    /** @type {!HTMLUListElement} */
    this.ulSymbolTree =
        /** @type {!HTMLUListElement} */ (this.query('#ul-symbol-tree'));

    /** @type {!HTMLTemplateElement} Template for groups in the Metrics Tree. */
    this.tmplMetricsTreeGroup = /** @type {!HTMLTemplateElement} */ (
        this.query('#tmpl-metrics-tree-group'));

    /** @type {!HTMLTemplateElement} Template for leaves in the Metrics Tree. */
    this.tmplMetricsTreeLeaf = /** @type {!HTMLTemplateElement} */ (
        this.query('#tmpl-metrics-tree-leaf'));

    /** @public {!HTMLDivElement} */
    this.divMetricsView =
        /** @type {!HTMLDivElement} */ (this.query('#div-metrics-view'));

    /** @type {!HTMLUListElement} */
    this.ulMetricsTree =
        /** @type {!HTMLUListElement} */ (this.query('#ul-metrics-tree'));

    /** @public {!HTMLDivElement} */
    this.divNoSymbolsMsg =
        /** @type {!HTMLDivElement} */ (this.query('#div-no-symbols-msg'));

    /**
     * @type {!HTMLTemplateElement} Template for groups in the Metadata Tree.
     */
    this.tmplMetadataTreeGroup = /** @type {!HTMLTemplateElement} */ (
        this.query('#tmpl-metadata-tree-group'));

    /**
     * @type {!HTMLTemplateElement} Template for leaves in the Metadata Tree.
     */
    this.tmplMetadataTreeLeaf = /** @type {!HTMLTemplateElement} */ (
        this.query('#tmpl-metadata-tree-leaf'));

    /** @public {!HTMLDivElement} */
    this.divMetadataView =
        /** @type {!HTMLDivElement} */ (this.query('#div-metadata-view'));

    /** @type {!HTMLUListElement} */
    this.ulMetadataTree =
        /** @type {!HTMLUListElement} */ (this.query('#ul-metadata-tree'));

    /** @public {!HTMLDivElement} */
    this.divInfocardArtifact =
        /** @type {!HTMLDivElement} */ (this.query('#div-infocard-artifact'));

    /** @public {!HTMLDivElement} */
    this.divInfocardSymbol =
        /** @type {!HTMLDivElement} */ (this.query('#div-infocard-symbol'));

    /** @public {!HTMLDivElement} */
    this.divSigninModal =
        /** @type {!HTMLDivElement} */ (this.query('#div-signin-modal'));

    /** @public {!HTMLDivElement} */
    this.divDisassemblyModal =
        /** @type {!HTMLDivElement} */ (this.query('#div-disassembly-modal'));
  }

  /**
   * @param {string} q Query string.
   * @return {!Element}
   * @private
   */
  query(q) {
    return /** @type {!Element} */ (assertNotNull(document.querySelector(q)));
  }

  /**
   * @param {!Element} elt
   * @return {!Element}
   * @public
   */
  getAriaDescribedBy(elt) {
    const id = assertNotNull(elt.getAttribute('aria-describedby'));
    return assertNotNull(document.getElementById(id));
  }
}

/** @const {!MainElements} */
const g_el = new MainElements();
