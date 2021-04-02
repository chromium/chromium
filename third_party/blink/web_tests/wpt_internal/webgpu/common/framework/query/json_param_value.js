/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ import { assert, sortObjectByKey } from '../util/util.js';
// JSON can't represent `undefined` and by default stores it as `null`.
// Instead, store `undefined` as this magic string value in JSON.
const jsUndefinedMagicValue = '_undef_';

function stringifyFilter(k, v) {
  // Make sure no one actually uses the magic value as a parameter.
  assert(v !== jsUndefinedMagicValue);

  return v === undefined ? jsUndefinedMagicValue : v;
}

export function stringifyParamValue(value) {
  return JSON.stringify(value, stringifyFilter);
}

/**
 * Like stringifyParamValue but sorts dictionaries by key, for hashing.
 */
export function stringifyParamValueUniquely(value) {
  return JSON.stringify(value, (k, v) => {
    if (typeof v === 'object' && v !== null) {
      return sortObjectByKey(v);
    }

    return stringifyFilter(k, v);
  });
}

export function parseParamValue(s) {
  return JSON.parse(s, (k, v) => (v === jsUndefinedMagicValue ? undefined : v));
}
