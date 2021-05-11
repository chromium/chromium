const debug = require('debug');

const debugInternal = debug('bidiMapper');
const debugSend = debug('bidiMapper:SEND ►');
const debugRecv = debug('bidiMapper:RECV ◀');
const debugRecvUnknown = debug('bidiMapper:RECV unknown ◀');
const debugConsole = debug('bidiMapper:remoteConsole ◀');

const WebSocket = require('ws');

module.exports = {
    create: async function (mapperContent, cdpUrl, onBidiMessage) {
        return await bidiMapperSession(mapperContent, cdpUrl, onBidiMessage);
    }
};

const bidiMapperSession = async (mapperContent, cdpUrl, onBidiMessage) => {
    const _commandCallbacks = [];
    let ws;
    let sessionId;

    const _establishSession = async function () {
        return new Promise(resolve => {
            debugInternal("Establiushing session with cdpUrl: ", cdpUrl);

            ws = new WebSocket(cdpUrl);

            ws.on('open', () => {
                debugInternal("Session established.");
                resolve();
            });

            ws.on('message', async function incoming(dataStr) {
                const data = JSON.parse(dataStr);
                debugRecv(data);
                if (_commandCallbacks.hasOwnProperty(data.id)) {
                    await _commandCallbacks[data.id](data.result);
                    return;
                } else {
                    if (data.method === "Runtime.consoleAPICalled") {
                        _onConsoleMessage(data)
                        return;
                    }
                    if (data.method === "Runtime.bindingCalled" && data.params.name === "sendBidiResponse") {
                        onBidiMessage(data.params.payload);
                        return;
                    }
                    else
                        debugRecvUnknown(data);
                }
            });
        });
    };
    const _sendCommand = async function (command) {
        return new Promise((resolve) => {
            const id = _commandCallbacks.length;
            _commandCallbacks[id] = resolve;
            command.id = id;
            debugSend(command);
            ws.send(JSON.stringify(command));
        });
    };
    const _onConsoleMessage = async function (data) {
        debugConsole(data.params.args);
    };

    //////////////////////

    await _establishSession();

    debugInternal("Connection opened.");

    await _sendCommand({
        method: "Log.enable"
    })

    const targetId = (
        await _sendCommand({
            method: "Target.createTarget",
            params: {
                url: "about:blank"
            }
        })).targetId;

    sessionId = (await _sendCommand({
        "method": "Target.attachToTarget",
        "params": {
            "targetId": targetId,
            "flatten": true
        }
    })).sessionId;

    await _sendCommand({
        method: "Runtime.enable",
        sessionId
    })


    await _sendCommand({
        method: "Target.exposeDevToolsProtocol",
        params: {
            bindingName: "cdp",
            targetId
        }
    });

    await _sendCommand({
        method: "Runtime.addBinding",
        sessionId,
        params: {
            name: "sendBidiResponse"
        }
    });

    await _sendCommand({
        method: "Runtime.evaluate",
        sessionId,
        params: {
            expression: mapperContent
        }
    })

    // Let Mapper know what is it's TargetId to filter out related targets.
    await _sendCommand({
        method: "Runtime.evaluate",
        sessionId,
        params: {
            expression: "window.setCurrentTargetId(" + JSON.stringify(targetId) + ")"
        }
    });

    debugInternal("Launched!");

    return {
        sendBidiCommand: async function (message) {
            return _sendCommand({
                method: "Runtime.evaluate",
                sessionId,
                params: {
                    expression: "onBidiMessage(" + JSON.stringify(message) + ")"
                }
            });
        }
    }
}
