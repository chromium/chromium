import { bidi_log as log } from "./log.js";

export function createBidiClient(sendBidiResponse) {
    const messageHandlers = [];

    function _sendBidiMessage(message) {
        const messageStr = JSON.stringify(message);
        log("sent > " + messageStr);
        sendBidiResponse(messageStr);
    }

    function _jsonType(value) {
        if (value === null) {
            return 'null';
        }
        if (Array.isArray(value)) {
            return 'array';
        }
        return typeof value;
    }

    function _matchData(data) {
        let parsed;
        try {
            parsed = JSON.parse(data);
        } catch {
            throw new Error('Cannot parse data as JSON');
        }

        const parsedType = _jsonType(parsed);
        if (parsedType !== 'object') {
            throw new Error(`Expected JSON object but got ${parsedType}`);
        }

        // Extract amd validate id, method and params.
        const { id, method, params } = parsed;

        const idType = _jsonType(id);
        if (idType !== 'number' || !Number.isInteger(id) || id < 0) {
            // TODO: should uint64_t be the upper limit?
            // https://tools.ietf.org/html/rfc7049#section-2.1
            throw new Error(`Expected unsigned integer but got ${idType}`);
        }

        const methodType = _jsonType(method);
        if (methodType !== 'string') {
            throw new Error(`Expected string method but got ${methodType}`);
        }

        const paramsType = _jsonType(params);
        if (paramsType !== 'object') {
            throw new Error(`Expected object params but got ${paramsType}`);
        }

        return { id, method, params };
    }

    function _respondWithError(plainCommandData, errorCode, errorMessage) {
        const errorResponse = _getErrorResponse(plainCommandData, errorCode, errorMessage);
        _sendBidiMessage(errorResponse);
    }

    function _getErrorResponse(plainCommandData, errorCode, errorMessage) {
        // TODO: this is bizarre per spec. We reparse the payload and
        // extract the ID, regardless of what kind of value it was.
        let commandId = undefined;
        try {
            const commandData = JSON.parse(plainCommandData);
            if (_jsonType(commandData) === 'object' && 'id' in commandData) {
                commandId = commandData.id;
            }
        } catch { }

        return {
            id: commandId,
            error: errorCode,
            message: errorMessage,
            // TODO: optional stacktrace field.
        };
    }

    return {
        // Called via `Runtime.evaluate` from the bidi server side.
        onBidiMessageReceived: async function (messageStr) {
            log("received < " + messageStr);

            let commandData;
            try {
                commandData = _matchData(messageStr);
            } catch (e) {
                _respondWithError(messageStr, "invalid argument", e.message);
                return;
            }

            if (commandData) {
                for (let handler of messageHandlers)
                    handler(commandData);
            }
        },
        sendBidiMessage: _sendBidiMessage,
        setBidiMessageHandler: function (handler) {
            messageHandlers.push(handler);
        }

    };
};
