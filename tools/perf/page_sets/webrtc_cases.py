# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from page_sets import press_story
from telemetry import story


class WebrtcPage(press_story.PressStory):

  def __init__(self, url, page_set, name, tags, extra_browser_args=None):
    assert url.startswith('file://webrtc_cases/')
    self.URL = url
    self.NAME = name
    super(WebrtcPage, self).__init__(page_set,
                                     tags=tags,
                                     extra_browser_args=extra_browser_args)


class GetUserMedia(WebrtcPage):
  """Why: Acquires a high definition (720p) local stream."""

  def __init__(self, page_set, tags):
    super(GetUserMedia, self).__init__(
        url='file://webrtc_cases/resolution.html',
        name='hd_local_stream_10s',
        page_set=page_set, tags=tags)

  def ExecuteTest(self, action_runner):
    action_runner.ClickElement('button[id="hd"]')
    action_runner.Wait(10)


class DataChannel(WebrtcPage):
  """Why: Transfer as much data as possible through a data channel in 10s."""

  def __init__(self, page_set, tags):
    super(DataChannel, self).__init__(
        url='file://webrtc_cases/datatransfer.html',
        name='10s_datachannel_transfer',
        page_set=page_set, tags=tags)

  def ExecuteTest(self, action_runner):
    action_runner.ExecuteJavaScript('megsToSend.value = 100;')
    action_runner.ClickElement('button[id="sendTheData"]')
    action_runner.Wait(10)

  def ParseTestResults(self, action_runner):
    self.AddJavaScriptMeasurement(
        'data_transferred',
        'sizeInBytes_biggerIsBetter',
        'receiveProgress.value',
        description='Amount of data transferred by data channel in 10 seconds')
    self.AddJavaScriptMeasurement(
        'data_throughput',
        'bytesPerSecond_biggerIsBetter',
        'currentThroughput',
        description='Throughput of the data transfer.')


class CanvasCapturePeerConnection(WebrtcPage):
  """Why: Sets up a canvas capture stream connection to a peer connection."""

  def __init__(self, page_set, tags):
    super(CanvasCapturePeerConnection, self).__init__(
        url='file://webrtc_cases/canvas-capture.html',
        name='canvas_capture_peer_connection',
        page_set=page_set, tags=tags)

  def ExecuteTest(self, action_runner):
    with action_runner.CreateInteraction('Action_Canvas_PeerConnection',
                                         repeatable=False):
      action_runner.ClickElement('button[id="startButton"]')
      action_runner.Wait(10)


class VideoCodecConstraints(WebrtcPage):
  """Why: Sets up a video codec to a peer connection."""

  def __init__(self, page_set, video_codec, tags):
    super(VideoCodecConstraints, self).__init__(
        url='file://webrtc_cases/codec_constraints.html',
        name='codec_constraints_%s' % video_codec.lower(),
        page_set=page_set, tags=tags)
    self.video_codec = video_codec

  def ExecuteTest(self, action_runner):
    with action_runner.CreateInteraction('Action_Codec_Constraints',
                                         repeatable=False):
      action_runner.ClickElement('input[id="%s"]' % self.video_codec)
      action_runner.ClickElement('button[id="startButton"]')
      action_runner.WaitForElement('button[id="callButton"]:enabled')
      action_runner.ClickElement('button[id="callButton"]')
      action_runner.Wait(20)
      action_runner.ClickElement('button[id="hangupButton"]')


class MultiplePeerConnections(WebrtcPage):
  """Why: Sets up several peer connections in the same page."""

  def __init__(self, page_set, tags):
    super(MultiplePeerConnections, self).__init__(
        url='file://webrtc_cases/multiple-peerconnections.html',
        name='multiple_peerconnections',
        page_set=page_set, tags=tags)

  def ExecuteTest(self, action_runner):
    with action_runner.CreateInteraction('Action_Create_PeerConnection',
                                         repeatable=False):
      # Set the number of peer connections to create to 10.
      action_runner.ExecuteJavaScript(
          'document.getElementById("num-peerconnections").value=10')
      action_runner.ExecuteJavaScript(
          'document.getElementById("cpuoveruse-detection").checked=false')
      action_runner.ClickElement('button[id="start-test"]')
      action_runner.Wait(20)


class PausePlayPeerConnections(WebrtcPage):
  """Why: Ensures frequent pause and plays of peer connection streams work."""

  def __init__(self, page_set, tags):
    super(PausePlayPeerConnections, self).__init__(
        url='file://webrtc_cases/pause-play.html',
        name='pause_play_peerconnections',
        page_set=page_set, tags=tags)

  def ExecuteTest(self, action_runner):
    action_runner.ExecuteJavaScript(
        'startTest({test_runtime_s}, {num_peerconnections},'
        '{iteration_delay_ms}, "video");'.format(
            test_runtime_s=20, num_peerconnections=10, iteration_delay_ms=20))
    action_runner.Wait(20)


