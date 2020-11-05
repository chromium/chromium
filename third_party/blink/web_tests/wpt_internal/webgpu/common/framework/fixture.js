/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ import { assert } from './util/util.js';

export class SkipTestCase extends Error {}

// A Fixture is a class used to instantiate each test case at run time.
// A new instance of the Fixture is created for every single test case
// (i.e. every time the test function is run).
export class Fixture {
  eventualExpectations = [];
  numOutstandingAsyncExpectations = 0;

  constructor(rec, params) {
    this.rec = rec;
    this._params = params;
  }

  get params() {
    return this._params;
  }

  // This has to be a member function instead of an async `createFixture` function, because
  // we need to be able to ergonomically override it in subclasses.
  async init() {}

  debug(msg) {
    this.rec.debug(new Error(msg));
  }

  skip(msg) {
    throw new SkipTestCase(msg);
  }

  async finalize() {
    assert(
      this.numOutstandingAsyncExpectations === 0,
      'there were outstanding immediateAsyncExpectations (e.g. expectUncapturedError) at the end of the test'
    );

    // Loop to exhaust the eventualExpectations in case they chain off each other.
    while (this.eventualExpectations.length) {
      const previousExpectations = this.eventualExpectations;
      this.eventualExpectations = [];

      await Promise.all(previousExpectations);
    }
  }

  warn(msg) {
    this.rec.warn(new Error(msg));
  }

  fail(msg) {
    this.rec.expectationFailed(new Error(msg));
  }

  async immediateAsyncExpectation(fn) {
    this.numOutstandingAsyncExpectations++;
    const ret = await fn();
    this.numOutstandingAsyncExpectations--;
    return ret;
  }

  eventualAsyncExpectation(fn) {
    const promise = fn(new Error());
    this.eventualExpectations.push(promise);
    return promise;
  }

  expectErrorValue(expectedName, ex, niceStack) {
    if (!(ex instanceof Error)) {
      niceStack.message = `THREW non-error value, of type ${typeof ex}: ${ex}`;
      this.rec.expectationFailed(niceStack);
      return;
    }
    const actualName = ex.name;
    if (actualName !== expectedName) {
      niceStack.message = `THREW ${actualName}, instead of ${expectedName}: ${ex}`;
      this.rec.expectationFailed(niceStack);
    } else {
      niceStack.message = `OK: threw ${actualName}: ${ex.message}`;
      this.rec.debug(niceStack);
    }
  }

  shouldResolve(p, msg) {
    this.eventualAsyncExpectation(async niceStack => {
      const m = msg ? ': ' + msg : '';
      try {
        await p;
        niceStack.message = 'resolved as expected' + m;
      } catch (ex) {
        niceStack.message = `REJECTED${m}\n${ex.message}`;
        this.rec.expectationFailed(niceStack);
      }
    });
  }

  shouldReject(expectedName, p, msg) {
    this.eventualAsyncExpectation(async niceStack => {
      const m = msg ? ': ' + msg : '';
      try {
        await p;
        niceStack.message = 'DID NOT REJECT' + m;
        this.rec.expectationFailed(niceStack);
      } catch (ex) {
        niceStack.message = 'rejected as expected' + m;
        this.expectErrorValue(expectedName, ex, niceStack);
      }
    });
  }

  shouldThrow(expectedName, fn, msg) {
    const m = msg ? ': ' + msg : '';
    try {
      fn();
      this.rec.expectationFailed(new Error('DID NOT THROW' + m));
    } catch (ex) {
      this.expectErrorValue(expectedName, ex, new Error(m));
    }
  }

  expect(cond, msg) {
    if (cond) {
      const m = msg ? ': ' + msg : '';
      this.rec.debug(new Error('expect OK' + m));
    } else {
      this.rec.expectationFailed(new Error(msg));
    }
    return cond;
  }
}
