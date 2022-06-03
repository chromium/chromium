(function() {
  class ServiceWorkerHelper {
    constructor(dp, session) {
      this._dp = dp;
      this._session = session;
    }

    async installSWAndWaitForActivated(swUrl, options = {}) {
      await this._session.evaluateAsync(`
        (async function() {
          const opt = JSON.parse('${JSON.stringify(options)}');
          const reg = await navigator.serviceWorker.register('${swUrl}', opt);
          const worker = reg.installing || reg.waiting || reg.active;
          if (worker.state === 'activated')
            return;
          return new Promise(resolve => {
            worker.addEventListener('statechange', () => {
              if (worker.state === 'activated')
                resolve();
            });
          });
        })()`);
    }
  };

  return (dp, session) => {
    return new ServiceWorkerHelper(dp, session);
  };
})()
