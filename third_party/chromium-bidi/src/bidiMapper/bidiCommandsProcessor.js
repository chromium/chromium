export async function runBidiCommandsProcessor(cdpClient, bidiClient, getCurrentTargetId) {
    const targets = {};

    const getErrorResponse = (commandData, errorCode, errorMessage) => {
        // TODO: this is bizarre per spec. We reparse the payload and
        // extract the ID, regardless of what kind of value it was.
        let commandId = undefined;
        try {
                commandId = commandData.id;
        } catch { }

        return {
            id: commandId,
            error: errorCode,
            message: errorMessage,
            // TODO: optional stacktrace field.
        };
    };

    const respondWithError = (commandData, errorCode, errorMessage) => {
        const errorResponse = getErrorResponse(commandData, errorCode, errorMessage);
        bidiClient.sendBidiMessage(errorResponse);
    };


    const targetToContext = t => ({
        context: t.targetId,
        parent: t.openerId ? t.openerId : null,
        url: t.url
    });

    const process_browsingContext_getTree = async function (params) {
        const cdpTargets = await cdpClient.sendCdpCommand({ method: "Target.getTargets" });
        const contexts = cdpTargets.targetInfos
            // Don't expose any information about the tab with Mapper running.
            .filter(t => t.targetId !== getCurrentTargetId())
            .map(targetToContext);
        return { contexts };
    };

    const process_PROTO_browsingContext_createContext = async function (params) {
        const { targetId } = await cdpClient.sendCdpCommand({
            method: "Target.createTarget",
            params: { url: params.url }
        });
        return targetToContext(targets[targetId]);
    };

    const process_DEBUG_Page_close = async function (params) {
        await cdpClient.sendCdpCommand({
            method: "Target.closeTarget",
            params: { targetId: params.context }
        });
        return {};
    };

    const process_session_status = async function (params) {
        return { "ready": true, message: "ready" };
    };

    const handle_target_attachedToTarget = (message) => {
        targets[message.params.targetInfo.targetId] = message.params.targetInfo;
        bidiClient.sendBidiMessage(
            {
                method: 'browsingContext.contextCreated',
                params: targetToContext(message.params.targetInfo)
            });
    }

    const handle_target_detachedFromTarget = (message) => {
        bidiClient.sendBidiMessage(
            {
                method: 'browsingContext.contextDestroyed',
                params: targetToContext(targets[message.params.targetId])
            });
        delete targets[message.params.targetId];
    }

    const onCdpMessage = function (message) {
        switch (message.method) {
            case "Target.attachedToTarget":
                handle_target_attachedToTarget(message);
                return;
            case "Target.detachedFromTarget":
                handle_target_detachedFromTarget(message);
                return;
        }
    };

    const processCommand = async (commandData) => {
        const response = {};
        response.id = commandData.id;

        switch (commandData.method) {
            case 'session.status':
                return await process_session_status(commandData.params);
            case 'browsingContext.getTree':
                return await process_browsingContext_getTree(commandData.params);

            case 'PROTO.browsingContext.createContext':
                return await process_PROTO_browsingContext_createContext(commandData.params);

            case 'DEBUG.Page.close':
                return await process_DEBUG_Page_close(commandData.params);

            default:
                throw new Error('unknown command');
        }
    };

    const onBidiMessage = async (message) => {
        processCommand(message).then(result => {
            const response = {
                id: message.id,
                result
            };

            bidiClient.sendBidiMessage(response);
        }).catch(e => {
            console.error(e);
            respondWithError(message, "unknown error", e.message);
        });
    }

    cdpClient.setCdpMessageHandler(onCdpMessage);
    bidiClient.setBidiMessageHandler(onBidiMessage);

    await cdpClient.sendCdpCommand({
        method: "Target.setAutoAttach",
        params: {
            autoAttach: true,
            waitForDebuggerOnStart: false,
            flatten: true
        }
    });
};