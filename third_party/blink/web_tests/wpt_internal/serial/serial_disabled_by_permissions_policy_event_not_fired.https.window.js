// META: script=/resources/testdriver.js
// META: script=/resources/test-only-api.js
// META: script=resources/automation.js

async function sleep(timeout) {
    return new Promise(resolve => {
      step_timeout(() => {
        resolve();
      }, timeout);
    });
  }

serial_test(async (t, fake) => {
    let eventWatcher = new EventWatcher(t, navigator.serial, 'connect');

    // This isn't necessary as the expected scenario shouldn't send any mojo
    // request. However, in order to capture a bug that doesn't reject adding
    // event listener, time delay here is to allow mojo request to be intercepted
    // after adding connect event listener.
    await sleep(100);

    // If device connect event fires, EventWatcher will assert for an unexpected
    // event.
    fake.addPort();

    // Time delay here is to allow event to be fired if any.
    await sleep(100);
  }, 'Connect event is not fired when serial is disallowed.');
