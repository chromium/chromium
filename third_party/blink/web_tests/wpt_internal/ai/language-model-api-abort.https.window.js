// META: title=Language Model Abort
// META: script=resources/utils.js
// META: timeout=long

'use strict';

promise_test(async t => {
  await testAbortPromise(t, signal => {
    return LanguageModel.create({
      signal: signal
    });
  });
}, "Aborting LanguageModel.create().");

promise_test(async t => {
  const session = await LanguageModel.create();
  await testAbortPromise(t, signal => {
    return session.clone({
      signal: signal
    });
  });
}, "Aborting LanguageModel.clone().");

promise_test(async t => {
  const session = await LanguageModel.create();
  await testAbortPromise(t, signal => {
    return session.prompt(kTestPrompt, { signal: signal });
  });
}, "Aborting LanguageModel.prompt().");

promise_test(async t => {
  const session = await LanguageModel.create();
  await testAbortReadableStream(t, signal => {
    return session.promptStreaming(
      kTestPrompt, { signal: signal }
    );
  });
}, "Aborting LanguageModel.promptStreaming().");
