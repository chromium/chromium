<?
header('Content-Type: application/javascript');
header('Referrer-Policy: origin');
?>
importScripts('save-referrer.php');

if ('DedicatedWorkerGlobalScope' in self &&
    self instanceof DedicatedWorkerGlobalScope) {
  postMessage(referrer);
} else if (
    'SharedWorkerGlobalScope' in self &&
    self instanceof SharedWorkerGlobalScope) {
  onconnect = e => {
    var port = e.ports[0];
    port.postMessage(referrer);
  };
}
