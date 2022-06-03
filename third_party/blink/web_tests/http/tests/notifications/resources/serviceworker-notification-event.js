importScripts('../../serviceworker/resources/worker-testharness.js');

let messagePort = null;
addEventListener('message', workerEvent => {
    messagePort = workerEvent.data;
    messagePort.postMessage('ready');
});

addEventListener('notificationclick', e => runTest(e.notification));

// Test body for the serviceworker-notification-event.html layout test.
function runTest(notification) {
    const result = {
      success: true,
      message: null
    }
    try {
      assert_true('NotificationEvent' in self);

      assert_throws_js(TypeError, () => new NotificationEvent('NotificationEvent'));
      assert_throws_js(TypeError, () => new NotificationEvent('NotificationEvent', {}));
      assert_throws_js(TypeError, () => new NotificationEvent('NotificationEvent', { notification: null }));

      const event = new NotificationEvent('NotificationEvent', { notification });

      assert_equals(event.type, 'NotificationEvent');
      assert_idl_attribute(event, 'notification');
      assert_idl_attribute(event, 'action');
      assert_equals(event.cancelable, false);
      assert_equals(event.bubbles, false);
      assert_equals(event.notification, notification);
      assert_equals(event.action, '');
      assert_equals(event.reply, '');
      assert_inherits(event, 'waitUntil');

      const customEvent = new NotificationEvent('NotificationEvent', {
                              notification: notification,
                              reply: 'my reply',
                              bubbles: true,
                              cancelable: true });

      assert_equals(customEvent.type, 'NotificationEvent');
      assert_equals(customEvent.cancelable, true);
      assert_equals(customEvent.bubbles, true);
      assert_equals(customEvent.notification, notification);
      assert_equals(customEvent.reply, 'my reply');
    } catch (e) {
      result.success = false;
      result.message = e.message + '\n' + e.stack;
    }
    // Signal to the document that the test has finished running.
    messagePort.postMessage(result);
}
