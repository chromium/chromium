/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ import { SkipTestCase } from './fixture.js';
import { extractPublicParams } from './params_utils.js';
import { kPathSeparator } from './query/separators.js';
import { stringifyPublicParams, stringifyPublicParamsUniquely } from './query/stringify_params.js';
import { validQueryPart } from './query/validQueryPart.js';
import { assert } from './util/util.js';

export function makeTestGroup(fixture) {
  return new TestGroup(fixture);
}

// Interfaces for running tests

export function makeTestGroupForUnitTesting(fixture) {
  return new TestGroup(fixture);
}

class TestGroup {
  seen = new Set();
  tests = [];

  constructor(fixture) {
    this.fixture = fixture;
  }

  iterate() {
    return this.tests;
  }

  checkName(name) {
    assert(
      // Shouldn't happen due to the rule above. Just makes sure that treated
      // unencoded strings as encoded strings is OK.
      name === decodeURIComponent(name),
      `Not decodeURIComponent-idempotent: ${name} !== ${decodeURIComponent(name)}`
    );

    assert(!this.seen.has(name), `Duplicate test name: ${name}`);

    this.seen.add(name);
  }

  // TODO: This could take a fixture, too, to override the one for the group.
  test(name) {
    const testCreationStack = new Error(`Test created: ${name}`);

    this.checkName(name);

    const parts = name.split(kPathSeparator);
    for (const p of parts) {
      assert(validQueryPart.test(p), `Invalid test name part ${p}; must match ${validQueryPart}`);
    }

    const test = new TestBuilder(parts, this.fixture, testCreationStack);
    this.tests.push(test);
    return test;
  }

  validate() {
    for (const test of this.tests) {
      test.validate();
    }
  }
}

class TestBuilder {
  cases = undefined;

  constructor(testPath, fixture, testCreationStack) {
    this.testPath = testPath;
    this.fixture = fixture;
    this.testCreationStack = testCreationStack;
  }

  desc(description) {
    this.description = description.trim();
    return this;
  }

  fn(fn) {
    assert(this.testFn === undefined);
    this.testFn = fn;
  }

  unimplemented() {
    assert(this.testFn === undefined);

    this.description =
      (this.description ? this.description + '\n\n' : '') + 'TODO: .unimplemented()';

    this.testFn = () => {
      throw new SkipTestCase('test unimplemented');
    };
  }

  validate() {
    const testPathString = this.testPath.join(kPathSeparator);
    assert(this.testFn !== undefined, () => {
      let s = `Test is missing .fn(): ${testPathString}`;
      if (this.testCreationStack.stack) {
        s += `\n-> test created at:\n${this.testCreationStack.stack}`;
      }
      return s;
    });

    if (this.cases === undefined) {
      return;
    }

    const seen = new Set();
    for (const testcase of this.cases) {
      // stringifyPublicParams also checks for invalid params values
      const testcaseString = stringifyPublicParams(testcase);

      // A (hopefully) unique representation of a params value.
      const testcaseStringUnique = stringifyPublicParamsUniquely(testcase);
      assert(
        !seen.has(testcaseStringUnique),
        `Duplicate public test case params for test ${testPathString}: ${testcaseString}`
      );

      seen.add(testcaseStringUnique);
    }
  }

  params(casesIterable) {
    assert(this.cases === undefined, 'test case is already parameterized');
    this.cases = Array.from(casesIterable);

    return this;
  }

  *iterate() {
    assert(this.testFn !== undefined, 'No test function (.fn()) for test');
    for (const params of this.cases || [{}]) {
      yield new RunCaseSpecific(this.testPath, params, this.fixture, this.testFn);
    }
  }
}

class RunCaseSpecific {
  constructor(testPath, params, fixture, fn) {
    this.id = { test: testPath, params: extractPublicParams(params) };
    this.params = params;
    this.fixture = fixture;
    this.fn = fn;
  }

  async run(rec) {
    rec.start();
    try {
      const inst = new this.fixture(rec, this.params || {});

      try {
        await inst.init();

        await this.fn(inst);
      } finally {
        // Runs as long as constructor succeeded, even if initialization or the test failed.
        await inst.finalize();
      }
    } catch (ex) {
      // There was an exception from constructor, init, test, or finalize.
      // An error from init or test may have been a SkipTestCase.
      // An error from finalize may have been an eventualAsyncExpectation failure
      // or unexpected validation/OOM error from the GPUDevice.
      rec.threw(ex);
    }
    rec.finish();
  }
}
