`use strict`;
const WebSocket = require('ws');
const debug = require('debug');

const debugInternal = debug('bidiServer');
const debugSend = debug('biDiServer:SEND ►');
const debugRecv = debug('biDiServer:RECV ◀');

const biDiPort = process.env.BIDI_PORT || 8080;

module.exports = {
    create: async function (onMessage) {
        return await bidiServer(onMessage);
    }
};

const bidiServer = async (onMessage) => {
    const _onMessage = onMessage;
    let _bidiWsConnection;

    debugInternal("launching bidi websocket server");

    const wss = new WebSocket.Server({
        port: biDiPort,
    });

    debugInternal("bidi websocket server launched on port ", biDiPort);

    await new Promise((resolve) => {
        wss.on('connection', function connection(bidiWsConnection) {
            _bidiWsConnection = bidiWsConnection
            debugInternal("bidi websocket server connected");

            // Proxy BiDi messages to internal connection.
            _bidiWsConnection.on('message', message => {
                debugRecv(message);
                _onMessage(message);
            });

            resolve();
        });
    });

    return {
        sendMessage: async function (message) {
            debugSend(message);
            _bidiWsConnection.send(message);
        },
    }
};