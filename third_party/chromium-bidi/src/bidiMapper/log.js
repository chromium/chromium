export function cdp_log(message) {
    log(message, "cdp");
}
export function bidi_log(message) {
    log(message, "bidi");
}
export function log(message, type) {
    console.log(type, message);
    getLogElement(type).innerHTML += message + "\n";
}

function getLogElement(type) {
    if (type === "cdp")
        return window.document.getElementById("cdp_log");
    if (type === "bidi")
        return window.document.getElementById("bidi_log");
    return window.document.getElementById("system_log");
}