// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Externs generated from namespace: echoPrivate */

/**
 * @const
 */
chrome.echoPrivate = {};

/**
 * Sets the offer info in Local State.
 * @param {string} id The service id of the echo offer.
 * @param {Object} offerInfo The offer info.
 */
chrome.echoPrivate.setOfferInfo = function(id, offerInfo) {};

/**
 * Check in Local State for the offer info.
 * @param {string} id The service id of the offer eligibility check.
 * @param {Function} callback
 */
chrome.echoPrivate.getOfferInfo = function(id, callback) {};

/**
 * Get the group or coupon code from underlying storage.
 * @param {string} type Type of coupon code requested to be read (coupon or
 * group).
 * @param {Function} callback
 */
chrome.echoPrivate.getRegistrationCode = function(type, callback) {};

/**
 * Get the OOBE timestamp.
 * @param {Function} callback
 */
chrome.echoPrivate.getOobeTimestamp = function(callback) {};

/**
 * If device policy allows user to redeem offer, displays a native dialog
 * asking user for a consent to verify device's eligibility for the offer. If
 * the device policy forbids user to redeem offers, displays a native dialog
 * informing user the offer redeeming is disabled.
 * @param {Object} consentRequester Information about the service requesting
 * user consent.
 * @param {Function} callback
 */
chrome.echoPrivate.getUserConsent = function(consentRequester, callback) {};
