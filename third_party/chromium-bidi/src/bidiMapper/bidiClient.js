function createBidiClient(sendBidiResponse) {
    const _sendBidiMessage = function (message) {
        const messageStr = JSON.stringify(message);
        writeLog("sending Bidi command: " + messageStr);
        sendBidiResponse(messageStr);
    }

    return {
        onBidiMessageReceived: async function (messageStr) {
            writeLog("Bidi message received: " + messageStr);
            let message;
            try {
                message = JSON.parse(messageStr);
            } catch {
                _sendBidiMessage({
                    "error": "invalid argument",
                    "message": "not supported type"
                });
                return;
            }

            if (message)
                return bidiCommandsProcessor.processCommand(message);
         },
        sendBidiMessage: _sendBidiMessage
    };
};
