# Fenced Frames

This directory contains [Web Platform
Tests](third_party/blink/web_tests/external/wpt) for the [Fenced
Frames](https://github.com/shivanigithub/fenced-frame) feature.

These tests are generally intended to be upstreamed to the Web Platform Tests
repository (i.e., moved from `wpt_internal/fenced_frame/` to `external/wpt/`).
There are a few reasons why we're holding off doing that right now, see [Fenced
Frames Testing Plan > Web Platform Tests](https://docs.google.com/document/d/1A4Dkw8PesXSqmRLy2Xa-KxpXgIZUT4rPocbxMBuP_3E/edit#heading=h.3plnzof3mgvv).

In general, these tests should follow Chromium's [web tests
guidelines](docs/testing/web_tests_tips.md) and [web-platform-tests
guidelines](/docs/testing/web_platform_tests.md). This document describes
how to use the specific fenced frame testing infrastructure.

## How to write tests

The `<fencedframe>` element has a strict requirement that it cannot directly
communicate with or reach its embedder document. The fenced frame does have
network access however, so we use a server-side stash to communicate with the
outer page via message passing. Message passing is done by using the helpers
defined in
[resources/utils.js](third_party/blink/web_tests/wpt_internal/fenced_frame/resources/utils.js)
to send a message to the server, and poll the server for a response. All
messages have a unique key associated with them so that documents that want to
receive messages can poll the server for a given message that can be identified
by a unique key.

Let's see an example of sending a message to the server that a fenced frame will
receive and respond to.

**outer-page.js:**
```js
promise_test(async () => {
  const important_message_key = KEYS["important_message"];
  const important_value = "Hello";
  writeValueToServer(important_message_key, important_value);

  // Now that the message has been sent to the fenced frame, let's wait for its
  // ACK, so that we don't exit the test before the fenced frame gets the
  // message.
  const fenced_frame_ack_key = KEYS["fenced_frame_ack"];
  const response_from_fenced_frame = await
      nextValueFromServer(fenced_frame_ack_key);
  assert_equals(response_from_fenced_frame, "Hello to you too",
      "The fenced frame received the message, and said hello back to us");
}, "Fenced frame and receive and send a greeting");
```

**inner-fenced-frame.js:**

```js
async function init() { // Needed in order to use top-level await.
  const important_message_key = KEYS["important_message"];
  const greeting_from_embedder = await nextValueFromServer(important_message_key);

  const fenced_frame_ack_key = KEYS["fenced_frame_ack"];
  if (greeting_from_embedder == "Hello") {
    // Message that we received was expected.
    writeValueToServer(fenced_frame_ack_key, "Hello to you too");
  } else {
    // Message that we received was *not* expected, let's report an error to the
    // outer page so it fails the test.
    writeValueToServer(fenced_frame_ack_key, "Unexpected message");
  }
}

init();
```

When you write a new web platform test, it will likely involve passing a _new_
message like the messages above, to and from the fenced frame. In that case,
please add a new key to the `KEYS` object in
[resources/utils.js](third_party/blink/web_tests/wpt_internal/fenced_frame/resources/utils.js)
and see the documentation there for why. You may have to add a new _pair_ of
keys as well, so that when one document writes a message associated with one
unique key, it can listen for an ACK from the receiving document, so that it
doesn't write over the message again before the receiving document actually
reads it. **No two tests should ever use the same key to communicate information
to and from a fenced frame**, as this will cause server-side race conditions.

For a good test example, see
[window-parent.html](window-parent.html).

## Underlying implementations

This directory contains <fencedframe> tests that exercise the
`blink::features::kFencedFrames` feature. Specifically, they exercise the
default implementation mode of fenced frames, which is
`blink::features::FencedFramesImplementationType::kShadowDOM`.

The test are also run exercising the MPArch implementation path
(`blink::features::FencedFramesImplementationType::kMPArch`) via the virtual
test suite (see the `VirtualTestSuites` file).

## Wrap lines at 80 columns

This is the convention for most Chromium/WPT style tests. Note that
`git cl format [--js]` does not reformat js code in .html files.
