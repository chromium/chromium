/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ import { version } from '../version.js';
import { TestCaseRecorder } from './test_case_recorder.js';

export class Logger {
  results = new Map();

  constructor(debug) {
    this.debug = debug;
  }

  record(name) {
    const result = { status: 'running', timems: -1 };
    this.results.set(name, result);
    return [new TestCaseRecorder(result, this.debug), result];
  }

  asJSON(space) {
    return JSON.stringify({ version, results: Array.from(this.results) }, undefined, space);
  }
}
