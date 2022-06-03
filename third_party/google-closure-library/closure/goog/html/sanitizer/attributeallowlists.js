/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */


/**
 * @fileoverview Contains the attribute allowlists for use in the Html
 * sanitizer.
 */

goog.module('goog.html.sanitizer.attributeallowlists');
goog.module.declareLegacyNamespace();

/**
 * An allowlist for attributes that are always safe and allowed by default.
 * The sanitizer only applies whitespace trimming to these.
 * @const @dict {boolean}
 */
const AllowedAttributes = {
  '* ARIA-CHECKED': true,
  '* ARIA-COLCOUNT': true,
  '* ARIA-COLINDEX': true,
  '* ARIA-CONTROLS': true,
  '* ARIA-DESCRIBEDBY': true,
  '* ARIA-DISABLED': true,
  '* ARIA-EXPANDED': true,
  '* ARIA-GOOG-EDITABLE': true,
  '* ARIA-HASPOPUP': true,
  '* ARIA-HIDDEN': true,
  '* ARIA-LABEL': true,
  '* ARIA-LABELLEDBY': true,
  '* ARIA-MULTILINE': true,
  '* ARIA-MULTISELECTABLE': true,
  '* ARIA-ORIENTATION': true,
  '* ARIA-PLACEHOLDER': true,
  '* ARIA-READONLY': true,
  '* ARIA-REQUIRED': true,
  '* ARIA-ROLEDESCRIPTION': true,
  '* ARIA-ROWCOUNT': true,
  '* ARIA-ROWINDEX': true,
  '* ARIA-SELECTED': true,
  '* ABBR': true,
  '* ACCEPT': true,
  '* ACCESSKEY': true,
  '* ALIGN': true,
  '* ALT': true,
  '* AUTOCOMPLETE': true,
  '* AXIS': true,
  '* BGCOLOR': true,
  '* BORDER': true,
  '* CELLPADDING': true,
  '* CELLSPACING': true,
  '* CHAROFF': true,
  '* CHAR': true,
  '* CHECKED': true,
  '* CLEAR': true,
  '* COLOR': true,
  '* COLSPAN': true,
  '* COLS': true,
  '* COMPACT': true,
  '* COORDS': true,
  '* DATETIME': true,
  '* DIR': true,
  '* DISABLED': true,
  '* ENCTYPE': true,
  '* FACE': true,
  '* FRAME': true,
  '* HEIGHT': true,
  '* HREFLANG': true,
  '* HSPACE': true,
  '* ISMAP': true,
  '* LABEL': true,
  '* LANG': true,
  '* MAX': true,
  '* MAXLENGTH': true,
  '* METHOD': true,
  '* MULTIPLE': true,
  '* NOHREF': true,
  '* NOSHADE': true,
  '* NOWRAP': true,
  '* OPEN': true,
  '* READONLY': true,
  '* REQUIRED': true,
  '* REL': true,
  '* REV': true,
  '* ROLE': true,
  '* ROWSPAN': true,
  '* ROWS': true,
  '* RULES': true,
  '* SCOPE': true,
  '* SELECTED': true,
  '* SHAPE': true,
  '* SIZE': true,
  '* SPAN': true,
  '* START': true,
  '* SUMMARY': true,
  '* TABINDEX': true,
  '* TITLE': true,
  '* TYPE': true,
  '* VALIGN': true,
  '* VALUE': true,
  '* VSPACE': true,
  '* WIDTH': true
};
exports.AllowedAttributes = AllowedAttributes;

/**
 * An allowlist for attributes that are not safe to allow unrestricted, but are
 * made safe by default policies installed by the sanitizer in
 * goog.html.sanitizer.HtmlSanitizer.Builder.prototype.build, and thus allowed
 * by default under these policies.
 * @const @dict {boolean}
 */
const SanitizedAttributeAllowlist = {

  // Attributes which can contain URL fragments
  '* USEMAP': true,
  // Attributes which can contain URLs
  '* ACTION': true,
  '* CITE': true,
  '* HREF': true,
  // Attributes which can cause network requests
  '* LONGDESC': true,
  '* SRC': true,
  'LINK HREF': true,
  // Prevents clobbering
  '* FOR': true,
  '* HEADERS': true,
  '* NAME': true,
  // Controls where a window is opened. Prevents tab-nabbing
  'A TARGET': true,

  // Attributes which could cause UI redressing.
  '* CLASS': true,
  '* ID': true,

  // CSS style can cause network requests and XSSs
  '* STYLE': true
};
exports.SanitizedAttributeAllowlist = SanitizedAttributeAllowlist;
