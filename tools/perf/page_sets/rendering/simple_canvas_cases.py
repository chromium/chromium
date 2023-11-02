# Copyright 2020 The Chromium Authors
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
from telemetry.page import shared_page_state

from page_sets.rendering import rendering_story
from page_sets.rendering import story_tags


class SimpleCanvasPage(rendering_story.RenderingStory):
  ABSTRACT_STORY = True
  TAGS = [story_tags.SIMPLE_CANVAS]

  def __init__(self,
               page_set,
               shared_page_state_class=shared_page_state.SharedPageState,
               name_suffix='',
               extra_browser_args=None):
    super(SimpleCanvasPage,
          self).__init__(page_set=page_set,
                         shared_page_state_class=shared_page_state_class,
                         name_suffix=name_suffix,
                         extra_browser_args=extra_browser_args)

  def RunNavigateSteps(self, action_runner):
    super(SimpleCanvasPage, self).RunNavigateSteps(action_runner)
    action_runner.WaitForJavaScriptCondition(
        "document.readyState == 'complete'")

  def RunPageInteractions(self, action_runner):
    with action_runner.CreateInteraction('CanvasAnimation'):
      action_runner.Wait(10)


class Canvas2dToTexture(SimpleCanvasPage):
  BASE_NAME = 'canvas2d_to_texture.html'
  URL = 'file://../simple_canvas/canvas2d_to_texture.html'


class CanvasToCanvasDrawPage(SimpleCanvasPage):
  BASE_NAME = 'canvas_to_canvas_draw'
  URL = 'file://../simple_canvas/canvas_to_canvas_draw.html'


class DocsPaper(SimpleCanvasPage):
  BASE_NAME = 'docs_paper.html'
  URL = 'file://../simple_canvas/docs_paper.html'


class DocsResume(SimpleCanvasPage):
  BASE_NAME = 'docs_resume.html'
  URL = 'file://../simple_canvas/docs_resume.html'


class DocsTable(SimpleCanvasPage):
  BASE_NAME = 'docs_table.html'
  URL = 'file://../simple_canvas/docs_table.html'


class DrawImage(SimpleCanvasPage):
  BASE_NAME = 'draw_image'
  URL = 'file://../simple_canvas/draw_image.html'


class DrawimageNotPixelAligned(SimpleCanvasPage):
  BASE_NAME = 'draw_image_not_pixel_aligned'
  URL = 'file://../simple_canvas/draw_image_not_pixel_aligned.html'


class DynamicCanvasToHWAcceleratedCanvas(SimpleCanvasPage):
  BASE_NAME = 'dynamic_canvas_to_hw_accelerated_canvas.html'
  URL = 'file://../simple_canvas/dynamic_canvas_to_hw_accelerated_canvas.html'


class DynamicWebglToHWAcceleratedCanvas(SimpleCanvasPage):
  BASE_NAME = 'dynamic_webgl_to_hw_accelerated_canvas.html'
  URL = 'file://../simple_canvas/dynamic_webgl_to_hw_accelerated_canvas.html'


class FallingParticleSimulationOnCPU(SimpleCanvasPage):
  BASE_NAME = 'falling_particle_simulation_cpu.html'
  URL = 'file://../simple_canvas/falling_particle_simulation_cpu.html'


class FallingParticleSimulationOnGPU(SimpleCanvasPage):
  BASE_NAME = 'falling_particle_simulation_gpu.html'
  URL = 'file://../simple_canvas/falling_particle_simulation_gpu.html'


class FillClearRect(SimpleCanvasPage):
  BASE_NAME = 'fill_clear_rect.html'
  URL = 'file://../simple_canvas/fill_clear_rect.html'


class GetImageDataOnCPU(SimpleCanvasPage):
  BASE_NAME = 'get_image_data_cpu.html'
  URL = 'file://../simple_canvas/get_image_data_cpu.html'


class GetImageDataOnGPU(SimpleCanvasPage):
  BASE_NAME = 'get_image_data_gpu.html'
  URL = 'file://../simple_canvas/get_image_data_gpu.html'