class InsertableStreamsAudioProcessing(WebrtcPage):
  """Why: processes/transforms audio using insertable streams."""

  def __init__(self, page_set, tags):
    super(InsertableStreamsAudioProcessing, self).__init__(
        url='file://webrtc_cases/audio-processing.html',
        name='insertable_streams_audio_processing',
        page_set=page_set,
        tags=tags,
        extra_browser_args=(
            '--enable-blink-features=WebCodecs,MediaStreamInsertableStreams'))
    self.supported = None

  def RunNavigateSteps(self, action_runner):
    self.supported = action_runner.EvaluateJavaScript('''(function () {
  try {
    new MediaStreamTrackGenerator('audio');
    return true;
  } catch (e) {
    return false;
  }
})()''')
    if self.supported:
      super(InsertableStreamsAudioProcessing,
            self).RunNavigateSteps(action_runner)

  def ExecuteTest(self, action_runner):
    self.AddMeasurement(
        'supported', 'count_biggerIsBetter', 1 if self.supported else 0,
        'Boolean flag indicating if this benchmark is supported by the browser.'
    )
    if not self.supported:
      return
    action_runner.WaitForJavaScriptCondition('!!audio')
    action_runner.ExecuteJavaScript('start()')
    action_runner.Wait(10)


class InsertableStreamsVideoProcessing(WebrtcPage):
  """Why: processes/transforms video in various ways."""

  def __init__(self, page_set, source, transform, sink, tags):
    super(InsertableStreamsVideoProcessing, self).__init__(
        url='file://webrtc_cases/video-processing.html',
        name=('insertable_streams_video_processing_%s_%s_%s' %
              (source, transform, sink)),
        page_set=page_set,
        tags=tags,
        extra_browser_args=(
            '--enable-blink-features=WebCodecs,MediaStreamInsertableStreams'))
    self.source = source
    self.transform = transform
    self.sink = sink
    self.supported = None

  def RunNavigateSteps(self, action_runner):
    self.supported = action_runner.EvaluateJavaScript(
        "typeof MediaStreamTrackProcessor !== 'undefined' &&"
        "typeof MediaStreamTrackGenerator !== 'undefined'")
    if self.supported:
      super(InsertableStreamsVideoProcessing,
            self).RunNavigateSteps(action_runner)

  def ExecuteTest(self, action_runner):
    self.AddMeasurement(
        'supported', 'count_biggerIsBetter', 1 if self.supported else 0,
        'Boolean flag indicating if this benchmark is supported by the browser.'
    )
    if not self.supported:
      return
    with action_runner.CreateInteraction('Start_Pipeline', repeatable=True):
      action_runner.WaitForElement('select[id="sourceSelector"]:enabled')
      action_runner.ExecuteJavaScript(
          'document.getElementById("sourceSelector").value="%s";' % self.source)
      action_runner.WaitForElement('select[id="transformSelector"]:enabled')
      action_runner.ExecuteJavaScript(
          'document.getElementById("transformSelector").value="%s";' %
          self.transform)
      action_runner.WaitForElement('select[id="sinkSelector"]:enabled')
      action_runner.ExecuteJavaScript(
          'document.getElementById("sinkSelector").value="%s";' % self.sink)
      action_runner.ExecuteJavaScript(
          'document.getElementById("sourceSelector").dispatchEvent('
          '  new InputEvent("input", {}));')
      action_runner.WaitForElement('.sinkVideo')
      action_runner.Wait(10)
    self.AddJavaScriptMeasurement(
        'sink_decoded_frames',
        'count_biggerIsBetter',
        'document.querySelector(".sinkVideo").webkitDecodedFrameCount',
        description='Number of frames received at the sink video.')


