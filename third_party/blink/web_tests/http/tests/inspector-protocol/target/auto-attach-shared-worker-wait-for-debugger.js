(async function (/** @type {import('test_runner').TestRunner} */ testRunner) {
    const { session, dp } = await testRunner.startBlank('Tests auto-attach of shared workers.');

    const bp = testRunner.browserP();
    await bp.Target.setAutoAttach(
        { autoAttach: true, waitForDebuggerOnStart: true, flatten: true });

    const attachedPromise = bp.Target.onceAttachedToTarget(
        e => e.params.targetInfo.type === 'shared_worker');

    session.evaluate(`
        const workerScript = 'console.log("From worker");';
        const blob = new Blob([workerScript], { type: 'application/javascript' });
        const worker = new SharedWorker(URL.createObjectURL(blob));
        worker.port.start();`);

    const { params: { sessionId, targetInfo, waitingForDebugger } } = await attachedPromise;
    const workerDp = session.createChild(sessionId).protocol;
    const logsPromise = new Promise(resolve => {
        const logs = [];
        workerDp.Runtime.onConsoleAPICalled(event => {
            const text = event.params.args.map(arg => arg.value).join(' ');
            testRunner.log(`Console from worker: ${text}`);
            logs.push(text);
            if (logs.length === 2) {
                resolve();
            }
        });
    });

    await workerDp.Runtime.enable();

    await workerDp.Runtime.evaluate({
        expression: `console.log("From CDP: ${targetInfo.type}", ${waitingForDebugger});`
    });

    await workerDp.Runtime.runIfWaitingForDebugger();

    await logsPromise;

    testRunner.completeTest();
});
