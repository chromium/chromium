/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ import { SkipTestCase } from '../fixture.js';
import { now, assert } from '../util/util.js';
import { LogMessageWithStack } from './log_message.js';
var LogSeverity;
(function (LogSeverity) {
  LogSeverity[(LogSeverity['Pass'] = 0)] = 'Pass';
  LogSeverity[(LogSeverity['Skip'] = 1)] = 'Skip';
  LogSeverity[(LogSeverity['Warn'] = 2)] = 'Warn';
  LogSeverity[(LogSeverity['ExpectFailed'] = 3)] = 'ExpectFailed';
  LogSeverity[(LogSeverity['ValidationFailed'] = 4)] = 'ValidationFailed';
  LogSeverity[(LogSeverity['ThrewException'] = 5)] = 'ThrewException';
})(LogSeverity || (LogSeverity = {}));

const kMaxLogStacks = 2;

/** Holds onto a LiveTestCaseResult owned by the Logger, and writes the results into it. */
export class TestCaseRecorder {
  finalCaseStatus = LogSeverity.Pass;
  hideStacksBelowSeverity = LogSeverity.Warn;
  startTime = -1;
  logs = [];
  logLinesAtCurrentSeverity = 0;
  debugging = false;
  /** Used to dedup log messages which have identical stacks. */
  messagesForPreviouslySeenStacks = new Map();

  constructor(result, debugging) {
    this.result = result;
    this.debugging = debugging;
  }

  start() {
    assert(this.startTime < 0, 'TestCaseRecorder cannot be reused');
    this.startTime = now();
  }

  finish() {
    assert(this.startTime >= 0, 'finish() before start()');

    const timeMilliseconds = now() - this.startTime;
    // Round to next microsecond to avoid storing useless .xxxx00000000000002 in results.
    this.result.timems = Math.ceil(timeMilliseconds * 1000) / 1000;

    // Convert numeric enum back to string (but expose 'exception' as 'fail')
    this.result.status =
      this.finalCaseStatus === LogSeverity.Pass
        ? 'pass'
        : this.finalCaseStatus === LogSeverity.Skip
        ? 'skip'
        : this.finalCaseStatus === LogSeverity.Warn
        ? 'warn'
        : 'fail'; // Everything else is an error

    this.result.logs = this.logs;
  }

  injectResult(injectedResult) {
    Object.assign(this.result, injectedResult);
  }

  debug(ex) {
    if (!this.debugging) return;
    this.logImpl(LogSeverity.Pass, 'DEBUG', ex);
  }

  info(ex) {
    this.logImpl(LogSeverity.Pass, 'INFO', ex);
  }

  skipped(ex) {
    this.logImpl(LogSeverity.Skip, 'SKIP', ex);
  }

  warn(ex) {
    this.logImpl(LogSeverity.Warn, 'WARN', ex);
  }

  expectationFailed(ex) {
    this.logImpl(LogSeverity.ExpectFailed, 'EXPECTATION FAILED', ex);
  }

  validationFailed(ex) {
    this.logImpl(LogSeverity.ValidationFailed, 'VALIDATION FAILED', ex);
  }

  threw(ex) {
    if (ex instanceof SkipTestCase) {
      this.skipped(ex);
      return;
    }
    this.logImpl(LogSeverity.ThrewException, 'EXCEPTION', ex);
  }

  logImpl(level, name, baseException) {
    const logMessage = new LogMessageWithStack(name, baseException);

    // Deduplicate errors with the exact same stack
    if (!this.debugging && logMessage.stack) {
      const seen = this.messagesForPreviouslySeenStacks.get(logMessage.stack);
      if (seen) {
        seen.incrementTimesSeen();
        return;
      }
      this.messagesForPreviouslySeenStacks.set(logMessage.stack, logMessage);
    }

    // Final case status should be the "worst" of all log entries.
    if (level > this.finalCaseStatus) this.finalCaseStatus = level;

    // setStackHidden for all logs except `kMaxLogStacks` stacks at the highest severity
    if (level > this.hideStacksBelowSeverity) {
      this.logLinesAtCurrentSeverity = 0;
      this.hideStacksBelowSeverity = level;

      // Go back and setStackHidden for everything of a lower log level
      for (const log of this.logs) {
        log.setStackHidden();
      }
    }
    if (level === this.hideStacksBelowSeverity) {
      this.logLinesAtCurrentSeverity++;
    }
    if (level < this.hideStacksBelowSeverity || this.logLinesAtCurrentSeverity > kMaxLogStacks) {
      logMessage.setStackHidden();
    }

    this.logs.push(logMessage);
  }
}
