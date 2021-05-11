export async function runBidiCommandsProcessor(cdpClient, bidiClient, getCurrentTargetId) {
    const targets = {};

    const targetToContext = t => ({
        context: t.targetId,
        parent: t.openerId ? t.openerId : null,
        url: t.url
    });

    const process_browsingContext_getTree = async function (messageId) {
        const cdpTargets = await cdpClient.sendCdpCommand({ method: "Target.getTargets" });
        const contexts = cdpTargets.targetInfos
            // Don't expose any information about the tab with Mapper running.
            .filter(t => t.targetId !== getCurrentTargetId())
            .map(targetToContext);
        bidiClient.sendBidiMessage({
            id: messageId,
            result: { contexts }
        });
    };

    const process_PROTO_browsingContext_createContext = async function (message) {
        const { targetId } = await cdpClient.sendCdpCommand({
            method: "Target.createTarget",
            params: { url: message.params.url }
        });

        bidiClient.sendBidiMessage({
            id: message.id,
            result: targetToContext(targets[targetId])
        });
    };

    const process_session_status = async function (messageId) {
        bidiClient.sendBidiMessage({
            id: messageId,
            result: { "ready": true, message: "ready" }
        });
    };

    const process_unknown_command = async function (messageId) {
        bidiClient.sendBidiMessage({
            id: messageId,
            "error": "invalid argument",
            "message": "not supported operation"
        });
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
        delete targets[message.params.targetId];
        bidiClient.sendBidiMessage(
            {
                method: 'browsingContext.contextDestroyed',
                params: { context: message.params.targetId }
            });
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

    const onBidiMessage = function (message) {
        const messageId = message.id;
        switch (message.method) {
            case 'session.status':
                process_session_status(messageId);
                return;
            case 'browsingContext.getTree':
                process_browsingContext_getTree(messageId);
                return;
            case 'PROTO.browsingContext.createContext':
                process_PROTO_browsingContext_createContext(message);
                return;
            default:
                process_unknown_command(messageId);
                return;
        }
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