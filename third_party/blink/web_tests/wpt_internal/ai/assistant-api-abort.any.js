// META: script=resources/workaround-for-362676838.js
// META: script=resources/utils.js

promise_test(async (t) => {
  const controller = new AbortController();
  const createPromise = ai.assistant.create({ signal: controller.signal });
  controller.abort();
  await promise_rejects_dom(t, 'AbortError', createPromise);
}, "Aborting AIAssistantFactory.create()");

promise_test(async (t) => {
  const controller = new AbortController();
  const session = await ai.assistant.create();
  const clonePromise = session.clone({ signal: controller.signal });
  controller.abort();
  await promise_rejects_dom(t, 'AbortError', clonePromise);
}, "Aborting AIAssistant.clone");

promise_test(async (t) => {
  const controller = new AbortController();
  const session = await ai.assistant.create();
  const promptPromise = session.prompt(
    "Write a poem", { signal: controller.signal }
  );
  controller.abort();
  await promise_rejects_dom(t, 'AbortError', promptPromise);
}, "Aborting AIAssistant.prompt");
