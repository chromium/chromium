// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @type {?string} Result
 */
var result;

/**
 * @type {!PromiseSlot} Test target.
 */
var slot;

function setUp() {
  slot = new PromiseSlot(function(value) {
    result = 'fulfilled:' + value;
  }, function(value) {
    result = 'rejected:' + value;
  });
  result = null;
}

function testPromiseSlot(callback) {
  var fulfilledPromise = Promise.resolve('fulfilled');
  var rejectedPromise = Promise.reject('rejected');
  slot.setPromise(fulfilledPromise);
  reportPromise(fulfilledPromise.then(function() {
    assertEquals('fulfilled:fulfilled', result);
    slot.setPromise(rejectedPromise);
    return rejectedPromise;
  }).then(function() {
    // Should not reach here.
    assertTrue(false);
  }, function() {
    assertEquals('rejected:rejected', result);
  }), callback);
}

function testPromiseSlotReassignBeforeCompletion(callback) {
  var fulfillComputation;
  var computingPromise = new Promise(function(fulfill, reject) {
    fulfillComputation = fulfill;
  });
  var fulfilledPromise = Promise.resolve('fulfilled');

  slot.setPromise(computingPromise);
  // Reassign promise.
  slot.setPromise(fulfilledPromise);
  reportPromise(fulfilledPromise.then(function() {
    assertEquals('fulfilled:fulfilled', result);
    fulfillComputation('fulfilled after detached');
    return computingPromise;
  }).then(function(value) {
    assertEquals('fulfilled after detached', value);
    // The detached promise does not affect the slot.
    assertEquals('fulfilled:fulfilled', result);
  }), callback);
}

function testPromiseSlotReassignBeforeCompletionWithCancel(callback) {
  var rejectComputation;
  var computingPromise = new Promise(function(fulfill, reject) {
    rejectComputation = reject;
  });
  computingPromise.cancel = function() {
    rejectComputation('cancelled');
  };
  var fulfilledPromise = Promise.resolve('fulfilled');

  slot.setPromise(computingPromise);
  slot.setPromise(fulfilledPromise);
  reportPromise(fulfilledPromise.then(function() {
    assertEquals('fulfilled:fulfilled', result);
    return computingPromise;
  }).then(function() {
    // Should not reach here.
    assertTrue(false);
  }, function(value) {
    assertEquals('cancelled', value);
    // The detached promise does not affect the slot.
    assertEquals('fulfilled:fulfilled', result);
  }), callback);
}

function testPromiseSlotReassignNullBeforeCompletion(callback) {
  var fulfillComputation;
  var computingPromise = new Promise(function(fulfill, reject) {
    fulfillComputation = fulfill;
  });

  slot.setPromise(computingPromise);
  slot.setPromise(null);
  assertEquals(null, result);

  fulfillComputation('fulfilled');
  reportPromise(computingPromise.then(function(value) {
    assertEquals('fulfilled', value);
    assertEquals(null, result);
  }), callback);
}
