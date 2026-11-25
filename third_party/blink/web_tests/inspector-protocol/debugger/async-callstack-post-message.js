// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} = await testRunner.startBlank(
      'Tests asynchronous call stacks for window.postMessage.');
  const runAsyncCallStacksTest =
      await testRunner.loadScript('resources/async-callstack.js');

  await runAsyncCallStacksTest(testRunner, session, dp, {
    totalDebuggerStatements: 5,
    async setup() {
      const frameHTML = [
        '<!DOCTYPE html>',
        '<html>',
        '<body>',
        '<script>',
        'function onMessageReceivedInFrame(e) {',
        '    var msg = \'Received message: data="\' + e.data + \'" origin="\' + e.origin + \'"\';',
        '    if (e.data !== "skip debugger") debugger;',
        '    postMessageToParent(msg);',
        '}',
        '',
        'function postMessageToParent(msg)',
        '{',
        '    top.postMessage(msg, "*");',
        '}',
        '',
        'window.addEventListener("message", onMessageReceivedInFrame, false);',
        '//# sourceURL=post-message-listener.html',
        '</script>',
        '</body>',
        '</html>',
      ].join('\n');

      await session.evaluateAsync(frameHTML => {
        return new Promise((resolve, reject) => {
          const div = document.createElement('div');
          const iframe = document.createElement('iframe');
          iframe.id = 'iframe';
          iframe.width = '800';
          iframe.height = '100';
          iframe.style.border = '1px solid black';
          iframe.onload = resolve;
          iframe.onerror = () => reject(new Error('Failed to load iframe'));
          iframe.srcdoc = frameHTML;
          div.appendChild(iframe);
          document.body.appendChild(div);
        });
      }, frameHTML);

      await session.evaluate(`
          function testFunction()
          {
              setTimeout(timeout, 0);
          }

          function timeout()
          {
              window.addEventListener("message", onMessageReceivedInParent, false);
              postMessageToSelf();
              postMessageToFrame("start");
          }

          function onMessageReceivedInParent(e)
          {
              debugger;
              if (/data="start"/.test(e.data || ""))
                  postMessageToFrame("done");
          }

          function postMessageToSelf()
          {
              window.postMessage("message to myself", "*");
          }

          function postMessageToFrame(msg)
          {
              var iframe = document.getElementById("iframe");
              var win = iframe.contentWindow;
              win.postMessage(msg, "*");
          }
          //# sourceURL=async-callstack-post-message.js
      `);
    },
  });
});
