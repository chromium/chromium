/**
 * @license
 * Copyright The Closure Library Authors.
 * SPDX-License-Identifier: Apache-2.0
 */

goog.module('goog.async.deferredListTest');
goog.setTestOnly();
const Deferred = goog.require('goog.async.Deferred');
const DeferredList = goog.require('goog.async.DeferredList');
const googArray = goog.require('goog.array');
const testSuite = goog.require('goog.testing.testSuite');


// Re-throw (after a timeout) any errors not handled in an errback.
/** Use a computed property to avoid the compiler check on this define  */
Deferred['STRICT_ERRORS'] = true;


/**
 * A list of unhandled errors.
 * @type {Array.<Error>}
 */
const storedErrors = [];


/**
 * Adds a catch-all error handler to deferred objects. Unhandled errors that
 * reach the catch-all will be rethrown during tearDown.
 *
 * @param {...Deferred} var_args A list of deferred objects.
 */
function addCatchAll(var_args) {
  for (let i = 0, d; d = arguments[i]; i++) {
    d.addErrback(function(res) {
      storedErrors.push(res);
    });
  }
}


/**
 * Checks storedErrors for unhandled errors. If found, the error is rethrown.
 */
function checkCatchAll() {
  const err = storedErrors.shift();
  googArray.clear(storedErrors);

  if (err) {
    throw err;
  }
}

/**
 * @param {*} res
 */
function neverHappen(res) {
  fail('This should not happen');
}

