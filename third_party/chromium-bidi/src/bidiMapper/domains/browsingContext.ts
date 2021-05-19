import { log } from "../log";
const logContext = log("context");

export default class Context {
    private static _contexts: Map<string, Context> = new Map();

    // Set from outside.
    static cdpClient: any;
    static getCurrentContextId: () => string;

    static onContextCreated: (t: Context) => Promise<void>;
    static onContextDestroyed: (t: Context) => Promise<void>;

    private static _getOrCreateContext(contextId: string): Context {
        if (!Context._contexts.has(contextId))
            Context._contexts.set(contextId, new Context(contextId));
        return Context._contexts.get(contextId);
    }

    static handleAttachedToTargetEvent(eventData: any) {
        logContext("AttachedToTarget event recevied", eventData);

        const targetInfo: TargetInfo = eventData.params.targetInfo;
        if (!Context._isValidTarget(targetInfo))
            return;

        const context = Context._getOrCreateContext(targetInfo.targetId);
        context._updateTargetInfo(targetInfo);
        Context.onContextCreated(context);
    }

    static handleDetachedFromTargetEvent(eventData: any) {
        logContext("detachedFromTarget event recevied", eventData);

        const target: TargetInfo = eventData.params.targetInfo;
        if (!Context._isValidTarget(target))
            return;

        const context = Context._getOrCreateContext(target.targetId);
        Context.onContextDestroyed(context);

        delete Context._contexts[context.contextId];
    }

    static async process_createContext(params: any): Promise<any> {
        const { targetId } = await Context.cdpClient.sendCdpCommand({
            method: "Target.createTarget",
            params: { url: params.url }
        });
        return Context._getOrCreateContext(targetId).toBidi();
    };

    static _isValidTarget = (target: TargetInfo) => {
        if (target.targetId === Context.getCurrentContextId())
            return false;
        if (!target.type || target.type !== "page")
            return false;
        return true;
    }

    private _targetInfo: TargetInfo;
    private contextId: string;

    constructor(contextId: string) {
        Context._contexts[contextId] = this;
    }

    private _updateTargetInfo(targetInfo: TargetInfo) {
        this._targetInfo = targetInfo;
    }

    toBidi(): any {
        return {
            context: this._targetInfo.targetId,
            parent: this._targetInfo.openerId ? this._targetInfo.openerId : null,
            url: this._targetInfo.url
        };
    }
}

class TargetInfo {
    targetId: string
    openerId: string | null
    url: string | null
    type: string
}