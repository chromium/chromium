/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.provide('goog.promise.Resolver');

goog.requireType('goog.Promise');



/**
 * Resolver interface for promises. The resolver is a convenience interface that
 * bundles the promise and its associated resolve and reject functions together,
 * for cases where the resolver needs to be persisted internally.
 *
 * @interface
 * @template TYPE
 */
goog.promise.Resolver = function() {};


/**
 * The promise that created this resolver.
 * @type {!goog.Promise<TYPE>}
 */
goog.promise.Resolver.prototype.promise;


/**
 * Resolves this resolver with the specified value.
 * @type {function((TYPE|goog.Promise<TYPE>|Thenable)=)}
 */
goog.promise.Resolver.prototype.resolve;


/**
 * Rejects this resolver with the specified reason.
 * @type {function(*=): void}
 */
goog.promise.Resolver.prototype.reject;
