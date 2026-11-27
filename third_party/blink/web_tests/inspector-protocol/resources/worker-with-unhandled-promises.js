function runPromisesInWorker() {
  Promise.reject(new Error('err1')).then().then().then();  // Last is unhandled.

  var reject;
  var m0 = new Promise(function(res, rej) {
    reject = rej;
  });
  var m1 = m0.then(function() {});
  var m2 = m0.then(function() {});
  var m3 = m0.then(function() {});
  var m4 = 0;
  m0.catch(function() {
    m2.catch(function() {
      m1.catch(function() {
        m4 = m3.then(function() {});  // Unhandled.
      });
    });
  });
  reject(new Error('err2'));
}

onmessage = function(event) {
  runPromisesInWorker();
  setInterval(doWork, 0);
};

var message_id = 0;
function doWork() {
  postMessage('Message #' + message_id++);
}
