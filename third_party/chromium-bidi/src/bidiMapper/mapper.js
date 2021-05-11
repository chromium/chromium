import { createCdpClient } from "./cdpClient.js";
import {runBidiCommandsProcessor} from "./bidiCommandsProcessor.js";
import { createBidiClient } from "./bidiClient.js";
import { log } from "./log.js";


// `currentTargetId` is set by `setCurrentTargetId` + `Runtime.evaluate`.
let currentTargetId;
// `window.cdp` is exposed by `Target.exposeDevToolsProtocol`.
const cdpBinding = window.cdp;
// `window.sendBidiResponse` is exposed by `Runtime.addBinding`.
const sendBidiResponse = window.sendBidiResponse;
// Needed to filter out info related to BiDi target.
window.setCurrentTargetId = function (targetId) {
    log("current target ID: " + targetId);
    currentTargetId = targetId;
}

// Run via `Runtime.evaluate` from the bidi server side.
window.onBidiMessage = function (messageStr) {
    bidiClient.onBidiMessageReceived(messageStr);
};

const cdpClient = createCdpClient(cdpBinding);
const bidiClient = createBidiClient(sendBidiResponse);

const runBidiMapper = async function () {
    window.document.documentElement.innerHTML = `<h1>Bidi mapper runs here!</h1><h2>Don't close.</h2>
    <h3>BiDi:</h3>
    <pre id='bidi_log'></pre>
    <h3>CDP:</h3>
    <pre id='cdp_log'></pre>
    <h3>System:</h3>
    <pre id='system_log'></pre>`;

    window.document.title = "BiDi Mapper";

    await runBidiCommandsProcessor(cdpClient, bidiClient, () => currentTargetId);

    console.log("launched");
    log("launched");
    bidiClient.sendBidiMessage("launched");
};

runBidiMapper();
