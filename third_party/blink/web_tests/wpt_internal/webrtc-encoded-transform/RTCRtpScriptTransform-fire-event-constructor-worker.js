onrtctransform = async (e) => {
    if(e.transformer.options && e.transformer.options.port) {
        e.transformer.options.port.onmessage = (event) => {
            if (event.data == "ping") {
                e.transformer.options.port.postMessage("pong");
            }
        };
    } else {
        postMessage(e.transformer.options);
    }
}
