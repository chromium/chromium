export function log(type: string): (...message: any[]) => void {
    return (...messages: any[]) => {
        const elementId = type + "_log";

        if (!window.document.getElementById(elementId)) {
            window.document.documentElement.innerHTML += `<h3>${type}:</h3><pre id='${elementId}'></pre>`;
            window.document.getElementById(elementId);
        }

        const element = window.document.getElementById(elementId);

        console.log.apply(null, [type].concat(messages));
        element.innerText += messages.join(", ") + "\n";
    }
}
