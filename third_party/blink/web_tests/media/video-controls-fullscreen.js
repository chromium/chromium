"use strict";

function fullscreen_test()
{
    async_test(function(t)
    {
        var v1 = document.createElement("video");
        var v2 = document.createElement("video");
        v1.controls = v2.controls = true;
        v1.src = "content/test.ogv";
        v2.src = "content/test.oga";
        document.body.appendChild(v1);
        document.body.appendChild(v2);

        // load event fires when both video elements are ready
        window.addEventListener("load", t.step_func(function()
        {
            assert_true(hasEnabledFullscreenButton(v1),
                        "fullscreen button enabled when there is a video track");
            assert_false(hasEnabledFullscreenButton(v2),
                         "fullscreen button not enabled when there is no video track");

            // click the fullscreen button
            var coords = mediaControlsButtonCoordinates(v1, "fullscreen-button");
            eventSender.mouseMoveTo(coords[0], coords[1]);
            eventSender.mouseDown();
            eventSender.mouseUp();
            // wait for the fullscreenchange event
        }));

        v1.addEventListener("fullscreenchange", t.step_func_done());

        v2.addEventListener("webkitfullscreenchange", t.unreached_func());
        v2.addEventListener("fullscreenchange", t.unreached_func());
    });
}

function fullscreen_iframe_test()
{
    async_test(function(t)
    {
        var iframe = document.querySelector("iframe");
        var doc = iframe.contentDocument;
        var v = doc.createElement("video");
        v.controls = true;
        v.src = "content/test.ogv";
        doc.body.appendChild(v);

        v.addEventListener("loadeddata", t.step_func_done(function()
        {
            assert_equals(hasEnabledFullscreenButton(v), iframe.allowFullscreen,
                          "fullscreen button enabled if and only if fullscreen is allowed");
        }));
    });
}

function fullscreen_not_supported_test()
{
    async_test(function(t)
    {
        var v = document.createElement("video");
        v.controls = true;
        v.src = "content/test.ogv";
        document.body.appendChild(v);

        v.addEventListener("loadeddata", t.step_func_done(function()
        {
            assert_false(hasEnabledFullscreenButton(v),
                         "fullscreen button not enabled when fullscreen is not supported");
        }));
    });
}
