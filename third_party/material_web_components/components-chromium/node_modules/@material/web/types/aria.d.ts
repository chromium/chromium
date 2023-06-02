/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 */
/**
 * @fileoverview Provides types for `ariaX` properties. These are required when
 * typing `ariaX` properties since lit analyzer requires strict aria string
 * types.
 */

/**
 * An extension of `ARIAMixin` that enforces strict value types for aria
 * properties.
 *
 * This is needed for correct typing in render functions with lit analyzer.
 *
 * @example
 * render() {
 *   const {ariaLabel} = this as ARIAMixinStrict;
 *   return html`
 *     <button aria-label=${ariaLabel || nothing}>
 *       <slot></slot>
 *     </button>
 *   `;
 * }
 */
export interface ARIAMixinStrict extends ARIAMixin {
  ariaAtomic: 'true'|'false'|null;
  ariaAutoComplete: 'none'|'inline'|'list'|'both'|null;
  ariaBusy: 'true'|'false'|null;
  ariaChecked: 'true'|'false'|null;
  ariaColCount: `${number}`|null;
  ariaColIndex: `${number}`|null;
  ariaColSpan: `${number}`|null;
  ariaCurrent: 'page'|'step'|'location'|'date'|'time'|'true'|'false'|null;
  ariaDisabled: 'true'|'false'|null;
  ariaExpanded: 'true'|'false'|null;
  ariaHasPopup: 'false'|'true'|'menu'|'listbox'|'tree'|'grid'|'dialog'|null;
  ariaHidden: 'true'|'false'|null;
  ariaInvalid: 'true'|'false'|null;
  ariaKeyShortcuts: string|null;
  ariaLabel: string|null;
  ariaLevel: `${number}`|null;
  ariaLive: 'assertive'|'off'|'polite'|null;
  ariaModal: 'true'|'false'|null;
  ariaMultiLine: 'true'|'false'|null;
  ariaMultiSelectable: 'true'|'false'|null;
  ariaOrientation: 'horizontal'|'vertical'|'undefined'|null;
  ariaPlaceholder: string|null;
  ariaPosInSet: `${number}`|null;
  ariaPressed: 'true'|'false'|null;
  ariaReadOnly: 'true'|'false'|null;
  ariaRequired: 'true'|'false'|null;
  ariaRoleDescription: string|null;
  ariaRowCount: `${number}`|null;
  ariaRowIndex: `${number}`|null;
  ariaRowSpan: `${number}`|null;
  ariaSelected: 'true'|'false'|null;
  ariaSetSize: `${number}`|null;
  ariaSort: 'ascending'|'descending'|'none'|'other'|null;
  ariaValueMax: `${number}`|null;
  ariaValueMin: `${number}`|null;
  ariaValueNow: `${number}`|null;
  ariaValueText: string|null;
  role: ARIARole|null;
}

/**
 * Valid values for `aria-expanded`.
 */
export type ARIAAutoComplete = 'none'|'inline'|'list'|'both';

/**
 * Valid values for `aria-expanded`.
 */
export type ARIAExpanded = 'true'|'false';

/**
 * Valid values for `aria-haspopup`.
 */
export type ARIAHasPopup =
    'false'|'true'|'menu'|'listbox'|'tree'|'grid'|'dialog';

/**
 * Valid values for `role`.
 */
export type ARIARole =
    'alert'|'alertdialog'|'button'|'checkbox'|'dialog'|'gridcell'|'link'|'log'|
    'marquee'|'menuitem'|'menuitemcheckbox'|'menuitemradio'|'option'|
    'progressbar'|'radio'|'scrollbar'|'searchbox'|'slider'|'spinbutton'|
    'status'|'switch'|'tab'|'tabpanel'|'textbox'|'timer'|'tooltip'|'treeitem'|
    'combobox'|'grid'|'listbox'|'menu'|'menubar'|'radiogroup'|'tablist'|'tree'|
    'treegrid'|'application'|'article'|'cell'|'columnheader'|'definition'|
    'directory'|'document'|'feed'|'figure'|'group'|'heading'|'img'|'list'|
    'listitem'|'math'|'none'|'note'|'presentation'|'region'|'row'|'rowgroup'|
    'rowheader'|'separator'|'table'|'term'|'text'|'toolbar'|'banner'|
    'complementary'|'contentinfo'|'form'|'main'|'navigation'|'region'|'search'|
    'doc-abstract'|'doc-acknowledgments'|'doc-afterword'|'doc-appendix'|
    'doc-backlink'|'doc-biblioentry'|'doc-bibliography'|'doc-biblioref'|
    'doc-chapter'|'doc-colophon'|'doc-conclusion'|'doc-cover'|'doc-credit'|
    'doc-credits'|'doc-dedication'|'doc-endnote'|'doc-endnotes'|'doc-epigraph'|
    'doc-epilogue'|'doc-errata'|'doc-example'|'doc-footnote'|'doc-foreword'|
    'doc-glossary'|'doc-glossref'|'doc-index'|'doc-introduction'|'doc-noteref'|
    'doc-notice'|'doc-pagebreak'|'doc-pagelist'|'doc-part'|'doc-preface'|
    'doc-prologue'|'doc-pullquote'|'doc-qna'|'doc-subtitle'|'doc-tip'|'doc-toc';
