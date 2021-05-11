function createCdpClient(cdpBinding, onCdpMessage) {
    const commandCallbacks = [];

    const _onCdpMessage = function (messageStr) {
        writeLog("bidiMapperScript got a CDP message: " + messageStr);

        const message = JSON.parse(messageStr);

        if (commandCallbacks.hasOwnProperty(message.id)) {
            commandCallbacks[message.id](message.result);
        } else {
            onCdpMessage(message);
        }
    };

    cdpBinding.onmessage = _onCdpMessage;


    return {
        sendCdpCommand: async function (cdpCommand) {
            const id = commandCallbacks.length;
            cdpCommand.id = id;

            cdpBinding.send(JSON.stringify(cdpCommand));
            return new Promise(resolve => {
                commandCallbacks[id] = resolve;
            });
        }
    }
}