// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(async function(/** @type {import('test_runner').TestRunner} */ testRunner) {
  const {session, dp} =
      await testRunner.startBlank('Test trace events for window.crypto encryption and decryption.');

  const TracingHelper =
      await testRunner.loadScript('../resources/tracing-test.js');
  const Phase = TracingHelper.Phase;

  const tracingHelper = new TracingHelper(testRunner, session);

  await tracingHelper.startTracing('disabled-by-default-devtools.timeline');
  await session.evaluateAsync(`
     async function run() {
        const key = await window.crypto.subtle.generateKey({name: "AES-CBC", length: 256}, false, ["encrypt", "decrypt"]);
        const data = new Uint8Array(512);
        const iv = window.crypto.getRandomValues(new Uint8Array(16));
        const crypted = await window.crypto.subtle.encrypt({name: "AES-CBC", iv}, key, data);
        return window.crypto.subtle.decrypt({name: "AES-CBC", iv}, key, crypted);
    }
    run();
  `);
  const events = await tracingHelper.stopTracing(/disabled-by-default-devtools\.timeline/);

  const doEncryptEvent = tracingHelper.findEvent('DoEncrypt', Phase.COMPLETE);
  testRunner.log('Found DoEncrypt event:');
  tracingHelper.logEventShape(doEncryptEvent);

  const doEncryptReplyEvent = tracingHelper.findEvent('DoEncryptReply', Phase.COMPLETE);
  testRunner.log('Found DoEncryptReply event:');
  tracingHelper.logEventShape(doEncryptReplyEvent);

  const doDecryptEvent = tracingHelper.findEvent('DoDecrypt', Phase.COMPLETE);
  testRunner.log('Found DoDecrypt event:');
  tracingHelper.logEventShape(doDecryptEvent);

  const doDecryptReplyEvent = tracingHelper.findEvent('DoDecryptReply', Phase.COMPLETE);
  testRunner.log('Found DoDecryptReply event:');
  tracingHelper.logEventShape(doDecryptReplyEvent);

  testRunner.completeTest();
});
