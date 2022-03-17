importScripts("/speculation-rules/prerender/resources/utils.js");

const params = new URLSearchParams(location.search);
const uid = params.get('uid');

const bc = new PrerenderChannel('worker-channel', uid);
bc.onmessage = e => {
  bc.postMessage(e.data + 'pong');
  bc.close();
};