testSuite({
  tearDown() {
    checkCatchAll();
  },

  testNoInputs() {
    let count = 0;
    const d = new DeferredList([]);

    d.addCallback(function(res) {
      assertArrayEquals([], res);
      count++;
    });
    addCatchAll(d);

    assertEquals(
        'An empty DeferredList should fire immediately with an empty ' +
            'list of results.',
        1, count);
  },

  testNoInputsAndFireOnOneCallback() {
    let count = 0;
    const d = new DeferredList([], true);

    d.addCallback(function(res) {
      assertArrayEquals([], res);
      count++;
    });
    addCatchAll(d);

    assertEquals(
        'An empty DeferredList with opt_fireOnOneCallback set should ' +
            'not fire unless callback is invoked explicitly.',
        0, count);

    d.callback([]);
    assertEquals('Calling callback explicitly should still fire.', 1, count);
  },

  testDeferredList() {
    let count = 0;

    const a = new Deferred();
    const b = new Deferred();
    const c = new Deferred();

    const dl = new DeferredList([a, b, c]);

    dl.addCallback(function(res) {
      assertEquals('Expected 3 Deferred results.', 3, res.length);

      assertTrue('Deferred a should return success.', res[0][0]);
      assertFalse('Deferred b should return failure.', res[1][0]);
      assertTrue('Deferred c should return success.', res[2][0]);

      assertEquals('Unexpected return value for a.', 'A', res[0][1]);
      assertEquals('Unexpected return value for c.', 'C', res[2][1]);

      assertEquals('B', res[1][1]);

      count++;
    });

    addCatchAll(dl);

    c.callback('C');
    assertEquals(0, count);

    b.errback('B');
    assertEquals(0, count);

    a.callback('A');

    checkCatchAll();
    assertEquals('DeferredList should fire on last call or errback.', 1, count);
  },

  testFireOnFirstCallback() {
    const a = new Deferred();
    const b = new Deferred();
    const c = new Deferred();

    const dl = new DeferredList([a, b, c], true);

    dl.addCallback(function(res) {
      assertEquals('Should be the deferred index in this mode.', 1, res[0]);
      assertEquals('B', res[1]);
    });
    dl.addErrback(neverHappen);

    addCatchAll(dl);

    a.errback('A');
    b.callback('B');

    // Shouldn't cause any more callbacks on the DeferredList.
    c.callback('C');
  },

  testFireOnFirstErrback() {
    const a = new Deferred();
    const b = new Deferred();
    const c = new Deferred();

    const dl = new DeferredList([a, b, c], false, true);

    dl.addCallback(neverHappen);
    dl.addErrback(function(res) {
      assertEquals('A', res);

      // Return a non-error value to break out of the errback path.
      return null;
    });
    addCatchAll(dl);

    b.callback('B');
    a.errback('A');

    assertTrue(c.hasFired());
    c.addErrback(function(res) {
      assertTrue(
          'The DeferredList errback should have canceled all pending inputs.',
          res instanceof Deferred.CanceledError);
      return null;
    });
    addCatchAll(c);

    // Shouldn't cause any more callbacks on the DeferredList.
    c.callback('C');
  },

  testNoConsumeErrors() {
    let count = 0;

    const a = new Deferred();
    const dl = new DeferredList([a]);

    a.addErrback(function(res) {
      count++;
      return null;
    });

    addCatchAll(a, dl);

    a.errback('oh noes');
    assertEquals(1, count);
  },

  testConsumeErrors() {
    const count = 0;

    const a = new Deferred();
    const dl = new DeferredList([a], false, false, true);

    a.addErrback(neverHappen);

    addCatchAll(a, dl);

    a.errback('oh noes');
    assertEquals(0, count);
  },

  testNesting() {
    function upperCase(res) {
      return res.toUpperCase();
    }

    // Concatenates a list of callback or errback results into a single string.
    function combine(res) {
      return googArray
          .map(
              res,
              function(result) {
                return result[1];
              })
          .join('');
    }

    const a = new Deferred();
    const b = new Deferred();
    const c = new Deferred();
    const d = new Deferred();

    a.addCallback(upperCase);
    b.addCallback(upperCase);
    c.addCallback(upperCase);
    d.addCallback(upperCase);

    const dl1 = new DeferredList([a, b]);
    const dl2 = new DeferredList([c, d]);

    dl1.addCallback(combine);
    dl2.addCallback(combine);

    const dl3 = new DeferredList([dl1, dl2]);
    dl3.addCallback(combine);
    dl3.addCallback(function(res) {
      assertEquals('AbCd', res);
    });

    addCatchAll(dl1, dl2, dl3);

    a.callback('a');
    c.callback('c');
    b.errback('b');
    d.errback('d');
  },

  testGatherResults() {
    const a = new Deferred();
    const b = new Deferred();
    const c = new Deferred();

    const dl = DeferredList.gatherResults([a, b, c]);

    dl.addCallback(function(res) {
      assertArrayEquals(['A', 'B', 'C'], res);
    });

    addCatchAll(dl);

    b.callback('B');
    a.callback('A');
    c.callback('C');
  },

  testGatherResultsFailure() {
    const a = new Deferred();
    const b = new Deferred();
    const c = new Deferred();

    const dl = DeferredList.gatherResults([a, b, c]);

    let firedErrback = false;
    let firedCallback = false;
    dl.addCallback(function() {
      firedCallback = true;
    });
    dl.addErrback(function() {
      firedErrback = true;
      return null;
    });

    addCatchAll(dl);

    b.callback('B');
    a.callback('A');
    c.errback();

    assertTrue('Errback should be called', firedErrback);
    assertFalse('Callback should not be called', firedCallback);
  },

  testGatherResults_cancelCancelsChildren() {
    const canceled = [];
    const a = new Deferred(function() {
      canceled.push('a');
    });
    const b = new Deferred(function() {
      canceled.push('b');
    });
    const c = new Deferred(function() {
      canceled.push('c');
    });

    const dl = new DeferredList([a, b, c]);

    let firedErrback = false;
    let firedCallback = false;
    dl.addCallback(function() {
      firedCallback = true;
    });
    dl.addErrback(function() {
      firedErrback = true;
      return null;
    });

    addCatchAll(dl);

    b.callback('b');
    dl.cancel();

    assertTrue('Errback should be called', firedErrback);
    assertFalse('Callback should not be called', firedCallback);
    assertArrayEquals(['a', 'c'], canceled);
  },

  testErrorCancelsPendingChildrenWhenFireOnFirstError() {
    const canceled = [];
    const a = new Deferred(function() {
      canceled.push('a');
    });
    const b = new Deferred(function() {
      canceled.push('b');
    });
    const c = new Deferred(function() {
      canceled.push('c');
    });

    const dl = new DeferredList([a, b, c], false, true);

    let firedErrback = false;
    let firedCallback = false;
    dl.addCallback(function() {
      firedCallback = true;
    });
    dl.addErrback(function() {
      firedErrback = true;
      return null;
    });

    addCatchAll(dl);

    a.callback('a');
    b.errback();

    assertTrue('Errback should be called', firedErrback);
    assertFalse('Callback should not be called', firedCallback);
    assertArrayEquals(
        'Only C should be canceled since A and B fired.', ['c'], canceled);
  },

  testErrorDoesNotCancelPendingChildrenForVanillaLists() {
    const canceled = [];
    const a = new Deferred(function() {
      canceled.push('a');
    });
    const b = new Deferred(function() {
      canceled.push('b');
    });
    const c = new Deferred(function() {
      canceled.push('c');
    });

    const dl = new DeferredList([a, b, c]);

    let firedErrback = false;
    let firedCallback = false;
    dl.addCallback(function() {
      firedCallback = true;
    });
    dl.addErrback(function() {
      firedErrback = true;
      return null;
    });

    addCatchAll(dl);

    a.callback('a');
    b.errback();
    c.callback('c');

    assertFalse('Errback should not be called', firedErrback);
    assertTrue('Callback should be called', firedCallback);
    assertArrayEquals('No cancellations', [], canceled);
  },

  testInputDeferredsStillUsable() {
    const increment = function(res) {
      return res + 1;
    };
    const incrementErrback = function(res) {
      throw res + 1;
    };

    let aComplete = false;
    let bComplete = false;
    let hadListCallback = false;

    const a = new Deferred().addCallback(increment);
    const b = new Deferred().addErrback(incrementErrback);
    const c = new Deferred();

    const dl = new DeferredList([a, b, c]);

    a.callback(0);
    a.addCallback(increment);
    a.addCallback(function(res) {
      aComplete = true;
      assertEquals(
          'The "a" Deferred should have had two increment callbacks.', 2, res);
    });
    assertTrue('The "a" deferred should complete before the list.', aComplete);

    b.errback(0);
    b.addErrback(incrementErrback);
    b.addErrback(function(res) {
      bComplete = true;
      assertEquals(
          'The "b" Deferred should have had two increment errbacks.', 2, res);
    });
    assertTrue('The "b" deferred should complete before the list.', bComplete);

    assertFalse(
        'The list should not fire until every input has.', dl.hasFired());
    c.callback();
    assertTrue(dl.hasFired());

    assertFalse(hadListCallback);
    dl.addCallback(function(results) {
      hadListCallback = true;

      const aResult = results[0];
      const bResult = results[1];
      const cResult = results[2];

      assertTrue(aResult[0]);
      assertEquals(
          'Should see the result from before the second callback was added.', 1,
          aResult[1]);

      assertFalse(bResult[0]);
      assertEquals(
          'Should see the result from before the second errback was added.', 1,
          aResult[1]);

      assertTrue(cResult[0]);
    });
    assertTrue(hadListCallback);

    addCatchAll(dl);
  }
});
