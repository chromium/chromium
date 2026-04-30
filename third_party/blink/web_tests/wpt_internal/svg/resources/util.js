import {WebFeature} from '/gen/third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.m.js';

export function expectCounters(local, remote, plugin, context) {
  let counters = getCounters();
  assert_equals(
    printCounters(counters.local, counters.remote, counters.plugin),
    printCounters(local, remote, plugin),
    context
  );
}

export function expectIframeCounters(message, local, remote, plugin, context) {
  assert_equals(
    printCounters(message.local, message.remote, message.plugin),
    printCounters(local, remote, plugin),
    context
  );
}

export function getCounters() {
  return {
    local: internals.isUseCounted(document, WebFeature.kSvgFilterPaintedOnLocalFrame),
    remote: internals.isUseCounted(document, WebFeature.kSvgFilterPaintedOnRemoteFrame),
    plugin: internals.isUseCounted(document, WebFeature.kSvgFilterPaintedOnWebPlugin),
  };
}

export async function getIframeResults(id, url=undefined) {
  return await new Promise(resolve => {
    window.addEventListener('message', (e) => {
      resolve(e.data);
    }, {once: true});
    if (url != undefined) {
      document.getElementById(id).setAttribute('src', url);
    } else {
      document.getElementById(id).contentWindow.postMessage({}, '*');
    }
  });
}

export function printCounters(local, remote, plugin) {
  return `Local: ${local}, Remote: ${remote}, Plugin: ${plugin}`;
}

export function resetCounters() {
  internals.clearUseCounter(document, WebFeature.kSvgFilterPaintedOnLocalFrame);
  internals.clearUseCounter(document, WebFeature.kSvgFilterPaintedOnRemoteFrame);
  internals.clearUseCounter(document, WebFeature.kSvgFilterPaintedOnWebPlugin);
}

export async function waitForLoad() {
  if (document.readyState === 'complete') {
    return;
  }
  await new Promise(resolve => window.addEventListener('load', resolve));
}

export async function waitForRender() {
  await new Promise(requestAnimationFrame);
  await new Promise(requestAnimationFrame);
  await new Promise(setTimeout);
}