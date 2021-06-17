test(() => {
    const path = new URL("resources/initiator-type-early-hints-preload.h2.py",
                         window.location);
    window.location.replace(path);
});