class NegotiateTiming(WebrtcPage):
  """Why: Measure how long renegotiation takes with large SDP blobs."""

  def __init__(self, page_set, tags):
    super(NegotiateTiming,
          self).__init__(url='file://webrtc_cases/negotiate-timing.html',
                         name='negotiate-timing',
                         page_set=page_set,
                         tags=tags)

  def ExecuteTest(self, action_runner):
    action_runner.ExecuteJavaScript('start()')
    action_runner.WaitForJavaScriptCondition('!callButton.disabled')
    action_runner.ExecuteJavaScript('call()')
    action_runner.WaitForJavaScriptCondition('!renegotiateButton.disabled')
    # Due to suspicion of renegotiate activating too early:
    action_runner.Wait(1)
    # Negotiate 50 transceivers, then negotiate back to 1, simulating Meet "pin"
    action_runner.ExecuteJavaScript('videoSectionsField.value = 50')
    action_runner.ExecuteJavaScript('renegotiate()')
    action_runner.WaitForJavaScriptCondition('!renegotiateButton.disabled')
    action_runner.ExecuteJavaScript('videoSectionsField.value = 1')
    action_runner.ExecuteJavaScript('renegotiate()')
    action_runner.WaitForJavaScriptCondition('!renegotiateButton.disabled')
    # Negotiate back up to 50, simulating Meet "unpin". This is what gets measured.
    action_runner.ExecuteJavaScript('videoSectionsField.value = 50')
    action_runner.ExecuteJavaScript('renegotiate()')
    action_runner.WaitForJavaScriptCondition('!renegotiateButton.disabled')
    result = action_runner.EvaluateJavaScript('result')

    self.AddMeasurement('callerSetLocalDescription',
                        'ms',
                        result['callerSetLocalDescription'],
                        description='Time for caller SetLocalDescription')
    self.AddMeasurement('calleeSetLocalDescription',
                        'ms',
                        result['calleeSetLocalDescription'],
                        description='Time for callee SetLocalDescription')
    self.AddMeasurement('callerSetRemoteDescription',
                        'ms',
                        result['callerSetRemoteDescription'],
                        description='Time for caller SetRemoteDescription')
    self.AddMeasurement('calleeSetRemoteDescription',
                        'ms',
                        result['calleeSetRemoteDescription'],
                        description='Time for callee SetRemoteDescription')
    self.AddMeasurement('callerCreateOffer',
                        'ms',
                        result['callerCreateOffer'],
                        description='Time for overall offer/answer handshake')
    self.AddMeasurement('calleeCreateAnswer',
                        'ms',
                        result['calleeCreateAnswer'],
                        description='Time for overall offer/answer handshake')
    self.AddMeasurement('elapsedTime',
                        'ms',
                        result['elapsedTime'],
                        description='Time for overall offer/answer handshake')
    self.AddMeasurement(
        'audioImpairment',
        'count',
        result['audioImpairment'],
        description='Number of late audio samples concealed during negotiation')


class EncodedInsertableStreams(WebrtcPage):
  """Why: Performs encoded insertable streams."""
  def __init__(self, page_set, tags):
    super(EncodedInsertableStreams, self).__init__(
        url='file://webrtc_cases/encoded-insertable-streams.html',
        name='encoded_insertable_streams',
        page_set=page_set,
        tags=tags)

  def ExecuteTest(self, action_runner):
    with action_runner.CreateInteraction('Action_Create_PeerConnection',
                                         repeatable=False):
      # Set the number of peer connections to create to 10.
      action_runner.ExecuteJavaScript(
          'document.getElementById("num-peerconnections").value=10')
      action_runner.ClickElement('button[id="start-test"]')
      action_runner.Wait(20)


class WebrtcPageSet(story.StorySet):
  def __init__(self):
    super(WebrtcPageSet, self).__init__(
        cloud_storage_bucket=story.PUBLIC_BUCKET)

    self.AddStory(PausePlayPeerConnections(self, tags=['pauseplay']))
    self.AddStory(MultiplePeerConnections(self, tags=['stress']))
    self.AddStory(DataChannel(self, tags=['datachannel']))
    self.AddStory(GetUserMedia(self, tags=['getusermedia']))
    self.AddStory(CanvasCapturePeerConnection(self, tags=['smoothness']))
    self.AddStory(VideoCodecConstraints(self, 'H264', tags=['videoConstraints']))
    self.AddStory(VideoCodecConstraints(self, 'VP8', tags=['videoConstraints']))
    self.AddStory(VideoCodecConstraints(self, 'VP9', tags=['videoConstraints']))
    self.AddStory(
        InsertableStreamsAudioProcessing(self, tags=['insertableStreams']))
    self.AddStory(
        InsertableStreamsVideoProcessing(self,
                                         'camera',
                                         'webgl',
                                         'video',
                                         tags=['insertableStreams']))
    self.AddStory(
        InsertableStreamsVideoProcessing(self,
                                         'video',
                                         'webgl',
                                         'video',
                                         tags=['insertableStreams']))
    self.AddStory(
        InsertableStreamsVideoProcessing(self,
                                         'pc',
                                         'webgl',
                                         'video',
                                         tags=['insertableStreams']))
    self.AddStory(
        InsertableStreamsVideoProcessing(self,
                                         'camera',
                                         'canvas2d',
                                         'video',
                                         tags=['insertableStreams']))
    self.AddStory(
        InsertableStreamsVideoProcessing(self,
                                         'camera',
                                         'noop',
                                         'video',
                                         tags=['insertableStreams']))
    self.AddStory(
        InsertableStreamsVideoProcessing(self,
                                         'camera',
                                         'webgl',
                                         'pc',
                                         tags=['insertableStreams']))
    self.AddStory(NegotiateTiming(self, tags=['sdp']))
    self.AddStory(EncodedInsertableStreams(self, tags=['stress']))
