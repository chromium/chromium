    var video = null;
    var consoleDiv = null;
    var currentTest = null;
    var fragmentEndTime;
    var testData =
    {
        // http://www.w3.org/2008/WebVideo/Fragments/TC/ua-test-cases

        TC0001 : { start: null, end: null, valid: false, description: "#t=,", fragment: "t=,", comment: "Syntax error, not allowed according to the ABNF. The media fragment is ignored."},
        TC0002 : { start: null, end: null, valid: false, description: "#t=a,a and a >= 0", fragment: "t=3,3", comment: "Invalid semantics: start must be smaller than end. The media fragment is ignored."},
        TC0003 : { start: null, end: null, valid: false, description: "#t=a,b and a > b", fragment: "t=7,3", comment: "Invalid semantics: the requested interval's start is beyond its end. The media fragment is ignored."},
        TC0004 : { start: null, end: null, valid: true, description: "#t=a,b and a = 0, b = e", fragment: "t=0,9.97", comment: "The media is requested from 0 to e."},
        TC0005 : { start: 3, end: 7, valid: true, description: "#t=a,b and a >= 0, a < b, a < e and b <= e", fragment: "t=3,7", comment: "The media is requested from a to b."},
        TC0006 : { start: 3, end: null, valid: true, description: "#t=a,b and a >= 0, a < b, a < e and b > e", fragment: "t=3,15", comment: "The media is requested from a to e."},
        TC0009 : { start: "duration", end: null, valid: false, description: "#t=a,b and a < b and a >= e", fragment: "t=15,20", comment: "The request lies beyond the end of the resource. If the UA knows the duration of the resource, it seeks to the end of the media resource. Otherwise, the UA will send an (out-of-range) HTTP request with an 'include-setup' in order to setup its decoding pipeline."},
        TC0011 : { start: 3, end: null, valid: true, description: "#t=a with a >= 0, a < e", fragment: "t=3", comment: "Equivalent to #t=a,e. The media is requested from a to e."},
        TC0012 : { start: null, end: null, valid: false, description: "#t=a, with a >= 0, a < e", fragment: "t=3,", comment: "Invalid syntax, hence the temporal fragment is ignored."},
        TC0014 : { start: "duration", end: null, valid: false, description: "#t=a with a >= e", fragment: "t=15", comment: "The request lies beyond the end of the resource. If the UA knows the duration of the resource, it seeks to the end of the media resource. Otherwise, the UA will send an (out-of-range) HTTP request with an 'include-setup' in order to setup its decoding pipeline."},
        TC0015 : { start: null, end: 7, valid: true, description: "#t=,b and b > 0, b <= e", fragment: "t=,7", comment: "Equivalent to #t=0,b. The media is requested from 0 to b."},
        TC0017 : { start: null, end: "duration", valid: true, description: "#t=,b and b > e", fragment: "t=,15", comment: "Equivalent to #t=0,e. The media is requested from 0 to e."},
        TC0024 : { start: 3, end: 7, valid: true, description: "NPT", fragment: "t=npt:3,7", comment: "equivalent to #t=3,7"},
        TC0027 : { start: null, end: null, valid: false, description: "Illegal strings", fragment: "t=banana", comment: "UA knows that this is an invalid media fragment, so it will play the entire media resource."},
        TC0028 : { start: null, end: null, valid: false, description: "Illegal strings", fragment: "t=3,banana", comment: "UA knows that this is an invalid media fragment, so it will play the entire media resource."},
        TC0029 : { start: null, end: null, valid: false, description: "Illegal strings", fragment: "t=banana,7", comment: "UA knows that this is an invalid media fragment, so it will play the entire media resource."},
        TC0030 : { start: null, end: null, valid: false, description: "Illegal strings", fragment: "t='3'", comment: "UA knows that this is an invalid media fragment, so it will play the entire media resource."},
        TC0031 : { start: null, end: null, valid: false, description: "Illegal strings", fragment: "t=3-7", comment: "UA knows that this is an invalid media fragment, so it will play the entire media resource."},
        TC0032 : { start: null, end: null, valid: false, description: "Illegal strings", fragment: "t=3:7", comment: "UA knows that this is an invalid media fragment, so it will play the entire media resource."},
        TC0033 : { start: null, end: null, valid: false, description: "Illegal strings", fragment: "t=3,7,9", comment: "UA knows that this is an invalid media fragment, so it will play the entire media resource."},
        TC0034 : { start: null, end: null, valid: false, description: "Illegal strings", fragment: "t%3D3", comment: "UA does not identify this as a media fragment, so it will play the entire media resource. Note: %3D is equivalent to =."},
        TC0035 : { start: 3, end: null, valid: true, description: "Valid percent encoding", fragment: "%74=3", comment: "The media is requested from 3 seconds to the end. Note: %74 is equivalent to t."},
        TC0036 : { start: 3, end: null, valid: true, description: "Valid percent encoding", fragment: "t=%33", comment: "The media is requested from 3 seconds to the end. Note: %33 is equivalent to 3."},
        TC0037 : { start: 3, end: 7, valid: true, description: "Valid percent encoding", fragment: "t=3%2C7", comment: "The media is requested from 3 to 7 seconds. Note: %2C is equivalent to ,."},
        TC0038 : { start: 3, end: null, valid: true, description: "Valid percent encoding", fragment: "t=%6Ept:3", comment: "The media is requested from 3 seconds to the end. %6E is equivalent to n."},
        TC0039 : { start: 3, end: null, valid: true, description: "Valid percent encoding", fragment: "t=npt%3A3", comment: "The media is requested from 3 seconds to the end. Note: %3A is equivalent to :."},
        TC0044 : { start: null, end: null, valid: false, description: "#t=a,b and a < 0", fragment: "t=-1,3", comment: "Invalid syntax: a '-' character is not allowed at this position according to the ABNF. The UA knows that this is an invalid media fragment, so it will play the entire media resource."},
        TC0051 : { start: 3, end: null, valid: true, description: "Trailing '&'", fragment: "t=3&", comment: "After processing name-value pairs, this appears to be equivalent to #t=3."},
        TC0052 : { start: 3, end: null, valid: true, description: "Unknown keys", fragment: "u=12&t=3", comment: "After processing name-value pairs, this appears to be equivalent to #t=3."},
        TC0053 : { start: 3, end: null, valid: true, description: "Unknown unit", fragment: "t=foo:7&t=npt:3", comment: "After processing name-value pairs, this appears to be equivalent to #t=3."},
        TC0054 : { start: 3, end: null, valid: true, description: "Unknown keys (bis)", fragment: "&&=&=tom&jerry=&t=3&t=meow:0#", comment: "After processing name-value pairs, this appears to be equivalent to #t=3."},
        TC0055 : { start: 3, end: null, valid: true, description: "Duplicate (key - known unit) combination", fragment: "t=7&t=3", comment: "When a fragment dimensions occurs multiple times, only the last occurrence of that dimension is interpreted."},
        TC0058 : { start: null, end: null, valid: false, description: "Invalid axis parameters", fragment: "T=3,7", comment: "UA does not identify this as a media fragment. The entire media resource is played."},
        TC0059 : { start: 3, end: null, valid: true, description: "Duplicate (key - known unit) combination", fragment: "t=smpte:00:00:01&t=npt:3", comment: "When a fragment dimensions occurs multiple times, only the last occurrence of that dimension is interpreted."},
        TC0061 : { start: null, end: null, valid: false, description: "Illegal strings", fragment: "t=", comment: "UA knows that this is an invalid media fragment, so it will play the entire media resource."},
        TC0062 : { start: null, end: null, valid: false, description: "Illegal strings", fragment: "t=.", comment: "UA knows that this is an invalid media fragment, so it will play the entire media resource."},
        TC0063 : { start: null, end: null, valid: false, description: "Illegal strings", fragment: "t=.0", comment: "UA knows that this is an invalid media fragment, so it will play the entire media resource."},
        TC0064 : { start: null, end: null, valid: false, description: "Illegal strings", fragment: "t=0s", comment: "UA knows that this is an invalid media fragment, so it will play the entire media resource."},
        TC0065 : { start: null, end: null, valid: false, description: "Illegal strings", fragment: "t=,0s", comment: "UA knows that this is an invalid media fragment, so it will play the entire media resource."},
        TC0066 : { start: null, end: null, valid: false, description: "Illegal strings", fragment: "t=0s,0s", comment: "UA knows that this is an invalid media fragment, so it will play the entire media resource."},
        TC0067 : { start: null, end: null, valid: false, description: "Illegal strings", fragment: "t=00:00:00s", comment: "UA knows that this is an invalid media fragment, so it will play the entire media resource."},
        TC0068 : { start: null, end: null, valid: false, description: "Illegal strings", fragment: "t=s", comment: "UA knows that this is an invalid media fragment, so it will play the entire media resource."},
        TC0069 : { start: null, end: null, valid: false, description: "Illegal strings", fragment: "t=npt:", comment: "UA knows that this is an invalid media fragment, so it will play the entire media resource."},
        TC0070 : { start: null, end: null, valid: false, description: "Illegal strings", fragment: "t=1e-1", comment: "UA knows that this is an invalid media fragment, so it will play the entire media resource."},
        TC0071 : { start: null, end: null, valid: false, description: "Illegal strings", fragment: "t=00:00:01.1e-1", comment: "UA knows that this is an invalid media fragment, so it will play the entire media resource."},
        TC0072 : { start: 3, end: null, valid: true, description: "Trailing dot", fragment: "t=3.", comment: "Equivalent to #t=a,e. The media is requested from a to e."},
        TC0073 : { start: null, end: null, valid: false, description: "Illegal strings", fragment: "t=0:0:0", comment: "UA knows that this is an invalid media fragment, so it will play the entire media resource."},
        TC0074 : { start: null, end: null, valid: false, description: "Illegal strings", fragment: "t=0:00:60", comment: "UA knows that this is an invalid media fragment, so it will play the entire media resource."},
        TC0075 : { start: null, end: null, valid: false, description: "Illegal strings", fragment: "t=0:01:60", comment: "UA knows that this is an invalid media fragment, so it will play the entire media resource."},
        TC0076 : { start: null, end: null, valid: false, description: "Illegal strings", fragment: "t=0:60:00", comment: "UA knows that this is an invalid media fragment, so it will play the entire media resource."},
        TC0077 : { start: null, end: null, valid: false, description: "Illegal strings", fragment: "t=0:000:000", comment: "UA knows that this is an invalid media fragment, so it will play the entire media resource."},
        TC0078 : { start: 3, end: 7, valid: true, description: "NPT HH:MM:SS format", fragment: "t=00:00:03,00:00:07", comment: "The media is requested from a to b."},
        TC0079 : { start: 3, end: 7, valid: true, description: "NPT mixed formats", fragment: "t=3,00:00:07", comment: "The media is requested from a to b."},
        TC0080 : { start: null, end: null, valid: true, description: "NPT, trailing dot", fragment: "t=00:00.", comment: "A valid media fragment: { starting at 0 seconds. Thus, the UA will play the entire media resource."},
        TC0081 : { start: null, end: null, valid: true, description: "NPT, trailing dot (bis)", fragment: "t=0:00:00.", comment: "A valid media fragment: { starting at 0 seconds. Thus, the UA will play the entire media resource."},
        TC0082 : { start: null, end: null, valid: false, description: "Illegal strings", fragment: "t=0:00:10e-1", comment: "UA knows that this is an invalid media fragment, so it will play the entire media resource."},
        TC0083 : { start: null, end: null, valid: false, description: "Illegal strings", fragment: "t=0:00:60.000", comment: "UA knows that this is an invalid media fragment, so it will play the entire media resource."},
        TC0084 : { start: null, end: null, valid: false, description: "Illegal strings", fragment: "t=0:60:00.000", comment: "UA knows that this is an invalid media fragment, so it will play the entire media resource."},
        TC0085 : { start: 3, end: 7, valid: true, description: "Trailing invalid time fragment is ignored", fragment: "t=3,7&t=foo", comment: "The media is requested from a to b."},
        TC0086 : { start: 3, end: 7, valid: true, description: "Rubbish before &", fragment: "foo&t=3,7", comment: "Rubbish before & is ignored."},
        TC0087 : { start: 3, end: 7, valid: true, description: "Rubbish after &", fragment: "t=3,7&foo", comment: "Rubbish after & is ignored."},
        TC0088 : { start: 3, end: 7, valid: true, description: "Sprinkling &", fragment: "t=3,7&&", comment: "Sprinkling & is OK."},
        TC0089 : { start: 3, end: 7, valid: true, description: "Sprinkling &", fragment: "&t=3,7", comment: "Sprinkling & is OK."},
        TC0090 : { start: 3, end: 7, valid: true, description: "Sprinkling &", fragment: "&&t=3,7", comment: "Sprinkling & is OK."},
        TC0091 : { start: 3, end: 7, valid: true, description: "Sprinkling &", fragment: "&t=3,7&", comment: "Sprinkling & is OK."},
        TC0092 : { start: null, end: null, valid: false, description: "Incorrect percent encoding", fragment: "t%3d10", comment: "UA knows that this is an invalid media fragment, so it will play the entire media resource."},
        TC0093 : { start: null, end: null, valid: false, description: "Incorrect percent encoding", fragment: "t=10%26", comment: "UA knows that this is an invalid media fragment, so it will play the entire media resource."},
        TC0094 : { start: null, end: null, valid: false, description: "Trailing comma", fragment: "t=3,7,", comment: "UA knows that this is an invalid media fragment, so it will play the entire media resource."},
        TC0095 : { start: 3, end: 7, valid: true, description: "NPT HH:MM:SS format. Single digit npt-hh.", fragment: "t=0:00:03,0:00:07", comment: "The media is requested from a to b."}
    };

    logConsole();
    if (window.testRunner) {
        testRunner.dumpAsText();
        testRunner.waitUntilDone();
    }

    function logConsole()
    {
        if (!consoleDiv && document.body) {
            consoleDiv = document.createElement('div');
            document.body.appendChild(consoleDiv);
        }
        return consoleDiv;
    }

    function testExpected(testFuncString, expected, comparison)
    {
        try {
            var observed = eval(testFuncString);
        } catch (ex) {
            consoleWrite(ex);
            return;
        }

        if (comparison === undefined)
            comparison = '==';

        var success = false;
        switch (comparison)
        {
            case '<':   success = observed <  expected; break;
            case '<=': success = observed <= expected; break;
            case '>':   success = observed >  expected; break;
            case '>=': success = observed >= expected; break;
            case '!=':  success = observed != expected; break;
            case '==': success = observed == expected; break;
            case '===': success = observed === expected; break;
        }

        reportExpected(success, testFuncString, comparison, expected, observed)
    }

    var testNumber = 0;

    function reportExpected(success, testFuncString, comparison, expected, observed)
    {
        testNumber++;

        var msg = "Test " + testNumber;

        msg = "EXPECTED (<em>" + testFuncString + " </em>" + comparison + " '<em>" + expected + "</em>')";

        if (!success)
            msg +=  ", OBSERVED '<em>" + observed + "</em>'";

        logResult(success, msg);
    }

    function run(testFuncString)
    {
        consoleWrite("RUN(" + testFuncString + ")");
        try {
            eval(testFuncString);
        } catch (ex) {
            consoleWrite(ex);
        }
    }

    function waitForEventOnce(eventName, func, endit, doNotLog)
    {
        waitForEvent(eventName, func, endit, true, null, doNotLog)
    }

    function waitForEvent(eventName, func, endit, oneTimeOnly, doNotLog)
    {
        function _eventCallback(event)
        {
            if (oneTimeOnly)
                video.removeEventListener(eventName, _eventCallback, true);

            if (!doNotLog)
              consoleWrite("EVENT(" + eventName + ")");

            if (func)
                func(event);

            if (endit)
                endTest();
        }

        video.addEventListener(eventName, _eventCallback, true);
    }

    var testEnded = false;

    function endTest()
    {
        consoleWrite("END OF TEST");
        testEnded = true;
        if (window.testRunner)
            testRunner.notifyDone();
    }

    function logResult(success, text)
    {
        if (success)
            consoleWrite(text + " <span style='color:green'>OK</span>");
        else
            consoleWrite(text + " <span style='color:red'>FAIL</span>");
    }

    function consoleWrite(text)
    {
        if (testEnded)
            return;
        var span = document.createElement("span");
        logConsole().appendChild(span);
        span.innerHTML = text + '<br>';
    }

    function pause()
    {
        const maximumStopDelta = 0.5;
        var delta = Math.abs(video.currentTime - fragmentEndTime).toFixed(2);
        reportExpected((delta <= maximumStopDelta), ("video.currentTime - fragmentEndTime"), "<=", maximumStopDelta, delta);

        endTest();
    }

    function canplaythrough()
    {
        var info = testData[currentTest];
        var duration = video.duration.toFixed(2);
        var start = info.start ? info.start : 0;
        fragmentEndTime = info.end ? info.end : duration;

        if (start == "duration")
            start = duration;
        if (fragmentEndTime == "duration")
            fragmentEndTime = duration;

        // Don't use "testExpected()" so we won't log the actual duration as the floating point result may differ with different engines.
        var startString = info.start == "duration" ? "duration" : start;
        reportExpected(video.currentTime.toFixed(2) == start, "video.currentTime", "==", startString, video.currentTime);

        if (info.valid) {
            video.currentTime = (fragmentEndTime - 0.5);
            run("video.play()");
        } else
            endTest();
    }

    function start()
    {
        video = document.createElement('video');
        video.setAttribute('id', 'vid');
        video.setAttribute('width', '320');
        video.setAttribute('height', '240');
        video.setAttribute('controls', '');
        var paragraph = document.createElement('p');
        paragraph.appendChild(video);
        document.body.appendChild(paragraph);

        waitForEventOnce("canplaythrough", canplaythrough);
        waitForEvent("pause", pause);

        var fileName = location.href.split('/').pop();
        currentTest = fileName.substring(0, fileName.lastIndexOf("."));

        var info = testData[currentTest];
        consoleWrite("<br>Title: <b>" + currentTest + "</b>");
        consoleWrite("Fragment: '<i>" + info.fragment + "</i>'");
        consoleWrite("Comment: <i>" + info.comment + "</i>");
        url = "../content/counting.ogv" + "#" + info.fragment;
        video.src = url;
    }