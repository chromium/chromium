(function(window) {
    // Set the testharness.js timeout to 120 seconds so that it is higher than
    // the LayoutTest timeout. This prevents testharness.js from prematurely
    // terminating tests and allows the LayoutTest runner to control when to
    // timeout the test.
    setup({ explicit_timeout: 120000 });

    var SEGMENT_INFO_LIST = [
        {
            url: '/media/resources/media-source/webm/test.webm',
            type: 'video/webm; codecs="vp8, vorbis"',
            durationInInitSegment: 6.042,
            duration: 6.051,
            // Supports jagged-ended stream end timestamps with some less than duration:
            bufferedRangeEndBeforeEndOfStream: 6.042,
            init: { offset: 0, size: 4357 },
            media: [
                {  offset: 4357, size: 11830, timecode: 0, highest_end_time: 0.398 },
                {  offset: 16187, size: 12588, timecode: 0.385, highest_end_time: 0.797 },
                {  offset: 28775, size: 14588, timecode: 0.779, highest_end_time: 1.195 },
                {  offset: 43363, size: 13023, timecode: 1.174, highest_end_time: 1.593 },
                {  offset: 56386, size: 13127, timecode: 1.592, highest_end_time: 1.992 },
                {  offset: 69513, size: 14456, timecode: 1.987, highest_end_time: 2.39 },
                {  offset: 83969, size: 13458, timecode: 2.381, highest_end_time: 2.789 },
                {  offset: 97427, size: 14566, timecode: 2.776, highest_end_time: 3.187 },
                {  offset: 111993, size: 13201, timecode: 3.171, highest_end_time: 3.585 },
                {  offset: 125194, size: 14061, timecode: 3.566, highest_end_time: 3.984 },
                {  offset: 139255, size: 15353, timecode: 3.96, highest_end_time: 4.382 },
                {  offset: 154608, size: 13618, timecode: 4.378, highest_end_time: 4.781 },
                {  offset: 168226, size: 15094, timecode: 4.773, highest_end_time: 5.179 },
                {  offset: 183320, size: 13069, timecode: 5.168, highest_end_time: 5.577 },
                {  offset: 196389, size: 13788, timecode: 5.563, highest_end_time: 5.976 },
                {  offset: 210177, size: 9009, timecode: 5.957, highest_end_time: 6.051 },
            ],
        },
        {
            url: '/media/resources/media-source/mp4/test.mp4',
            type: 'video/mp4; codecs="mp4a.40.2, avc1.4D401E"',
            // FIXME: Get the init segment duration fixed to match duration after append.
            //        See http://crbug.com/354284.
            durationInInitSegment: 6.0368,
            duration: 6.0424,
            bufferedRangeEndBeforeEndOfStream: 6.0368,
            init: { offset: 0, size: 1178 },
            media: [
                // FIXME: Fix these timecodes to be PTS, not DTS, and highest_end_times to correspond
                // to highest PTS+duration, not highest DTS+duration. See http://crbug.com/373039.
                // FIXME: Some segments are parsed to start with keyframe but DTS < PTS. See
                // http://crbug.com/371947 and http://crbug.com/367786.
                {  offset: 1246, size: 23828, timecode: 0, highest_end_time: 0.835917 },
                {  offset: 25142, size: 25394, timecode: 0.797, highest_end_time: 1.625395 },
                {  offset: 50604, size: 24761, timecode: 1.594, highest_end_time: 2.414874 },
                {  offset: 75433, size: 25138, timecode: 2.390, highest_end_time: 3.227572 },
                {  offset: 100639, size: 22935, timecode: 3.187, highest_end_time: 4.017051 },
                {  offset: 123642, size: 24995, timecode: 3.984, highest_end_time: 4.806529 },
                {  offset: 148637, size: 24968, timecode: 4.781, highest_end_time: 5.619228 },
                {  offset: 173689, size: 19068, timecode: 5.578, highest_end_time: 6.0424 },
                {  offset: 192757, size: 200, timecode: 5.619, highest_end_time: 6.0424 },
            ],
        }
    ];
    EventExpectationsManager = function(test)
    {
        this.test_ = test;
        this.eventTargetList_ = [];
        this.waitCallbacks_ = [];
    };

    EventExpectationsManager.prototype.expectEvent = function(object, eventName, description)
    {
        var eventInfo = { 'target': object, 'type': eventName, 'description': description};
        var expectations = this.getExpectations_(object);
        expectations.push(eventInfo);

        var t = this;
        var waitHandler = this.test_.step_func(this.handleWaitCallback_.bind(this));
        var eventHandler = this.test_.step_func(function(event)
        {
            object.removeEventListener(eventName, eventHandler);
            var expected = expectations[0];
            assert_equals(event.target, expected.target, "Event target match.");
            assert_equals(event.type, expected.type, "Event types match.");
            assert_equals(eventInfo.description, expected.description, "Descriptions match for '" +  event.type + "'.");

            expectations.shift(1);
            if (t.waitCallbacks_.length > 0)
                setTimeout(waitHandler, 0);
        });
        object.addEventListener(eventName, eventHandler);
    };

    EventExpectationsManager.prototype.waitForExpectedEvents = function(callback)
    {
        this.waitCallbacks_.push(callback);
        setTimeout(this.test_.step_func(this.handleWaitCallback_.bind(this)), 0);
    };

    EventExpectationsManager.prototype.expectingEvents = function()
    {
        for (var i = 0; i < this.eventTargetList_.length; ++i) {
            if (this.eventTargetList_[i].expectations.length > 0) {
                return true;
            }
        }
        return false;
    }

    EventExpectationsManager.prototype.handleWaitCallback_ = function()
    {
        if (this.waitCallbacks_.length == 0 || this.expectingEvents())
            return;
        var callback = this.waitCallbacks_.shift(1);
        callback();
    };

    EventExpectationsManager.prototype.getExpectations_ = function(target)
    {
        for (var i = 0; i < this.eventTargetList_.length; ++i) {
            var info = this.eventTargetList_[i];
            if (info.target == target) {
                return info.expectations;
            }
        }
        var expectations = [];
        this.eventTargetList_.push({ 'target': target, 'expectations': expectations });
        return expectations;
    };

    function loadData_(test, url, callback, responseType)
    {
        var request = new XMLHttpRequest();
        request.open("GET", url, true);
        if (responseType !== undefined) {
            request.responseType = responseType;
        }
        request.onload = test.step_func(function(event)
        {
            if (request.status != 200) {
                assert_unreached("Unexpected status code : " + request.status);
                return;
            }
            var response = request.response;
            if (responseType !== undefined && responseType == 'arraybuffer') {
                response = new Uint8Array(response);
            }
            callback(response);
        });
        request.onerror = test.step_func(function(event)
        {
            assert_unreached("Unexpected error");
        });
        request.send();
    }

    function openMediaSource_(test, mediaTag, callback)
    {
        var mediaSource = new MediaSource();
        var mediaSourceURL = URL.createObjectURL(mediaSource);

        var eventHandler = test.step_func(onSourceOpen);
        function onSourceOpen(event)
        {
            mediaSource.removeEventListener('sourceopen', eventHandler);
            URL.revokeObjectURL(mediaSourceURL);
            callback(mediaSource);
        }

        mediaSource.addEventListener('sourceopen', eventHandler);
        mediaTag.src = mediaSourceURL;
    }

    var MediaSourceUtil = {};

    MediaSourceUtil.loadTextData = function(test, url, callback)
    {
        loadData_(test, url, callback);
    };

    MediaSourceUtil.loadBinaryData = function(test, url, callback)
    {
        loadData_(test, url, callback, 'arraybuffer');
    };

    MediaSourceUtil.fetchManifestAndData = function(test, manifestFilename, callback)
    {
        var baseURL = '/media/resources/media-source/';
        var manifestURL = baseURL + manifestFilename;
        MediaSourceUtil.loadTextData(test, manifestURL, function(manifestText)
        {
            var manifest = JSON.parse(manifestText);

            assert_true(MediaSource.isTypeSupported(manifest.type), manifest.type + " is supported.");

            var mediaURL = baseURL + manifest.url;
            MediaSourceUtil.loadBinaryData(test, mediaURL, function(mediaData)
            {
                callback(manifest.type, mediaData);
            });
        });
    };

    MediaSourceUtil.extractSegmentData = function(mediaData, info)
    {
        var start = info.offset;
        var end = start + info.size;
        return mediaData.subarray(start, end);
    }

    MediaSourceUtil.getMediaDataForPlaybackTime = function(mediaData, segmentInfo, playbackTimeToAdd)
    {
        assert_less_than_equal(playbackTimeToAdd, segmentInfo.duration);
        var mediaInfo = segmentInfo.media;
        var start = mediaInfo[0].offset;
        var numBytes = 0;
        var segmentIndex = 0;
        while (segmentIndex < mediaInfo.length && mediaInfo[segmentIndex].timecode <= playbackTimeToAdd)
        {
          numBytes += mediaInfo[segmentIndex].size;
          ++segmentIndex;
        }
        return mediaData.subarray(start, numBytes + start);
    }

    // Fills up the sourceBuffer by repeatedly calling doAppendDataFunc, which is expected to append some data
    // to the sourceBuffer, until a QuotaExceeded exception is thrown. Then it calls the onCaughtQuotaExceeded callback.
    MediaSourceUtil.fillUpSourceBufferHelper = function(test, sourceBuffer, doAppendDataFunc, onCaughtQuotaExceeded)
    {
        // We are appending data repeatedly in sequence mode, there should be no gaps.
        assert_equals(sourceBuffer.mode, 'sequence');
        assert_false(sourceBuffer.buffered.length > 1, 'unexpected gap in buffered ranges.');
        try {
            doAppendDataFunc();
        } catch(ex) {
            assert_equals(ex.name, 'QuotaExceededError');
            onCaughtQuotaExceeded();
        }
        test.expectEvent(sourceBuffer, 'updateend', 'append ended.');
        test.waitForExpectedEvents(function() { MediaSourceUtil.fillUpSourceBufferHelper(test, sourceBuffer, doAppendDataFunc, onCaughtQuotaExceeded); });
    };

    MediaSourceUtil.fillUpSourceBuffer = function(test, mediaSource, mediaDataManifest, onBufferFull)
    {
        MediaSourceUtil.fetchManifestAndData(test, mediaDataManifest, function(type, mediaData)
        {
            var sourceBuffer = mediaSource.addSourceBuffer(MediaSourceUtil.AUDIO_ONLY_TYPE);
            sourceBuffer.mode = 'sequence';

            var appendedDataSize = 0;
            MediaSourceUtil.fillUpSourceBufferHelper(test, sourceBuffer,
                function () { // doAppendDataFunc
                    appendedDataSize += mediaData.length;
                    sourceBuffer.appendBuffer(mediaData);
                },
                function () { // onCaughtQuotaExceeded
                    onBufferFull(appendedDataSize);
                });
        });
    };

    function getFirstSupportedType(typeList)
    {
        for (var i = 0; i < typeList.length; ++i) {
            if (MediaSource.isTypeSupported(typeList[i]))
                return typeList[i];
        }
        return "";
    }

    function getSegmentInfo()
    {
        for (var i = 0; i < SEGMENT_INFO_LIST.length; ++i) {
            var segmentInfo = SEGMENT_INFO_LIST[i];
            if (MediaSource.isTypeSupported(segmentInfo.type)) {
                return segmentInfo;
            }
        }
        return null;
    }

    var audioOnlyTypes = ['audio/webm;codecs="vorbis"', 'audio/mp4;codecs="mp4a.40.2"'];
    var videoOnlyTypes = ['video/webm;codecs="vp8"', 'video/mp4;codecs="avc1.4D4001"'];
    var audioVideoTypes = ['video/webm;codecs="vp8,vorbis"', 'video/mp4;codecs="mp4a.40.2"'];
    MediaSourceUtil.AUDIO_ONLY_TYPE = getFirstSupportedType(audioOnlyTypes);
    MediaSourceUtil.VIDEO_ONLY_TYPE = getFirstSupportedType(videoOnlyTypes);
    MediaSourceUtil.AUDIO_VIDEO_TYPE = getFirstSupportedType(audioVideoTypes);
    MediaSourceUtil.SEGMENT_INFO = getSegmentInfo();

    MediaSourceUtil.getSubType = function(mimetype) {
        var slashIndex = mimetype.indexOf("/");
        var semicolonIndex = mimetype.indexOf(";");
        if (slashIndex <= 0) {
            assert_unreached("Invalid mimetype '" + mimetype + "'");
            return;
        }

        var start = slashIndex + 1;
        if (semicolonIndex >= 0) {
            if (semicolonIndex <= start) {
                assert_unreached("Invalid mimetype '" + mimetype + "'");
                return;
            }

            return mimetype.substr(start, semicolonIndex - start)
        }

        return mimetype.substr(start);
    };

    // TODO: Add wrapper object to MediaSourceUtil that binds loaded mediaData to its
    // associated segmentInfo.

    function addExtraTestMethods(test)
    {
        test.failOnEvent = function(object, eventName)
        {
            object.addEventListener(eventName, test.step_func(function(event)
            {
                assert_unreached("Unexpected event '" + eventName + "'");
            }));
        };

        test.endOnEvent = function(object, eventName)
        {
            object.addEventListener(eventName, test.step_func(function(event) { test.done(); }));
        };

        test.eventExpectations_ = new EventExpectationsManager(test);
        test.expectEvent = function(object, eventName, description)
        {
            test.eventExpectations_.expectEvent(object, eventName, description);
        };

        test.waitForExpectedEvents = function(callback)
        {
            test.eventExpectations_.waitForExpectedEvents(callback);
        };

        test.waitForCurrentTimeChange = function(mediaElement, callback)
        {
            var initialTime = mediaElement.currentTime;

            var onTimeUpdate = test.step_func(function()
            {
                if (mediaElement.currentTime != initialTime) {
                    mediaElement.removeEventListener('timeupdate', onTimeUpdate);
                    callback();
                }
            });

            mediaElement.addEventListener('timeupdate', onTimeUpdate);
        }

        var oldTestDone = test.done.bind(test);
        test.done = function()
        {
            if (test.status == test.PASS) {
                assert_false(test.eventExpectations_.expectingEvents(), "No pending event expectations.");
            }
            oldTestDone();
        };
    };

    window['MediaSourceUtil'] = MediaSourceUtil;
    window['media_test'] = function(testFunction, description, properties)
    {
        properties = properties || {};
        return async_test(function(test)
        {
            addExtraTestMethods(test);
            testFunction(test);
        }, description, properties);
    };
    window['mediasource_test'] = function(testFunction, description, properties)
    {
        return media_test(function(test)
        {
            var mediaTag = document.createElement("video");
            document.body.appendChild(mediaTag);

            // Overload done() so that element added to the document can be removed.
            test.removeMediaElement_ = true;
            var oldTestDone = test.done.bind(test);
            test.done = function()
            {
                if (test.removeMediaElement_) {
                    document.body.removeChild(mediaTag);
                    test.removeMediaElement_ = false;
                }
                oldTestDone();
            };

            openMediaSource_(test, mediaTag, function(mediaSource)
            {
                testFunction(test, mediaTag, mediaSource);
            });
        }, description, properties);
    };

    // In addition to test harness's async_test() properties parameter, this
    // function recognizes a custom properties dict with custom entries:
    //   - allow_media_element_error: don't immediately fail on media player error
    //   - enable_controls: add the default chromium media controls (timeline, play, volume) to the player
    window['mediasource_testafterdataloaded'] = function(testFunction, description, properties)
    {
        mediasource_test(function(test, mediaElement, mediaSource)
        {
            var segmentInfo = MediaSourceUtil.SEGMENT_INFO;

            if (!segmentInfo) {
                assert_unreached("No segment info compatible with this MediaSource implementation.");
                return;
            }

            if (properties == null || properties.allow_media_element_error == null || !properties.allow_media_element_error)
                test.failOnEvent(mediaElement, 'error');

            if (properties !== undefined && properties.enable_controls !== undefined && properties.enable_controls)
                mediaElement.setAttribute('controls', true);

            var sourceBuffer = mediaSource.addSourceBuffer(segmentInfo.type);
            MediaSourceUtil.loadBinaryData(test, segmentInfo.url, function(mediaData)
            {
                testFunction(test, mediaElement, mediaSource, segmentInfo, sourceBuffer, mediaData);
            });
        }, description, properties);
    };

    function timeRangesToString(ranges)
    {
        var s = "{";
        for (var i = 0; i < ranges.length; ++i) {
            s += " [" + ranges.start(i).toFixed(3) + ", " + ranges.end(i).toFixed(3) + ")";
        }
        return s + " }";
    }

    window['assertBufferedEquals'] = function(obj, expected, description)
    {
        var actual = timeRangesToString(obj.buffered);
        assert_equals(actual, expected, description);
    };

    window['assertSeekableEquals'] = function(obj, expected, description)
    {
        var actual = timeRangesToString(obj.seekable);
        assert_equals(actual, expected, description);
    };

})(window);
