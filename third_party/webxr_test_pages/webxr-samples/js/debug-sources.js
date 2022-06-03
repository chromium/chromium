// Utility for creating an element with attributes and text
let makeElem = function(type, attrs, text) {
  let elem = document.createElement(type);
  if (attrs) {
    for (let attr in attrs) elem[attr] = attrs[attr];
  }
  if (text) {
    elem.innerText = text;
  }
  return elem;
}

let showEvent = function(ev) {
  let elem = document.getElementById('ev-' + ev.type);
  if (!elem) return;
  elem.classList.remove('changed');
  void elem.offsetWidth; // trigger reflow
  elem.classList.add('changed');
}

export class DebugSources {
  constructor() {
    console.log('DebugSources constructor');
    let style = document.createElement('link');
    style.rel = 'stylesheet';
    style.type = 'text/css';
    style.href = '../css/debug-sources.css';
    document.head.appendChild(style);

    let div = makeElem('div', {className: 'debug-sources'}, 'XR events: ');
    div.appendChild(makeElem('span', {id: 'ev-selectstart', className: 'evstate'}, 'start'));
    div.appendChild(makeElem('span', {id: 'ev-selectend', className: 'evstate'}, 'end'));
    div.appendChild(makeElem('span', {id: 'ev-select', className: 'evstate'}, 'select'));
    div.appendChild(makeElem('br'));
    div.appendChild(makeElem('span', {id: 'ev-beforexrselect', className: 'evstate'}, 'beforexrselect'));
    div.appendChild(makeElem('br'));
    div.appendChild(makeElem('span', {id: 'ev-inputsourceschange', className: 'evstate'}, 'inputsourceschange'));
    div.appendChild(makeElem('pre', {id: 'debug-sources-list'}));
    document.body.insertBefore(div, document.body.firstChild);

    document.body.addEventListener('beforexrselect', showEvent);

    this.sourceCounter = 0;
  }

  startSession(session) {
    session.addEventListener('selectstart', showEvent);
    session.addEventListener('selectend', showEvent);
    session.addEventListener('select', showEvent);
    session.addEventListener('inputsourceschange', showEvent);

    session.addEventListener('inputsourceschange', function(ev) {
      document.getElementById('ev-inputsourceschange').innerText =
          'inputsourceschange' +
          ' add=' + ev.added.length +
          ' remove=' + ev.removed.length +
          ' total=' + ev.session.inputSources.length;
    });
  }

  update(frame, xrSpace) {
    let inputSources = frame.session.inputSources;
    let sourceDebug = [];
    for (let i = 0; i < inputSources.length; ++i) {
      let source = inputSources[i];

      if(source.number == null) {
        source.number = this.sourceCounter;
        this.sourceCounter++;
      }

      let ray_debug = 'n/a';
      if (source.targetRaySpace) {
        let rayPose = frame.getPose(source.targetRaySpace, xrSpace);
        let m = rayPose.transform.matrix;
        ray_debug = m[12].toFixed(3) + ',' + m[13].toFixed(3) + ',' + m[14].toFixed(3);
      }
      let grip_debug = 'n/a';
      if (source.gripSpace) {
        let gripPose = frame.getPose(source.targetRaySpace, xrSpace);
        let m = gripPose.transform.matrix;
        grip_debug = m[12].toFixed(3) + ',' + m[13].toFixed(3) + ',' + m[14].toFixed(3);
      }
      sourceDebug.push('#' + source.number +
                       '(index: ' + i +') ray:' + ray_debug +
                       ' grip:' + grip_debug);
    }
    document.getElementById('debug-sources-list').innerText =
        sourceDebug.length ? sourceDebug.join('\n') : '\n';
  }
}
