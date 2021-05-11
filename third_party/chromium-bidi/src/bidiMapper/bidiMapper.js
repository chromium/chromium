// import createCdpClient from "./cdpClient.js";
// import createBidiCommandsProcessor from "./bidiCommandsProcessor.js";
// import createBidiClient from "./bidiClient.js";

// `currentTargetId` is set by `setCurrentTargetId` + `Runtime.evaluate`.
let currentTargetId;
// `window.cdp` is exposed by `Target.exposeDevToolsProtocol`.
const cdpBinding = window.cdp;
// `window.sendBidiResponse` is exposed by `Runtime.addBinding`.
const sendBidiResponse = window.sendBidiResponse;
// Run via `Runtime.evaluate` from the bidi server side.

const cdpClient = createCdpClient(cdpBinding, (message) => {
    writeLog("not handled CDP message", message);
});

const bidiClient = createBidiClient(sendBidiResponse);

// Run via `Runtime.evaluate` from the bidi server side.
const onBidiMessage = function (messageStr) {
    bidiClient.onBidiMessageReceived(messageStr);
};

const bidiCommandsProcessor = createBidiCommandsProcessor(cdpClient, bidiClient, () => currentTargetId);

const runBidiMapper = function () {

    window.document.documentElement.innerHTML = "<h1>Bidi mapper runs here!</h1><h2>Don't close.</h2><pre id='log' />";
    window.document.title = "BiDi Mapper";

    console.log("launched");
    writeLog("launched");
    bidiClient.sendBidiMessage("launched");
};

// Needed to filter out info related to BiDi target.
const setCurrentTargetId = function (targetId) {
    writeLog("current target ID: " + targetId);
    currentTargetId = targetId;
}

runBidiMapper();
