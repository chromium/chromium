function createBidiCommandsProcessor(cdpClient, bidiClient, getCurrentTargetId) {
    const process_browsingContext_getTree = async function (messageId) {
        const cdpTargets = await cdpClient.sendCdpCommand({ method: "Target.getTargets" });
        const contexts = cdpTargets.targetInfos
            // Don't expose any information about the tab with Mapper running.
            .filter(t => t.targetId !== getCurrentTargetId())
            .map(t => ({
                context: t.targetId,
                parent: t.openerId ? t.openerId : null,
                url: t.url
            }));
        bidiClient.sendBidiMessage({
            id: messageId,
            result: { contexts }
        });
    };

    const progress_session_status = async function (messageId) {
        bidiClient.sendBidiMessage({
            id: messageId,
            result: { "ready": true, message: "ready" }
        });
    };

    const progress_unknown_command = async function (messageId) {
        bidiClient.sendBidiMessage({
            id,
            "error": "invalid argument",
            "message": "not supported operation"
        });
    };

    return {
        processCommand: async function (message) {
            const messageId = message.id;
            switch (message.method) {
                case 'session.status':
                    return progress_session_status(messageId);
                case 'browsingContext.getTree':
                    return process_browsingContext_getTree(messageId);
                default:
                    return progress_unknown_command(messageId);
            }
        }
    };
};