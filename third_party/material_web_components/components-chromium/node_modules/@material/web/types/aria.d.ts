/**
 * @license
 * Copyright 2022 Google LLC
 * SPDX-License-Identifier: Apache-2.0
 *
 * @fileoverview Provides types for `ariaX` properties. These are required when
 * typing `ariaX` properties since lit analyzer requires strict aria string
 * types.
 */

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
