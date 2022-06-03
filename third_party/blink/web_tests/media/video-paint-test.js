function waitForMultipleEvents(name, times, func) {
    var count = 0;
    document.addEventListener(name, function() {
        if (++count == times) {
            func();
        }
    }, true);
}

function waitForAllCallbacks(callCount, func) {
    var rvfc_calls = 0;
    return function() {
        if(++rvfc_calls == callCount) {
            func();
        }
    }
}

function setVideoSrcAndWaitForFirstFrame(src) {
    var videos = document.getElementsByTagName('video');

    var endTestCallback = waitForAllCallbacks(videos.length, () => {
        if (window.testRunner)
            testRunner.notifyDone();
    });

    for (var i = 0; i < videos.length; ++i) {
        videos[i].requestVideoFrameCallback(endTestCallback);
        videos[i].src = src;
    }
}

function init()
{
    var videos = document.getElementsByTagName('video');

    waitForMultipleEvents("canplaythrough", videos.length, function() {
        for (var i = 0; i < videos.length; ++i) {
            videos[i].play();
            videos[i].addEventListener("playing", function(event) {
                event.target.pause();
                event.target.currentTime = 0;
            });
        }

        waitForMultipleEvents("seeked", videos.length, function() {
            if (window.testRunner)
                testRunner.notifyDone();
        });
    });
}

if (window.testRunner) {
    testRunner.waitUntilDone();
    setTimeout(function() {
        document.body.appendChild(document.createTextNode('FAIL'));
        if (window.testRunner)
            testRunner.notifyDone();
    } , 8000);
}

function initAndPause()
{
    var videos = document.getElementsByTagName('video');

    waitForMultipleEvents("canplaythrough", videos.length, function() {
        for (var i = 0; i < videos.length; ++i) {
            videos[i].play();
            videos[i].addEventListener("playing", function(event) {
                event.target.pause();
            });
        }

        waitForMultipleEvents("pause", videos.length, function() {
            if (window.testRunner)
                testRunner.notifyDone();
        });
    });

}

function initAndSeeked()
{
    var videos = document.getElementsByTagName('video');

    waitForMultipleEvents("seeked", videos.length, function() {
        if (window.testRunner)
            testRunner.notifyDone();
    });
}
