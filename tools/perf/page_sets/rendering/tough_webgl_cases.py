# Copyright 2014 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags
from page_sets.system_health import platforms


class ToughWebglPage(rendering_story.RenderingStory):
  ABSTRACT_STORY = True
  TAGS = [story_tags.REQUIRED_WEBGL, story_tags.TOUGH_WEBGL]

  def __init__(self,
               page_set,
               shared_page_state_class,
               name_suffix='',
               extra_browser_args=None):
    if extra_browser_args is None:
      extra_browser_args = []
    extra_browser_args.append("--enable-webgl-draft-extensions")
    extra_browser_args.append("--disable-features=V8TurboFastApiCalls")
    super(ToughWebglPage, self).__init__(
        page_set=page_set,
        shared_page_state_class=shared_page_state_class,
        name_suffix=name_suffix,
        extra_browser_args=extra_browser_args,
        make_javascript_deterministic=False)

  @property
  def skipped_gpus(self):
    # crbug.com/462729
    return ['arm', 'broadcom', 'hisilicon', 'imagination', 'vivante']

  def RunNavigateSteps(self, action_runner):
    super(ToughWebglPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        'document.readyState == "complete"')
    action_runner.Wait(2)

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('WebGLAnimation'):
      action_runner.Wait(10)


class NvidiaVertexBufferObjectPage(ToughWebglPage):
  BASE_NAME = 'nvidia_vertex_buffer_object'
  # pylint: disable=line-too-long
  URL = 'http://www.khronos.org/registry/webgl/sdk/demos/google/nvidia-vertex-buffer-object/index.html'
  TAGS = ToughWebglPage.TAGS + [story_tags.REPRESENTATIVE_WIN_DESKTOP]


class SansAngelesPage(ToughWebglPage):
  BASE_NAME = 'san_angeles'
  # pylint: disable=line-too-long
  URL = 'http://www.khronos.org/registry/webgl/sdk/demos/google/san-angeles/index.html'


class ParticlesPage(ToughWebglPage):
  BASE_NAME = 'particles'
  # pylint: disable=line-too-long
  URL = 'http://www.khronos.org/registry/webgl/sdk/demos/google/particles/index.html'


class EarthPage(ToughWebglPage):
  BASE_NAME = 'earth'
  URL = 'http://www.khronos.org/registry/webgl/sdk/demos/webkit/Earth.html'


class ManyPlanetsDeepPage(ToughWebglPage):
  BASE_NAME = 'many_planets_deep'
  # pylint: disable=line-too-long
  URL = 'http://www.khronos.org/registry/webgl/sdk/demos/webkit/ManyPlanetsDeep.html'
  TAGS = ToughWebglPage.TAGS + [story_tags.REPRESENTATIVE_WIN_DESKTOP]


class Aquarium(ToughWebglPage):
  BASE_NAME = 'aquarium'
  URL = 'http://webglsamples.org/aquarium/aquarium.html'
  TAGS = ToughWebglPage.TAGS + [story_tags.REPRESENTATIVE_WIN_DESKTOP]


class Aquarium20KFish(ToughWebglPage):
  BASE_NAME = 'aquarium_20k'
  URL = 'http://webglsamples.org/aquarium/aquarium.html?numFish=20000'
  TAGS = ToughWebglPage.TAGS + [story_tags.REPRESENTATIVE_WIN_DESKTOP]

class Blob(ToughWebglPage):
  BASE_NAME = 'blob'
  URL = 'http://webglsamples.org/blob/blob.html'


class DynamicCubeMap(ToughWebglPage):
  BASE_NAME = 'dynamic_cube_map'
  URL = 'http://webglsamples.org/dynamic-cubemap/dynamic-cubemap.html'


class AnimometerWebGL(ToughWebglPage):
  BASE_NAME = 'animometer_webgl'
  # pylint: disable=line-too-long
  URL = 'http://kenrussell.github.io/webgl-animometer/Animometer/tests/3d/webgl.html'


class AnimometerWebGLMultiDraw(ToughWebglPage):
  BASE_NAME = 'animometer_webgl_multi_draw'
  # pylint: disable=line-too-long
  URL = 'http://kenrussell.github.io/webgl-animometer/Animometer/tests/3d/webgl.html?webgl_version=2&use_ubos=1&use_multi_draw=1'


class AnimometerWebGLIndexed(ToughWebglPage):
  BASE_NAME = 'animometer_webgl_indexed'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  # pylint: disable=line-too-long
  URL = 'http://kenrussell.github.io/webgl-animometer/Animometer/tests/3d/webgl-indexed-instanced.html?webgl_version=2&use_attributes=1&num_geometries=120000'


