import { bidi_log as log } from "./log.js";

export function createBidiClient(sendBidiResponse) {
    const messageHandlers = [];

    const _sendBidiMessage = function (message) {
        const messageStr = JSON.stringify(message);
        log("sent > " + messageStr);
        sendBidiResponse(messageStr);
    }

    return {
        // Called via `Runtime.evaluate` from the bidi server side.
        onBidiMessageReceived: async function (messageStr) {
            log("received < " + messageStr);
            let message;
            try {
                message = JSON.parse(messageStr);
            } catch {
                _sendBidiMessage({
                    "error": "invalid argument",
                    "message": "Cannot parse data as JSON"
                });
                return;
            }

            if (message) {
                for (let handler of messageHandlers)
                    handler(message);
            }
        },
        sendBidiMessage: _sendBidiMessage,
        setBidiMessageHandler: function (handler) {
            messageHandlers.push(handler);
        }

    };
};
