(function() {
  class ServiceWorkerHelper {
    constructor(dp, session) {
      this._dp = dp;
      this._session = session;
    }

    async installSWAndWaitForActivated(swUrl) {
      await this._session.evaluateAsync(`
        (async function() {
          const reg = await navigator.serviceWorker.register('${swUrl}');
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