class AnimometerWebGLIndexedMultiDraw(ToughWebglPage):
  BASE_NAME = 'animometer_webgl_indexed_multi_draw'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  # pylint: disable=line-too-long
  URL = 'http://kenrussell.github.io/webgl-animometer/Animometer/tests/3d/webgl-indexed-instanced.html?webgl_version=2&use_attributes=1&use_multi_draw=1&num_geometries=120000'


class AnimometerWebGLIndexedBaseVertexBaseInstance(ToughWebglPage):
  BASE_NAME = 'animometer_webgl_indexed_multi_draw_base_vertex_base_instance'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  # pylint: disable=line-too-long
  URL = 'http://kenrussell.github.io/webgl-animometer/Animometer/tests/3d/webgl-indexed-instanced.html?webgl_version=2&use_attributes=1&use_multi_draw=1&use_base_vertex_base_instance=1&num_geometries=120000'


class AnimometerWebGLAttribArrays(ToughWebglPage):
  BASE_NAME = 'animometer_webgl_attrib_arrays'
  # pylint: disable=line-too-long
  URL = 'http://kenrussell.github.io/webgl-animometer/Animometer/tests/3d/webgl.html?use_attributes=1'
  TAGS = ToughWebglPage.TAGS + [
    story_tags.REPRESENTATIVE_MAC_DESKTOP
  ]

class CameraToWebGL(ToughWebglPage):
  TAGS = ToughWebglPage.TAGS + [story_tags.USE_FAKE_CAMERA_DEVICE]
  BASE_NAME = 'camera_to_webgl'
  # pylint: disable=line-too-long
  URL = 'https://www.khronos.org/registry/webgl/sdk/tests/extra/texture-from-camera-stress.html?uploadsPerFrame=200'


class UnityPage(ToughWebglPage):
  ABSTRACT_STORY = True

  def RunNavigateSteps(self, action_runner):
    super(UnityPage, self).RunNavigateSteps(action_runner)
    # Wait an additional 10 seconds for any loading screens
    # or interaction to click "Play"
    action_runner.Wait(10)

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('WebGLAnimation'):
      action_runner.Wait(30)


class SkelebuddiesWasm2020(UnityPage):
  BASE_NAME = 'skelebuddies_wasm_2020'
  # pylint: disable=line-too-long
  URL = 'http://clb.confined.space/emunittest/Skelebuddies-Wasm-Release-2020-10-26-profiling/Skelebuddies.html?playback'

class TinyRacingV3Wasm2020(UnityPage):
  BASE_NAME = 'tiny_racing_v3_wasm_2020'
  # pylint: disable=line-too-long
  URL = 'http://clb.confined.space/emunittest/llvm-tinyracing-wasm-release-2020-03-17/TinyRacing.html?playback'

class MicrogameFPS(UnityPage):
  BASE_NAME = 'microgame_fps'
  # pylint: disable=line-too-long
  URL = 'http://clb.confined.space/emunittest/microgame-fps_20190922_131915_wasm_release_profiling/index.html?playback'

class LostCrypt(UnityPage):
  BASE_NAME = 'lost_crypt'
  SUPPORTED_PLATFORMS = platforms.DESKTOP_ONLY
  # pylint: disable=line-too-long
  URL = 'http://clb.confined.space/emunittest/LostCrypt_20191220_131436_wasm_release/index.html?playback'


def MakeFastCallVariant(cls):
  def __init__(self,
               page_set,
               shared_page_state_class,
               name_suffix='',
               extra_browser_args=None):
    if extra_browser_args is None:
      extra_browser_args = []
    super(cls, self).__init__(page_set=page_set,
                              shared_page_state_class=shared_page_state_class,
                              name_suffix=name_suffix,
                              extra_browser_args=extra_browser_args)
    # This has to be after after superclass init in order to override the args
    # added by ToughWebglPage.__init__
    extra_browser_args.remove("--disable-features=V8TurboFastApiCalls")
    extra_browser_args.append("--enable-features=V8TurboFastApiCalls")

  return type(
      cls.__name__ + 'FastCall', (cls,), {
          'BASE_NAME':
          cls.BASE_NAME + '_fast_call',
          'SUPPORTED_PLATFORMS':
          cls.SUPPORTED_PLATFORMS.intersection(platforms.DESKTOP_ONLY),
          '__init__':
          __init__,
      })


AnimometerWebGLFastCall = MakeFastCallVariant(AnimometerWebGL)
AnimometerWebGLIndexedFastCall = MakeFastCallVariant(AnimometerWebGLIndexed)
Aquarium20KFishFastCall = MakeFastCallVariant(Aquarium20KFish)
SkelebuddiesWasm2020FastCall = MakeFastCallVariant(SkelebuddiesWasm2020)
TinyRacingV3Wasm2020FastCall = MakeFastCallVariant(TinyRacingV3Wasm2020)
MicrogameFPSFastCall = MakeFastCallVariant(MicrogameFPS)
LostCryptFastCall = MakeFastCallVariant(LostCrypt)
