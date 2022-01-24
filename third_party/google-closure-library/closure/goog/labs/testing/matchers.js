/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @fileoverview Provides various matcher methods.
 */

goog.provide('goog.labs.testing');
goog.provide('goog.labs.testing.AllOfMatcher');
goog.provide('goog.labs.testing.AnyNumberMatcher');
goog.provide('goog.labs.testing.AnyObjectMatcher');
goog.provide('goog.labs.testing.AnyOfMatcher');
goog.provide('goog.labs.testing.AnyStringMatcher');
goog.provide('goog.labs.testing.AnythingMatcher');
goog.provide('goog.labs.testing.CloseToMatcher');
goog.provide('goog.labs.testing.ContainsStringMatcher');
goog.provide('goog.labs.testing.EndsWithMatcher');
goog.provide('goog.labs.testing.EqualToIgnoringWhitespaceMatcher');
goog.provide('goog.labs.testing.EqualToMatcher');
goog.provide('goog.labs.testing.EqualsMatcher');
goog.provide('goog.labs.testing.GreaterThanEqualToMatcher');
goog.provide('goog.labs.testing.GreaterThanMatcher');
goog.provide('goog.labs.testing.HasEntriesMatcher');
goog.provide('goog.labs.testing.HasEntryMatcher');
goog.provide('goog.labs.testing.HasKeyMatcher');
goog.provide('goog.labs.testing.HasPropertyMatcher');
goog.provide('goog.labs.testing.HasValueMatcher');
goog.provide('goog.labs.testing.InstanceOfMatcher');
goog.provide('goog.labs.testing.IsNotMatcher');
goog.provide('goog.labs.testing.IsNullMatcher');
goog.provide('goog.labs.testing.IsNullOrUndefinedMatcher');
goog.provide('goog.labs.testing.IsUndefinedMatcher');
goog.provide('goog.labs.testing.LessThanEqualToMatcher');
goog.provide('goog.labs.testing.LessThanMatcher');
goog.provide('goog.labs.testing.ObjectEqualsMatcher');
goog.provide('goog.labs.testing.RegexMatcher');
goog.provide('goog.labs.testing.StartsWithMatcher');
goog.provide('goog.labs.testing.StringContainsInOrderMatcher');

goog.require('goog.labs.testing.decoratormatcher');
goog.require('goog.labs.testing.dictionarymatcher');
goog.require('goog.labs.testing.logicmatcher');
goog.require('goog.labs.testing.numbermatcher');
goog.require('goog.labs.testing.objectmatcher');
goog.require('goog.labs.testing.stringmatcher');

/**
 * @const
 */
goog.labs.testing.AnythingMatcher =
    goog.labs.testing.decoratormatcher.AnythingMatcher;

/**
 * @const
 */
goog.labs.testing.HasEntriesMatcher =
    goog.labs.testing.dictionarymatcher.HasEntriesMatcher;

/**
 * @const
 */
goog.labs.testing.HasEntryMatcher =
    goog.labs.testing.dictionarymatcher.HasEntryMatcher;

/**
 * @const
 */
goog.labs.testing.HasKeyMatcher =
    goog.labs.testing.dictionarymatcher.HasKeyMatcher;

/**
 * @const
 */
goog.labs.testing.HasValueMatcher =
    goog.labs.testing.dictionarymatcher.HasValueMatcher;

/**
 * @const
 */
goog.labs.testing.AllOfMatcher = goog.labs.testing.logicmatcher.AllOfMatcher;

/**
 * @const
 */
goog.labs.testing.AnyOfMatcher = goog.labs.testing.logicmatcher.AnyOfMatcher;

/**
 * @const
 */
goog.labs.testing.IsNotMatcher = goog.labs.testing.logicmatcher.IsNotMatcher;

/**
 * @const
 */
goog.labs.testing.AnyNumberMatcher =
    goog.labs.testing.numbermatcher.AnyNumberMatcher;

/**
 * @const
 */
goog.labs.testing.CloseToMatcher =
    goog.labs.testing.numbermatcher.CloseToMatcher;

/**
 * @const
 */
goog.labs.testing.EqualToMatcher =
    goog.labs.testing.numbermatcher.EqualToMatcher;

/**
 * @const
 */
goog.labs.testing.GreaterThanEqualToMatcher =
    goog.labs.testing.numbermatcher.GreaterThanEqualToMatcher;

/**
 * @const
 */
goog.labs.testing.GreaterThanMatcher =
    goog.labs.testing.numbermatcher.GreaterThanMatcher;

/**
 * @const
 */
goog.labs.testing.LessThanEqualToMatcher =
    goog.labs.testing.numbermatcher.LessThanEqualToMatcher;

/**
 * @const
 */
goog.labs.testing.LessThanMatcher =
    goog.labs.testing.numbermatcher.LessThanMatcher;

/**
 * @const
 */
goog.labs.testing.AnyObjectMatcher =
    goog.labs.testing.objectmatcher.AnyObjectMatcher;

/**
 * @const
 */
goog.labs.testing.HasPropertyMatcher =
    goog.labs.testing.objectmatcher.HasPropertyMatcher;

/**
 * @const
 */
goog.labs.testing.InstanceOfMatcher =
    goog.labs.testing.objectmatcher.InstanceOfMatcher;

/**
 * @const
 */
goog.labs.testing.IsNullMatcher = goog.labs.testing.objectmatcher.IsNullMatcher;

/**
 * @const
 */
goog.labs.testing.IsNullOrUndefinedMatcher =
    goog.labs.testing.objectmatcher.IsNullOrUndefinedMatcher;

/**
 * @const
 */
goog.labs.testing.IsUndefinedMatcher =
    goog.labs.testing.objectmatcher.IsUndefinedMatcher;

