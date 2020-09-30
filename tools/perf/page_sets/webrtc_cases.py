# Copyright 2014 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from page_sets import press_story
from telemetry import story


class WebrtcPage(press_story.PressStory):

  def __init__(self, url, page_set, name, tags):
    assert url.startswith('file://webrtc_cases/')
    self.URL = url
    self.NAME = name
    super(WebrtcPage, self).__init__(page_set, tags=tags)


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
        'bytes',
        'receiveProgress.value',
        description='Amount of data transferred by data channel in 10 seconds')


class AudioCall(WebrtcPage):
  """Why: Sets up a WebRTC audio call."""

  def __init__(self, page_set, codec, tags):
    super(AudioCall, self).__init__(
        url='file://webrtc_cases/audio.html?codec=%s' % codec,
        name='audio_call_%s_10s' % codec.lower(),
        page_set=page_set, tags=tags)
    self.codec = codec

  def ExecuteTest(self, action_runner):
    action_runner.ExecuteJavaScript('codecSelector.value="%s";' % self.codec)
    action_runner.ClickElement('button[id="callButton"]')
    action_runner.Wait(10)

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
