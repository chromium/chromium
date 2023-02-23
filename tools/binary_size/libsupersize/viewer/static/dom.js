// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

/** Utilities for working with the DOM */
const dom = {
  /**
   * Creates a document fragment from the given nodes.
   * @param {Iterable<Node>} nodes
   * @return {DocumentFragment}
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
    this.divMetricsIcons =
        /** @type {!HTMLDivElement} */ (this.query('#div-metrics-icons'));

    /** @type {!HTMLTemplateElement} Template for groups in the symbol tree. */
    this.tmplSymbolTreeGroup = /** @type {!HTMLTemplateElement} */ (
        this.query('#tmpl-symbol-tree-group'));

    /** @type {!HTMLTemplateElement} Template for leaves in the symbol tree. */
    this.tmplSymbolTreeLeaf = /** @type {!HTMLTemplateElement} */ (
        this.query('#tmpl-symbol-tree-leaf'));

    /** @type {!HTMLSpanElement} */
    this.spanSizeHeader =
        /** @type {!HTMLSpanElement} */ (this.query('#span-size-header'));

    /** @type {!HTMLUListElement} */
    this.ulSymbolTree =
        /** @type {!HTMLUListElement} */ (this.query('#ul-symbol-tree'));

    /** @type {!HTMLTemplateElement} Template for groups in the metrics tree. */
    this.tmplMetricsTreeGroup = /** @type {!HTMLTemplateElement} */ (
        this.query('#tmpl-metrics-tree-group'));

    /** @type {!HTMLTemplateElement} Template for leaves in the metrics tree. */
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

    /** @public {!HTMLDivElement} */
    this.divMetadataView =
        /** @type {!HTMLDivElement} */ (this.query('#div-metadata-view'));

    /** @public {!HTMLPreElement} */
    this.preMetadataContent =
        /** @type {!HTMLPreElement} */ (this.query('#pre-metadata-content'));

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
