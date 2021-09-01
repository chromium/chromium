function createIFrame(origin, type) {
    var file;
    if (type == "document") {
        file = "post-addressspace-to-parent.php";
    } else if (type == "document+csp") {
        file = "post-addressspace-to-parent.php?csp";
    } else if (type == "document+appcache") {
        file = "post-addressspace-to-parent-with-appcache.php";
    } else if (type == "document+appcache+csp") {
        file = "post-addressspace-to-parent-with-appcache.php?csp";
    } else if (type == "worker") {
        file = "post-addressspace-from-worker.html";
    } else if (type == "module-worker") {
        file = "post-addressspace-from-worker.html?module";
    } else if (type == "sharedworker") {
        file = "post-addressspace-from-sharedworker.html";
    } else if (type == "module-sharedworker") {
        file = "post-addressspace-from-sharedworker.html?module";
    } else {
        throw new Error("Unknown type: " + type);
    }

    var i = document.createElement('iframe');
    i.src = origin + "/security/cors-rfc1918/resources/" + file;
    return i;
}

function addressSpaceTest(origin, type, expected, callback, nameExtra) {
    async_test(function (t) {
        var i = createIFrame(origin, type);
        window.addEventListener("message", t.step_func(function (e) {
            if (e.source == i.contentWindow) {
                assert_equals(e.data.origin, origin, 'origin');
                assert_equals(e.data.addressSpace, expected, 'addressSpace');
                if (callback)
                    callback();
                t.done();
            }
        }));

        document.body.appendChild(i);
    }, origin + " = '" + expected + "'" + (nameExtra ? nameExtra : ""));
}
