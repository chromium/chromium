// META: title=UseCounters for languageModel.prompt() options
// META: script=/resources/testdriver.js
// META: script=/resources/testdriver-vendor.js
// META: script=/ai/resources/util.js
// META: timeout=long

const kResponseConstraint = 5885;
const kPrefix = 5886;

promise_test(async t => {
  const session = await createLanguageModel();
  assert_false(internals.isUseCounted(document, kResponseConstraint));
  assert_false(internals.isUseCounted(document, kPrefix));

  // Call prompt with a basic message, no prefix, no responseConstraint.
  await session.prompt("Hello world");

  assert_false(internals.isUseCounted(document, kResponseConstraint));
  assert_false(internals.isUseCounted(document, kPrefix));
}, "UseCounters are NOT triggered when options are absent.");

promise_test(async t => {
  const session = await createLanguageModel();

  await session.prompt("Say hello",
    {
      responseConstraint: {
        type: "object",
        properties: { greeting: { type: "string" } }
      }
    }
  );

  assert_true(internals.isUseCounted(document, kResponseConstraint));
}, "ResponseConstraint UseCounter is triggered when responseConstraint is used.");

promise_test(async t => {
  const session = await createLanguageModel();

  await session.prompt([
      { role: "user", content: "Say hello" },
      { role: "assistant", content: "Hello", prefix: true }
    ]
  );

  assert_true(internals.isUseCounted(document, kPrefix));
}, "Prefix UseCounter is triggered when prefix is used.");