class GpuBoundShader(SimpleCanvasPage):
  BASE_NAME = 'gpu_bound_shader.html'
  URL = 'file://../simple_canvas/gpu_bound_shader.html'


class HWAcceleratedCanvasToSWCanvas(SimpleCanvasPage):
  BASE_NAME = 'hw_accelerated_canvas_to_sw_canvas.html'
  URL = 'file://../simple_canvas/hw_accelerated_canvas_to_sw_canvas.html'


class PutAndCreateImageBitmapFromImageData(SimpleCanvasPage):
  BASE_NAME = 'put_and_create_imagebitmap_from_imagedata'
  URL = 'file://../simple_canvas/put_and_create_imageBitmap_from_imageData.html'


class PutImageData(SimpleCanvasPage):
  BASE_NAME = 'put_image_data.html'
  URL = 'file://../simple_canvas/put_image_data.html'


class StaticCanvasToHWAcceleratedCanvas(SimpleCanvasPage):
  BASE_NAME = 'static_canvas_to_hw_accelerated_canvas.html'
  URL = 'file://../simple_canvas/static_canvas_to_hw_accelerated_canvas.html'


class StaticWebglToHWAcceleratedCanvas(SimpleCanvasPage):
  BASE_NAME = 'static_webgl_to_hw_accelerated_canvas.html'
  URL = 'file://../simple_canvas/static_webgl_to_hw_accelerated_canvas.html'


class SheetsRender(SimpleCanvasPage):
  BASE_NAME = 'sheets_render.html'
  URL = 'file://../simple_canvas/sheets_render.html'


class ToBlobDuration(SimpleCanvasPage):
  BASE_NAME = 'toBlob_duration.html'
  URL = 'file://../simple_canvas/toBlob_duration.html'


class ToBlobDurationJpeg(SimpleCanvasPage):
  BASE_NAME = 'toBlob_duration_jpeg.html'
  URL = 'file://../simple_canvas/toBlob_duration_jpeg.html'


class ToBlobSmallCanvasInWorker(SimpleCanvasPage):
  BASE_NAME = 'toBlob_small_canvas_in_worker.html'
  URL = 'file://../simple_canvas/toBlob_small_canvas_in_worker.html'


class TransferFromImageBitmap(SimpleCanvasPage):
  BASE_NAME = 'transfer_from_imageBitmap.html'
  URL = 'file://../simple_canvas/transfer_from_imageBitmap.html'


class VideoToHWAcceleratedCanvas(SimpleCanvasPage):
  BASE_NAME = 'video_to_hw_accelerated_canvas'
  URL = 'file://../simple_canvas/video_to_hw_accelerated_canvas.html'


class VideoToSubTexture(SimpleCanvasPage):
  BASE_NAME = 'video_to_sub_texture'
  # pylint: disable=line-too-long
  URL = 'file://../simple_canvas/video_to_sub_texture.html?flip_y=false&premult=false'


class VideoToSubTextureFlipY(SimpleCanvasPage):
  BASE_NAME = 'video_to_sub_texture_flip_y'
  # pylint: disable=line-too-long
  URL = 'file://../simple_canvas/video_to_sub_texture.html?flip_y=true&premult=false'


class VideoToSubTexturePremultiply(SimpleCanvasPage):
  BASE_NAME = 'video_to_sub_texture_premultiply'
  # pylint: disable=line-too-long
  URL = 'file://../simple_canvas/video_to_sub_texture.html?flip_y=false&premult=true'


class VideoToSubTextureFlipAndPremultiply(SimpleCanvasPage):
  BASE_NAME = 'video_to_sub_texture_flip_and_premultiply'
  # pylint: disable=line-too-long
  URL = 'file://../simple_canvas/video_to_sub_texture.html?flip_y=true&premult=true'


class VideoToTexture(SimpleCanvasPage):
  BASE_NAME = 'video_to_texture'
  URL = 'file://../simple_canvas/video_to_texture.html'


class WebglToTexture(SimpleCanvasPage):
  BASE_NAME = 'webgl_to_texture'
  URL = 'file://../simple_canvas/webgl_to_texture.html'