/**
 * @const
 */
goog.labs.testing.ObjectEqualsMatcher =
    goog.labs.testing.objectmatcher.ObjectEqualsMatcher;

/**
 * @const
 */
goog.labs.testing.AnyStringMatcher =
    goog.labs.testing.stringmatcher.AnyStringMatcher;

/**
 * @const
 */
goog.labs.testing.ContainsStringMatcher =
    goog.labs.testing.stringmatcher.ContainsStringMatcher;

/**
 * @const
 */
goog.labs.testing.EndsWithMatcher =
    goog.labs.testing.stringmatcher.EndsWithMatcher;

/**
 * @const
 */
goog.labs.testing.EqualToIgnoringWhitespaceMatcher =
    goog.labs.testing.stringmatcher.EqualToIgnoringWhitespaceMatcher;

/**
 * @const
 */
goog.labs.testing.EqualsMatcher = goog.labs.testing.stringmatcher.EqualsMatcher;

/**
 * @const
 */
goog.labs.testing.RegexMatcher = goog.labs.testing.stringmatcher.RegexMatcher;

/**
 * @const
 */
goog.labs.testing.StartsWithMatcher =
    goog.labs.testing.stringmatcher.StartsWithMatcher;

/**
 * @const
 */
goog.labs.testing.StringContainsInOrderMatcher =
    goog.labs.testing.stringmatcher.StringContainsInOrderMatcher;

// Globally-defined matchers

/**
 * @const
 */
var anything = goog.labs.testing.decoratormatcher.AnythingMatcher.anything;

/**
 * @const
 */
var describedAs =
    goog.labs.testing.decoratormatcher.AnythingMatcher.describedAs;

/**
 * @const
 */
var is = goog.labs.testing.decoratormatcher.AnythingMatcher.is;

/**
 * @const
 */
var hasEntries =
    goog.labs.testing.dictionarymatcher.HasEntriesMatcher.hasEntries;

/**
 * @const
 */
var hasEntry = goog.labs.testing.dictionarymatcher.HasEntryMatcher.hasEntry;

/**
 * @const
 */
var hasKey = goog.labs.testing.dictionarymatcher.HasKeyMatcher.hasKey;

/**
 * @const
 */
var hasValue = goog.labs.testing.dictionarymatcher.HasValueMatcher.hasValue;

/**
 * @const
 */
var allOf = goog.labs.testing.logicmatcher.AllOfMatcher.allOf;

/**
 * @const
 */
var anyOf = goog.labs.testing.logicmatcher.AnyOfMatcher.anyOf;

/**
 * @const
 */
var isNot = goog.labs.testing.logicmatcher.IsNotMatcher.isNot;

/**
 * @const
 */
var anyNumber = goog.labs.testing.numbermatcher.AnyNumberMatcher.anyNumber;

/**
 * @const
 */
var closeTo = goog.labs.testing.numbermatcher.CloseToMatcher.closeTo;

/**
 * @const
 */
var equalTo = goog.labs.testing.numbermatcher.EqualToMatcher.equalTo;

/**
 * @const
 */
var greaterThanEqualTo = goog.labs.testing.numbermatcher
                             .GreaterThanEqualToMatcher.greaterThanEqualTo;

/**
 * @const
 */
var greaterThan =
    goog.labs.testing.numbermatcher.GreaterThanMatcher.greaterThan;

/**
 * @const
 */
var lessThanEqualTo =
    goog.labs.testing.numbermatcher.LessThanEqualToMatcher.lessThanEqualTo;

/**
 * @const
 */
var lessThan = goog.labs.testing.numbermatcher.LessThanMatcher.lessThan;

/**
 * @const
 */
var anyObject = goog.labs.testing.objectmatcher.AnyObjectMatcher.anyObject;

/**
 * @const
 */
var hasProperty =
    goog.labs.testing.objectmatcher.HasPropertyMatcher.hasProperty;

/**
 * @const
 */
var instanceOfClass =
    goog.labs.testing.objectmatcher.InstanceOfMatcher.instanceOfClass;

/**
 * @const
 */
var isNull = goog.labs.testing.objectmatcher.IsNullMatcher.isNull;

/**
 * @const
 */
var isNullOrUndefined =
    goog.labs.testing.objectmatcher.IsNullOrUndefinedMatcher.isNullOrUndefined;

/**
 * @const
 */
var isUndefined =
    goog.labs.testing.objectmatcher.IsUndefinedMatcher.isUndefined;

/**
 * @const
 */
var equalsObject =
    goog.labs.testing.objectmatcher.ObjectEqualsMatcher.equalsObject;

/**
 * @const
 */
var anyString = goog.labs.testing.stringmatcher.AnyStringMatcher.anyString;

/**
 * @const
 */
var containsString =
    goog.labs.testing.stringmatcher.ContainsStringMatcher.containsString;

/**
 * @const
 */
var endsWith = goog.labs.testing.stringmatcher.EndsWithMatcher.endsWith;

/**
 * @const
 */
var equalToIgnoringWhitespace =
    goog.labs.testing.stringmatcher.EqualToIgnoringWhitespaceMatcher
        .equalToIgnoringWhitespace;

/**
 * @const
 */
var equals = goog.labs.testing.stringmatcher.EqualsMatcher.equals;

/**
 * @const
 */
var matchesRegex = goog.labs.testing.stringmatcher.RegexMatcher.matchesRegex;

/**
 * @const
 */
var startsWith = goog.labs.testing.stringmatcher.StartsWithMatcher.startsWith;

/**
 * @const
 */
var stringContainsInOrder =
    goog.labs.testing.stringmatcher.StringContainsInOrderMatcher
        .stringContainsInOrder;
