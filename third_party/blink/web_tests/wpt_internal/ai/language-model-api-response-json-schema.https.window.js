// META: title=Language Model Response JSON Schema
// META: script=resources/utils.js
// META: timeout=long

'use strict';

promise_test(async t => {
  await ensureLanguageModel();
  const session = await LanguageModel.create();
  // Circular reference is not valid.
  const invalidRepsonseJsonSchema = {};
  invalidRepsonseJsonSchema.self = invalidRepsonseJsonSchema;
  await promise_rejects_dom(t, 'NotSupportedError',
    session.prompt(kTestPrompt, { responseConstraint: invalidRepsonseJsonSchema }),
    'Response json schema is invalid - it should be an object that can be stringified into a JSON string.');
}, 'Prompt API should fail if an invalid response json schema is provided');

promise_test(async t => {
  await ensureLanguageModel();
  const session = await LanguageModel.create();
  const validRepsonseJsonSchema = {
    type: "object",
    required: ["Rating"],
    additionalProperties: false,
    properties: {
      Rating: {
        type: "number",
        minimum: 0,
        maximum: 5,
      },
    },
  };
  const promptPromise = session.prompt(kTestPrompt, { responseConstraint : validRepsonseJsonSchema });
  const result = await promptPromise;
  assert_true(typeof result === "string");
}, 'Prompt API should work when a valid response json schema is provided.');

promise_test(async t => {
  await ensureLanguageModel();
  const session = await LanguageModel.create();
  const promptPromise = session.prompt(kTestPrompt, { responseConstraint : /hello/ });
  const result = await promptPromise;
  assert_true(typeof result === "string");
}, 'Prompt API should work when a valid regex constraint is provided.');
