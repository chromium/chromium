// Worker tests need to set this for themselves in the .html file.
if (typeof window !== 'undefined')
  window.jsTestIsAsync = true;

var timeoutID = null;

function endTest()
{
    clearTimeout(timeoutID);
    finishJSTest();
}

// There must be no TCP/IP server listening on port |port|.
// If |closeOnError| is true, then the WebSocket close method will be called
// from the onerror handler.
// if |closeOnClose| is true, then the WebSocket close method will be called
// from the onclose handler.
function doTest(port, closeOnError, closeOnClose)
{
    debug("Test start (Port " + port + ")");

    var url = "ws://127.0.0.1:" + port + "/"

    var ws = new WebSocket(url);

    ws.onopen = function()
    {
        testFailed("Connected");
        endTest();
    };

    ws.onmessage = function(messageEvent)
    {
        testFailed("Received Message");
        ws.close();
        endTest();
    };

    ws.onclose = function()
    {
        testPassed("onclose was called");
        if (closeOnClose) {
            ws.close();
        }
        endTest();
    }

    ws.onerror = function()
    {
        testPassed("onerror was called");
        if (closeOnError) {
            ws.close();
        }
    };

    // Each failure to connect to 127.0.0.1 takes 3 seconds on Windows.
    // Allow 4 seconds for padding.
    timeoutID = setTimeout(timeOutCallback, 4000);
}

function timeOutCallback()
{
    debug("Timed out...");
    endTest();
}
