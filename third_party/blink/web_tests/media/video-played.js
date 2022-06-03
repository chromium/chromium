// TODO(srirama.m): Remove these globals with the help of expected arrays.
var timeRangeCount = 0;
var currentTimeRange = 0;
var playDuration = 0;
var startTimeOfPlay = 0;
var startTime = 0;

// Tracking down the exact cause of failure in https://crbug.com/931533
var testRangesCounter = 0;

function testRanges(expectedStartTimes, expectedEndTimes) {
    testRangesCounter++;

    assert_equals(video.played.length, timeRangeCount, "testRanges(" + testRangesCounter + ") played.length --");

    for (var i = 0; i < timeRangeCount; i++) {
        assert_equals(video.played.start(i).toFixed(2), expectedStartTimes[i], "testRanges(" + testRangesCounter + ") start["+i+"] --");
        assert_equals(video.played.end(i).toFixed(2), expectedEndTimes[i], "testRanges(" + testRangesCounter + ") end["+i+"] --");
    }
}

function waitForPauseAndContinue(t, nextFunc, extendsRange, expectedStartTimes, expectedEndTimes) {
    video.onpause = t.step_func(function() {
        var currentTime = video.currentTime.toFixed(2);
        if (extendsRange) {
            if(expectedEndTimes[currentTimeRange] < currentTime
                || expectedEndTimes[currentTimeRange] == undefined) {
                expectedEndTimes[currentTimeRange] = currentTime;
            }
        } else {
            expectedEndTimes.splice(currentTimeRange, 0, currentTime);
        }
        testRanges(expectedStartTimes, expectedEndTimes);
        if (nextFunc)
            nextFunc();
        else
            t.done();
    });
}

function willCreateNewRange(expectedStartTimes) {
    expectedStartTimes.splice(currentTimeRange, 0, video.currentTime.toFixed(2))
    ++timeRangeCount;
}

function callPauseIfTimeIsReached() {
    var playedTime = video.currentTime - startTimeOfPlay;
    if (playedTime <= 0) {
        // Deal with "loop" attribute. This allows only one loop, hence the first warning
        // at the begining of platForDuration().
        playedTime = video.duration - startTimeOfPlay + video.currentTime;
    }

    var elapsed = (performance.now() / 1000) - startTime;
    assert_less_than_equal(elapsed, 3.0);
    if (playedTime >= playDuration || video.currentTime == video.duration)
        video.pause();
    else {
        var delta = (playDuration - playedTime)  * 1000;
        setTimeout(this.step_func(callPauseIfTimeIsReached), delta);
    }
}

function playForDuration(duration, t) {
    playDuration = duration;
    assert_less_than_equal(duration, video.duration);

    // A 2 second timeout was sometimes insufficient to play 1.25 seconds of movie,
    // though more than 1 second of movie typically had played prior to those failures.
    // Use a larger value than 2 here.
    var timeoutThreshold = 3.0;
    assert_greater_than_equal(video.duration, timeoutThreshold);
    assert_less_than_equal(duration, timeoutThreshold - 1.5);
    video.play();
    startTime = performance.now() / 1000;
    startTimeOfPlay = video.currentTime;

    // Add a small amount to the timer because it will take a non-zero
    // amount of time for the video to start playing.
    setTimeout(t.step_func(callPauseIfTimeIsReached), (duration * 1000) + 100);
}

function startPlayingInNewRange(t, expectedStartTimes) {
    willCreateNewRange(expectedStartTimes);
    playForDuration(0.3, t);
}
