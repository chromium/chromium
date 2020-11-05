/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ import { extractImportantStackTrace } from '../util/stack.js';
export class LogMessageWithStack extends Error {
  stackHidden = false;
  timesSeen = 1;

  constructor(name, ex) {
    super(ex.message);

    this.name = name;
    this.stack = ex.stack;
  }

  /** Set a flag so the stack is not printed in toJSON(). */
  setStackHidden() {
    this.stackHidden = true;
  }

  /** Increment the "seen x times" counter. */
  incrementTimesSeen() {
    this.timesSeen++;
  }

  toJSON() {
    let m = this.name;
    if (this.message) m += ': ' + this.message;
    if (!this.stackHidden && this.stack) {
      m += '\n' + extractImportantStackTrace(this);
    }
    if (this.timesSeen > 1) {
      m += `\n(seen ${this.timesSeen} times with identical stack)`;
    }
    return m;
  }
}
