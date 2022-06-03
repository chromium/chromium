// Automatically add doNextStepButton to document for manual tests.
if (!window.testRunner) {
    setTimeout(function () {
        if (window.doNextStepButtonDisabled)
            return;
        doNextStepButton = document.body.insertBefore(document.createElement("button"), document.body.firstChild);
        doNextStepButton.onclick = doNextStep;
        doNextStepButton.innerText = "doNextStep button for manual testing. Use keyboard to select button and press (TAB, then SPACE).";
    }, 0);
}

function runOnKeyPress(fn)
{
    function keypressHandler() {
        document.removeEventListener('keypress', keypressHandler, false);
        fn();
    }
    document.addEventListener('keypress', keypressHandler, false);

    if (window.testRunner)
        eventSender.keyDown(" ", []);
}

function doNextStep(args)
{
    args = args || {};
    if (!window.testRunner && args.withUserGesture)
      return; // Wait for human to press doNextStep button.

    if (typeof(currentStep) == "undefined")
        currentStep = 0;

    setTimeout(function () {
        var thisStep = currentStep++;
        if (thisStep < todo.length)
            if (args.withUserGesture)
                runOnKeyPress(todo[thisStep]);
            else
                todo[thisStep]();
        else if (thisStep == todo.length)
            setTimeout(function () { finishJSTest(); }, 0); // Deferred so that excessive doNextStep calls will be observed.
        else
            testFailed("doNextStep called too many times.");
    }, 0);
}

function doNextStepWithUserGesture()
{
    doNextStep({withUserGesture: true});
}

function clickOnElement(element) {
    var viewportOffset = element.getBoundingClientRect();
    var top = viewportOffset.top;
    var bottom = viewportOffset.bottom;
    var left = viewportOffset.left;
    var right = viewportOffset.right;
    var vertical_center = top + (bottom - top) / 2;
    var horizontal_center = left + (right - left) / 2;

    if (window.testRunner) {
        eventSender.mouseMoveTo(horizontal_center, vertical_center);
        eventSender.mouseDown(0);
        eventSender.mouseUp(0);
    }

}

function eventExpected(eventHandlerName, message, expectedCalls, targetHanderNode)
{
    targetHanderNode[eventHandlerName] = function () {
        switch (expectedCalls--) {
        case 0:
            testFailed(eventHandlerName + " received after: " + message);
            finishJSTest();
            break;
        case 1:
            doNextStep();
        default:
            testPassed(eventHandlerName + " received after: " + message);
        };
    };
};

function expectOnlyChangeEvent(message, targetDocument) {
    debug("     " + message);
    targetDocument = targetDocument !== undefined ? targetDocument : document;
    eventExpected("onpointerlockchange", message, 1, targetDocument);
    eventExpected("onpointerlockerror", message, 0, targetDocument);
};

function expectOnlyErrorEvent(message, targetDocument) {
    debug("     " + message);
    targetDocument = targetDocument !== undefined ? targetDocument : document;
    eventExpected("onpointerlockchange", message, 0, targetDocument);
    eventExpected("onpointerlockerror", message, 1, targetDocument);
};

function expectNoEvents(message, targetDocument) {
    debug("     " + message);
    targetDocument = targetDocument !== undefined ? targetDocument : document;
    eventExpected("onpointerlockchange", message, 0, targetDocument);
    eventExpected("onpointerlockerror", message, 0, targetDocument);
};
