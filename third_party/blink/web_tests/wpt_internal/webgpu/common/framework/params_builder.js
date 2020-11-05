/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ import { publicParamsEquals } from './params_utils.js';
import { assert } from './util/util.js';
/** Forces a type to resolve its type definitions, to make it readable/debuggable. */

function typeAssert() {}
{
  {
    typeAssert();
    typeAssert();
    typeAssert();
    typeAssert();
    typeAssert();

    typeAssert();

    typeAssert();
    typeAssert();
    typeAssert();
    typeAssert();
    typeAssert();

    // Unexpected test results - hopefully okay to ignore these
    typeAssert();
    typeAssert();
  }
}

export function poptions(name, values) {
  const iter = makeReusableIterable(function* () {
    for (const value of values) {
      yield { [name]: value };
    }
  });

  return iter;
}

export function pbool(name) {
  return poptions(name, [false, true]);
}

export function params() {
  return new ParamsBuilder();
}

export class ParamsBuilder {
  paramSpecs = [{}];

  [Symbol.iterator]() {
    const iter = this.paramSpecs[Symbol.iterator]();
    return iter;
  }

  combine(newParams) {
    const paramSpecs = this.paramSpecs;
    this.paramSpecs = makeReusableIterable(function* () {
      for (const a of paramSpecs) {
        for (const b of newParams) {
          yield mergeParams(a, b);
        }
      }
    });

    return this;
  }

  expand(expander) {
    const paramSpecs = this.paramSpecs;
    this.paramSpecs = makeReusableIterable(function* () {
      for (const a of paramSpecs) {
        for (const b of expander(a)) {
          yield mergeParams(a, b);
        }
      }
    });

    return this;
  }

  filter(pred) {
    const paramSpecs = this.paramSpecs;
    this.paramSpecs = makeReusableIterable(function* () {
      for (const p of paramSpecs) {
        if (pred(p)) {
          yield p;
        }
      }
    });
    return this;
  }

  unless(pred) {
    return this.filter(x => !pred(x));
  }

  exclude(exclude) {
    const excludeArray = Array.from(exclude);
    const paramSpecs = this.paramSpecs;
    this.paramSpecs = makeReusableIterable(function* () {
      for (const p of paramSpecs) {
        if (excludeArray.every(e => !publicParamsEquals(p, e))) {
          yield p;
        }
      }
    });
    return this;
  }
}

// If you create an Iterable by calling a generator function (e.g. in IIFE), it is exhausted after
// one use. This just wraps a generator function in an object so it be iterated multiple times.
function makeReusableIterable(generatorFn) {
  return { [Symbol.iterator]: generatorFn };
}

// (keyof A & keyof B) is not empty, so they overlapped

function mergeParams(a, b) {
  for (const key of Object.keys(a)) {
    assert(!(key in b), 'Duplicate key: ' + key);
  }
  return { ...a, ...b };
}
