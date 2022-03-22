// Implements the wpt-embedded test runner (see also: wpt/cts.https.html).

import { DefaultTestFileLoader } from '../../third_party/webgpu-cts/src/common/internal/file_loader.js';
import { prettyPrintLog } from '../../third_party/webgpu-cts/src/common/internal/logging/log_message.js';
import { Logger } from '../../third_party/webgpu-cts/src/common/internal/logging/logger.js';
import { parseQuery } from '../../third_party/webgpu-cts/src/common/internal/query/parseQuery.js';
import { parseExpectationsForTestQuery, relativeQueryString } from '../../third_party/webgpu-cts/src/common/internal/query/query.js';
import { assert } from '../../third_party/webgpu-cts/src/common/util/util.js';

import { optionEnabled } from '../../third_party/webgpu-cts/src/common/runtime/helper/options.js';
import { TestWorker } from '../../third_party/webgpu-cts/src/common/runtime/helper/test_worker.js';

var socket;

async function setupWebsocket(port) {
  socket = new WebSocket('ws://127.0.0.1:' + port)
  socket.addEventListener('message', runCtsTestViaSocket);
}

async function runCtsTestViaSocket(event) {
  let input = JSON.parse(event.data);
  runCtsTest(input['q'], input['w']);
}

async function runCtsTest(query, use_worker) {
  const workerEnabled = use_worker;
  const worker = workerEnabled ? new TestWorker(false) : undefined;

  const loader = new DefaultTestFileLoader();
  const filterQuery = parseQuery(query);
  const testcases = await loader.loadCases(filterQuery);

  const expectations = [];

  const log = new Logger();

  for (const testcase of testcases) {
    const name = testcase.query.toString();
    // For brevity, display the case name "relative" to the ?q= path.
    const shortName = relativeQueryString(filterQuery, testcase.query) || '(case)';

    const wpt_fn = async () => {
      const [rec, res] = log.record(name);
      if (worker) {
        await worker.run(rec, name, expectations);
      } else {
        await testcase.run(rec, expectations);
      }

      socket.send(JSON.stringify({'s': res.status,
                                  'l': (res.logs ?? []).map(prettyPrintLog)}));
    };
    await wpt_fn();
  }
}

window.runCtsTest = runCtsTest;
window.setupWebsocket = setupWebsocket
