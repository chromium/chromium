/**
 * Copyright 2026 Google LLC.
 * Copyright (c) Microsoft Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

import fs from 'node:fs';
import path from 'node:path';

function getSinkData() {
  if (!process.env.LUCI_CONTEXT || !fs.existsSync(process.env.LUCI_CONTEXT)) {
    return null;
  }
  try {
    const luciConfig = fs.readFileSync(process.env.LUCI_CONTEXT, 'utf8');
    const sink = JSON.parse(luciConfig)['result_sink'];
    if (!sink) return null;
    return {
      url: `http://${sink.address}/prpc/luci.resultsink.v1.Sink/ReportTestResults`,
      authToken: sink.auth_token,
    };
  } catch (e) {
    console.error(e);
    return null;
  }
}

function escapeHtml(unsafe) {
  if (!unsafe) return '';
  return unsafe
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;')
    .replace(/'/g, '&#039;');
}

export default async function* customReporter(source) {
  const sinkData = getSinkData();
  const fileSuites = new Map();
  let pendingResults = [];

  const sendBatch = async (batch) => {
    if (!sinkData || batch.length === 0) return;
    try {
      await fetch(sinkData.url, {
        method: 'POST',
        headers: {
          'Content-Type': 'application/json',
          Accept: 'application/json',
          Authorization: `ResultSink ${sinkData.authToken}`,
        },
        body: JSON.stringify({testResults: batch}),
      });
    } catch (err) {
      console.error('Failed to post to ResultSink', err);
    }
  };

  for await (const event of source) {
    // Collect for debug regardless of sinkData

    if (event.type === 'test:start') {
      const {data} = event;
      if (!fileSuites.has(data.file)) {
        fileSuites.set(data.file, []);
      }
      fileSuites.get(data.file)[data.nesting] = data.name;
    }

    if (event.type === 'test:pass' || event.type === 'test:fail') {
      const {data} = event;

      if (data.details && data.details.type === 'suite') {
        yield '';
        continue;
      }

      // Filter out synthetic file-level test events generated for files with no tests
      if (
        data.nesting === 0 &&
        /\.test\.js$/.test(data.name) &&
        data.file &&
        data.file.endsWith(data.name.replace(/^[./\\]+/, ''))
      ) {
        yield '';
        continue;
      }

      const stack = fileSuites.get(data.file) || [];

      if (!sinkData) {
        yield '';
        continue;
      }

      let status = event.type === 'test:pass' ? 'PASS' : 'FAIL';
      let expected = status === 'PASS';

      if (data.skip !== undefined && data.skip !== false) {
        status = 'SKIP';
        expected = true;
      }

      const durationMs = data.details?.duration_ms || 0;

      let sourcePath = path.relative(process.cwd(), data.file);
      // Strip 'out/<Target>/gen/' to get the original source path and replace extension
      sourcePath = sourcePath
        .replace(/^out\/[^/]+\/gen\//, '')
        .replace(/\.js$/, '.ts');

      const escapeComponent = (s) =>
        s.replace(/\\/g, '\\\\').replace(/:/g, '\\:');
      const components = [...stack.slice(0, data.nesting), data.name].map((c) =>
        escapeComponent(c.replace(/[\r\n]+/g, ' ').normalize('NFC')),
      );
      let coarseName = path.dirname(sourcePath);
      if (!coarseName.endsWith('/')) {
        coarseName += '/';
      }
      const fineName = path.basename(sourcePath);

      const flatEscape = (s) => s.replace(/([!#:\\])/g, '\\$1');
      const caseNameFlat = components
        .map(flatEscape)
        .join(':')
        .substring(0, 512);
      const coarseNameFlat = flatEscape(coarseName);
      const fineNameFlat = flatEscape(fineName);

      const testId = `:chromium-bidi!mocha:${coarseNameFlat}:${fineNameFlat}#${caseNameFlat}`;

      const testResult = {
        testId,
        testIdStructured: {
          moduleName: 'chromium-bidi',
          moduleScheme: 'mocha',
          coarseName,
          fineName,
          caseNameComponents: components,
        },
        status,
        expected,
        duration: `${(durationMs / 1000).toFixed(3)}s`,
      };

      if (status === 'FAIL' && data.details?.error) {
        const errorInfo =
          data.details.error.cause?.stack ||
          data.details.error.cause?.message ||
          data.details.error.message ||
          data.details.error.name;
        testResult.summaryHtml = `<pre>${escapeHtml(errorInfo)}</pre>`;
      }

      pendingResults.push(testResult);

      if (pendingResults.length >= 50) {
        const batch = pendingResults;
        pendingResults = [];
        await sendBatch(batch);
      }
    }
    yield '';
  }

  if (pendingResults.length > 0) {
    await sendBatch(pendingResults);
  }
}
