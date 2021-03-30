/*
 * mediasessionservice-mock contains a mock implementation of
 * MediaSessionService.
 */

import {MediaSessionService, MediaSessionServiceReceiver} from '/gen/third_party/blink/public/mojom/mediasession/media_session.mojom.m.js';

function mojoString16ToJS(mojoString16) {
  return String.fromCharCode.apply(null, mojoString16.data);
}

function mojoImageToJS(mojoImage) {
  var src = mojoImage.src.url;
  var type = mojoString16ToJS(mojoImage.type);
  var sizes = '';
  for (var i = 0; i < mojoImage.sizes.length; i++) {
    if (i > 0)
      sizes += ' ';

    var mojoSize = mojoImage.sizes[i];
    sizes += mojoSize.width.toString() + 'x' + mojoSize.height.toString();
  }
  return {src, type, sizes};
}

function mojoMetadataToJS(mojoMetadata) {
  if (mojoMetadata == null)
    return null;

  var title = mojoString16ToJS(mojoMetadata.title);
  var artist = mojoString16ToJS(mojoMetadata.artist);
  var album = mojoString16ToJS(mojoMetadata.album);
  var artwork = [];
  for (var i = 0; i < mojoMetadata.artwork.length; i++)
    artwork.push(mojoImageToJS(mojoMetadata.artwork[i]));

  return new MediaMetadata({title, artist, album, artwork});
}

export class MediaSessionServiceMock {
  constructor() {
    this.pendingResponse_ = null;
    this.receiver_ = new MediaSessionServiceReceiver(this);

    this.interceptor_ =
        new MojoInterfaceInterceptor(MediaSessionService.$interfaceName);
    this.interceptor_.oninterfacerequest = e =>
        this.receiver_.$.bindHandle(e.handle);
    this.interceptor_.start();
  }

  setMetadata(metadata) {
    if (!!this.metadataCallback_)
      this.metadataCallback_(mojoMetadataToJS(metadata));
  }

  setMetadataCallback(callback) {
    this.metadataCallback_ = callback;
  }

  setPlaybackState(state) {
    if (!!this.setPlaybackStateCallback_)
      this.setPlaybackStateCallback_(state);
  }

  setPlaybackStateCallback(callback) {
    this.setPlaybackStateCallback_ = callback;
  }

  setPositionState(position) {
    if (!!this.setPositionStateCallback_)
      this.setPositionStateCallback_(position);
  }

  setPositionStateCallback(callback) {
    this.setPositionStateCallback_ = callback;
  }

  setMicrophoneState(microphoneState) {
    if (!!this.setMicrophoneStateCallback_)
      this.setMicrophoneStateCallback_(microphoneState);
  }

  setMicrophoneStateCallback(callback) {
    this.setMicrophoneStateCallback_ = callback;
  }

  setCameraState(cameraState) {
    if (!!this.setCameraStateCallback_)
      this.setCameraStateCallback_(cameraState);
  }

  setCameraStateCallback(callback) {
    this.setCameraStateCallback_ = callback;
  }

  enableAction(action) {
    if (!!this.enableDisableActionCallback_)
      this.enableDisableActionCallback_(action, true);
  }

  disableAction(action) {
    if (!!this.enableDisableActionCallback_)
      this.enableDisableActionCallback_(action, false);
  }

  setEnableDisableActionCallback(callback) {
    this.enableDisableActionCallback_ = callback;
  }

  setClient(client) {
    this.client_ = client;
    if (!!this.clientCallback_)
      this.clientCallback_();
  }

  setClientCallback(callback) {
    this.clientCallback_ = callback;
  }

  getClient() {
    return this.client_;
  }
}
