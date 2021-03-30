/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ import { SkipTestCase, UnexpectedPassError } from './fixture.js';
import { extractPublicParams, mergeParams } from './params_utils.js';
import { compareQueries, Ordering } from './query/compare.js';
import { TestQuerySingleCase } from './query/query.js';
import { kPathSeparator } from './query/separators.js';
import { stringifyPublicParams, stringifyPublicParamsUniquely } from './query/stringify_params.js';
import { validQueryPart } from './query/validQueryPart.js';
import { assert, unreachable } from './util/util.js';

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
  caseParams = undefined;
  subcaseParams = undefined;

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
    // TODO: add TODO if there's no description? (and make sure it only ends up on actual tests,
    // not on test parents in the tree, which is what happens if you do it here, not sure why)
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

    if (this.caseParams === undefined) {
      return;
    }

    const seen = new Set();
    for (const testcase of this.caseParams) {
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
    return this.cases(casesIterable);
  }

  cases(casesIterable) {
    assert(this.caseParams === undefined, 'test case is already parameterized');
    const newSelf = this;
    newSelf.caseParams = Array.from(casesIterable);

    return newSelf;
  }

  subcases(specs) {
    assert(this.subcaseParams === undefined, 'test subcases are already parameterized');
    const newSelf = this;
    newSelf.subcaseParams = specs;

    return newSelf;
  }

  *iterate() {
    assert(this.testFn !== undefined, 'No test function (.fn()) for test');
    for (const params of this.caseParams || [{}]) {
      yield new RunCaseSpecific(
        this.testPath,
        params,
        this.subcaseParams,
        this.fixture,
        this.testFn,
        this.testCreationStack
      );
    }
  }
}

class RunCaseSpecific {
  constructor(testPath, params, subParamGen, fixture, fn, testCreationStack) {
    this.id = { test: testPath, params: extractPublicParams(params) };
    this.params = params;
    this.subParamGen = subParamGen;
    this.fixture = fixture;
    this.fn = fn;
    this.testCreationStack = testCreationStack;
  }

  async runTest(rec, params, throwSkip, expectedStatus) {
    try {
      rec.beginSubCase();
      if (expectedStatus === 'skip') {
        throw new SkipTestCase('Skipped by expectations');
      }
      const inst = new this.fixture(rec, params);

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
      if (throwSkip && ex instanceof SkipTestCase) {
        throw ex;
      }
      rec.threw(ex);
    } finally {
      try {
        rec.endSubCase(expectedStatus);
      } catch (ex) {
        assert(ex instanceof UnexpectedPassError);
        ex.message = `Testcase passed unexpectedly.`;
        ex.stack = this.testCreationStack.stack;
        rec.warn(ex);
      }
    }
  }

  async run(rec, selfQuery, expectations) {
    const getExpectedStatus = selfQueryWithSubParams => {
      let didSeeFail = false;
      for (const exp of expectations) {
        const ordering = compareQueries(exp.query, selfQueryWithSubParams);
        if (ordering === Ordering.Unordered || ordering === Ordering.StrictSubset) {
          continue;
        }

        switch (exp.expectation) {
          // Skip takes precendence. If there is any expectation indicating a skip,
          // signal it immediately.
          case 'skip':
            return 'skip';
          case 'fail':
            // Otherwise, indicate that we might expect a failure.
            didSeeFail = true;
            break;
          default:
            unreachable();
        }
      }
      return didSeeFail ? 'fail' : 'pass';
    };

    rec.start();
    if (this.subParamGen) {
      let totalCount = 0;
      let skipCount = 0;
      for (const subParams of this.subParamGen(this.params)) {
        rec.info(new Error('subcase: ' + stringifyPublicParams(subParams)));
        try {
          const params = mergeParams(this.params, subParams);
          const subcaseQuery = new TestQuerySingleCase(
            selfQuery.suite,
            selfQuery.filePathParts,
            selfQuery.testPathParts,
            params
          );

          await this.runTest(rec, params, true, getExpectedStatus(subcaseQuery));
        } catch (ex) {
          if (ex instanceof SkipTestCase) {
            // Convert SkipTestCase to info messages
            ex.message = 'subcase skipped: ' + ex.message;
            rec.info(ex);
            ++skipCount;
          } else {
            // Since we are catching all error inside runTest(), this should never happen
            rec.threw(ex);
          }
        }
        ++totalCount;
      }
      if (skipCount === totalCount) {
        rec.skipped(new SkipTestCase('all subcases were skipped'));
      }
    } else {
      await this.runTest(rec, this.params, false, getExpectedStatus(selfQuery));
    }
    rec.finish();
  }
}
