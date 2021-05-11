import { cdp_log as log } from "./log.js";

export function createCdpClient(cdpBinding) {
    const commandCallbacks = [];
    const messageHandlers = [];

    const _onCdpMessage = function (messageStr) {
        log("received < " + messageStr);

        const message = JSON.parse(messageStr);

        if (commandCallbacks.hasOwnProperty(message.id)) {
            commandCallbacks[message.id](message.result);
        } else {
            for (let handler of messageHandlers)
                handler(message);
        }
    };

    cdpBinding.onmessage = _onCdpMessage;

    return {
        sendCdpCommand: async function (cdpCommand) {
            const id = commandCallbacks.length;
            cdpCommand.id = id;

            const cdpCommandStr = JSON.stringify(cdpCommand);
            log("sent > " + cdpCommandStr);
            cdpBinding.send(cdpCommandStr);
            return new Promise(resolve => {
                commandCallbacks[id] = resolve;
            });
        },
        setCdpMessageHandler: function (handler) {
            messageHandlers.push(handler);
        }
    }
}