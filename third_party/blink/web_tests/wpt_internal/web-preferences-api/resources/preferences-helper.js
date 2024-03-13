window.changeEventPromise = function changeEventPromise(preference) {
    return Promise.race([
        new Promise(resolve => {
            navigator.preferences[preference].onchange = resolve;
        }),
        new Promise((resolve, reject) => {
            setTimeout(() => {
                reject(`Change event for ${preference} preference not fired.`);
            }, 500);
        })
    ]);
}