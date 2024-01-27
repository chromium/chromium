export async function checkStateTransition(options) {
    const port = options.port;
    const access = options.access;
    assert_equals(options.initialconnection, port.connection);
    const checkHandler = function(e) {
        assert_not_equals(options.initialconnection, options.finalconnection);
        assert_equals(e.port.id, options.port.id);
        assert_equals(e.port.connection, options.finalconnection);
    };
    const portPromise = () => new Promise(resolve => {
        port.onstatechange = e => {
            checkHandler(e);
            resolve();
        };
    });
    const accessPromise = () => new Promise(resolve => {
        access.onstatechange = e => {
            checkHandler(e);
            resolve();
        };
    });
    if (options.method == "setonmidimessage") {
        port.onmidimessage = function() {};
        return Promise.all([portPromise(), accessPromise()]);
    }
    if (options.method == "addeventlistener") {
        port.addEventListener("midimessage", function() {});
        return Promise.all([portPromise(), accessPromise()]);
    }
    if (options.method == "send") {
        port.send([]);
        return Promise.all([portPromise(), accessPromise()]);
    }

    assert_in_array(options.method, ["open", "close"]);
    const p = await port[options.method]();
    assert_equals(p.id, options.port.id);
    assert_equals(p.connection, options.finalconnection);
    assert_equals(port.connection, options.finalconnection);
}
