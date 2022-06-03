/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Aliases `goog.editor.PluginImpl`.
 *
 * This is done to create a target for `goog.editor.PluginImpl` that also pulls
 * in `goog.editor.Field` without creating a cycle. Doing so allows downstream
 * targets to depend only on `goog.editor.Plugin` without js_library complaining
 * about unfullfilled forward declarations.
 */

goog.provide('goog.editor.Plugin');

/** @suppress {extraRequire} This is the whole point. */
goog.require('goog.editor.Field');
goog.require('goog.editor.PluginImpl');

/**
 * @constructor
 * @extends {goog.editor.PluginImpl}
 */
goog.editor.Plugin = goog.editor.PluginImpl;
