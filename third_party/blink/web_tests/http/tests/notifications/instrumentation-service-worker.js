// Deep-copies the attributes of |notification|. Note that the
// robustness of this function (and also |assert_object_equals| in
// testharness.js) affects the types of possible testing can be done.
// TODO(peter): change this to a structured clone algorithm.
function cloneNotification(notification) {
    function deepCopy(src) {
        if (typeof src !== 'object' || src === null)
            return src;
        var dst = Array.isArray(src) ? [] : {};
        for (var property in src) {
            if (typeof src[property] === 'function')
                continue;
            dst[property] = deepCopy(src[property]);
        }
        return dst;
    }

    return deepCopy(notification);
}

// Deserializes a trigger object sent via postMessage.
function deserializeTrigger(trigger) {
    if (trigger && trigger.timestamp)
        return new TimestampTrigger(trigger.timestamp);
    return trigger;
}

// Deserializes notification options sent via postMessage.
function deserializeOptions(options) {
    return {
        ...options,
        showTrigger: deserializeTrigger(options.showTrigger),
    };
}

// Allows a document to exercise the Notifications API within a service worker by sending commands.
var messagePort = null;

// All urls of requests that have been routed through the fetch event handler.
var fetchHistory = [];

addEventListener('install', event => {
    event.waitUntil(skipWaiting());
});

addEventListener('activate', event => {
    event.waitUntil(clients.claim());
});

addEventListener('message', workerEvent => {
    messagePort = workerEvent.data;

    // Listen to incoming commands on the message port.
    messagePort.onmessage = event => {
        if (typeof event.data != 'object' || !event.data.command)
            return;

        switch (event.data.command) {
            case 'permission':
                messagePort.postMessage({ command: event.data.command,
                                          value: Notification.permission });
                break;

            case 'show':
                registration.showNotification(event.data.title, deserializeOptions(event.data.options)).then(() => {
                    messagePort.postMessage({ command: event.data.command,
                                              success: true });
                }, error => {
                    messagePort.postMessage({ command: event.data.command,
                                              success: false,
                                              message: error.message });
                });
                break;

            case 'get-fetch-history':
                messagePort.postMessage({ command: event.data.command,
                                          fetchHistory: fetchHistory });
                break;

            case 'get':
                var filter = {};
                if (typeof (event.data.filter) !== 'undefined')
                    filter = event.data.filter;

                registration.getNotifications(filter).then(notifications => {
                    var clonedNotifications = [];
                    for (var notification of notifications)
                        clonedNotifications.push(cloneNotification(notification));

                    messagePort.postMessage({ command: event.data.command,
                                              success: true,
                                              notifications: clonedNotifications });
                }, error => {
                    messagePort.postMessage({ command: event.data.command,
                                              success: false,
                                              message: error.message });
                });
                break;

            case 'request-permission-exists':
                messagePort.postMessage({ command: event.data.command,
                                          value: 'requestPermission' in Notification });
                break;

            default:
                messagePort.postMessage({ command: 'error', message: 'Invalid command: ' + event.data.command });
                break;
        }
    };

    // Notify the controller that the worker is now available.
    messagePort.postMessage('ready');
});

addEventListener('notificationclick', event => {
    var notificationCopy = cloneNotification(event.notification);

    // Notifications containing "ACTION:CLOSE" in their message will be closed
    // immediately by the Service Worker.
    if (event.notification.body.indexOf('ACTION:CLOSE') != -1)
        event.notification.close();

    // Notifications containing "ACTION:OPENWINDOW" in their message will attempt
    // to open a new window for an example URL.
    if (event.notification.body.indexOf('ACTION:OPENWINDOW') != -1)
        event.waitUntil(clients.openWindow('https://example.com/'));

    messagePort.postMessage({ command: 'click',
                              notification: notificationCopy,
                              action: event.action,
                              reply: event.reply });
});

addEventListener('notificationclose', event => {
    var notificationCopy = cloneNotification(event.notification);
    messagePort.postMessage({ command: 'close',
                              notification: notificationCopy });
});

addEventListener('fetch', event => {
    fetchHistory.push(event.request.url);
    event.respondWith(fetch(event.request));
});
