/**
 * AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
 **/ // Implements the wpt-embedded test runner (see also: wpt/cts.html).
import { DefaultTestFileLoader } from '../framework/file_loader.js';
import { Logger } from '../framework/logging/logger.js';
import { parseQuery } from '../framework/query/parseQuery.js';
import { parseExpectationsForTestQuery } from '../framework/query/query.js';
import { assert } from '../framework/util/util.js';

import { optionEnabled } from './helper/options.js';
import { TestWorker } from './helper/test_worker.js';

// testharness.js API (https://web-platform-tests.org/writing-tests/testharness-api.html)

setup({
  // It's convenient for us to asynchronously add tests to the page. Prevent done() from being
  // called implicitly when the page is finished loading.
  explicit_done: true,
});

(async () => {
  const workerEnabled = optionEnabled('worker');
  const worker = workerEnabled ? new TestWorker(false) : undefined;

  const loader = new DefaultTestFileLoader();
  const qs = new URLSearchParams(window.location.search).getAll('q');
  assert(qs.length === 1, 'currently, there must be exactly one ?q=');
  const filterQuery = parseQuery(qs[0]);
  const testcases = await loader.loadCases(filterQuery);

  const expectations =
    typeof loadWebGPUExpectations !== 'undefined'
      ? parseExpectationsForTestQuery(
          await loadWebGPUExpectations,
          filterQuery,
          new URL(window.location.href)
        )
      : [];

  const log = new Logger(false);

  for (const testcase of testcases) {
    const name = testcase.query.toString();
    const wpt_fn = async () => {
      const [rec, res] = log.record(name);
      if (worker) {
        await worker.run(rec, name, expectations);
      } else {
        await testcase.run(rec, expectations);
      }

      // Unfortunately, it seems not possible to surface any logs for warn/skip.
      if (res.status === 'fail') {
        throw (res.logs || []).map(s => s.toJSON()).join('\n\n');
      }
    };

    promise_test(wpt_fn, name);
  }

  done();
})();
