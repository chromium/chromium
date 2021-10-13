/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Provides an interface {@link goog.soy.InjectedDataSupplier}
 * that users should implement to provide the injected data for a specific
 * application via goog.soy.renderer. The injected data format is a JavaScript
 * object:
 *
 * <pre>
 * {'dataKey': 'value', 'otherDataKey': 'otherValue'}
 * </pre>
 *
 * The injected data can then be referred to in any soy templates as
 * part of a magic "ij" parameter. For example, `$ij.dataKey`
 * will evaluate to 'value' with the above injected data.
 */

goog.module('goog.soy.InjectedDataSupplier');
goog.module.declareLegacyNamespace();

/**
 * An interface for a supplier that provides Soy injected data.
 * @interface
 */
exports = class InjectedDataSupplier {
  /**
   * Gets the injected data. Implementation may assume that
   * `goog.soy.Renderer` will treat the returned data as
   * immutable.  The renderer will call this every time one of its
   * `render*` methods is called.
   * @return {?} A key-value pair representing the injected data.
   */
  getData() {}
};
