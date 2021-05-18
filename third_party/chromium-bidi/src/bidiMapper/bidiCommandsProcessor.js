export async function runBidiCommandsProcessor(cdpClient, bidiClient, getCurrentTargetId) {
    const targets = {};

    const _isValidTarget = (target) => {
        if (target.targetId === getCurrentTargetId())
            return false;
        if (!target.type || target.type !== "page")
            return false;
        return true;
    }

    const _getErrorResponse = (commandData, errorCode, errorMessage) => {
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

    const _respondWithError = (commandData, errorCode, errorMessage) => {
        const errorResponse = _getErrorResponse(commandData, errorCode, errorMessage);
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
            .filter(_isValidTarget)
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
        const target = message.params.targetInfo;
        if (!_isValidTarget(target))
            return;

        targets[target.targetId] = target;
        bidiClient.sendBidiMessage(
            {
                method: 'browsingContext.contextCreated',
                params: targetToContext(target)
            });
    }

    const handle_target_detachedFromTarget = (message) => {
        const targetId = message.params.targetId;
        if (!targets.hasOwnProperty(targetId))
            return;
        bidiClient.sendBidiMessage(
            {
                method: 'browsingContext.contextDestroyed',
                params: targetToContext(targets[targetId])
            });
        delete targets[targetId];
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
        await processCommand(message)
            .then(result => {
                const response = {
                    id: message.id,
                    result
                };

                bidiClient.sendBidiMessage(response);
            }).catch(e => {
                console.error(e);
                _respondWithError(message, "unknown error", e.message);
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