// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_image_dxgi.h"

#include <d3d11_1.h>

#include "base/debug/alias.h"
#include "third_party/khronos/EGL/egl.h"
#include "third_party/khronos/EGL/eglext.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_image.h"
#include "ui/gl/gl_surface_egl.h"

#ifndef EGL_ANGLE_d3d_texture_client_buffer
#define EGL_ANGLE_d3d_texture_client_buffer 1
#define EGL_D3D_TEXTURE_ANGLE 0x33A3
#endif /* EGL_ANGLE_d3d_texture_client_buffer */

namespace gl {

namespace {
// Keys used to acquire and release the keyed mutex.  Will need to be kept in
// sync with any other code that reads from or draws to the same DXGI handle.
const static UINT64 KEY_BIND = 0;
const static UINT64 KEY_RELEASE = 1;

bool SupportedBindFormat(gfx::BufferFormat format) {
  switch (format) {
    case gfx::BufferFormat::RGBA_8888:
    case gfx::BufferFormat::RGBX_8888:
      return true;
    default:
      return false;
  };
}

bool HasAlpha(gfx::BufferFormat format) {
  DCHECK(SupportedBindFormat(format));
  switch (format) {
    case gfx::BufferFormat::RGBA_8888:
      return true;
    case gfx::BufferFormat::RGBX_8888:
      return false;
    default:
      NOTREACHED();
      return false;
  };
}

EGLConfig ChooseCompatibleConfig(gfx::BufferFormat format) {
  DCHECK(SupportedBindFormat(format));

  const EGLint buffer_bind_to_texture =
      HasAlpha(format) ? EGL_BIND_TO_TEXTURE_RGBA : EGL_BIND_TO_TEXTURE_RGB;
  const EGLint buffer_size = HasAlpha(format) ? 32 : 24;
  EGLint const attrib_list[] = {EGL_RED_SIZE,
                                8,
                                EGL_GREEN_SIZE,
                                8,
                                EGL_BLUE_SIZE,
                                8,
                                EGL_SURFACE_TYPE,
                                EGL_PBUFFER_BIT,
                                buffer_bind_to_texture,
                                EGL_TRUE,
                                EGL_BUFFER_SIZE,
                                buffer_size,
                                EGL_NONE};

  EGLint num_config;
  EGLDisplay display = gl::GLSurfaceEGL::GetHardwareDisplay();
  EGLBoolean result =
      eglChooseConfig(display, attrib_list, nullptr, 0, &num_config);
  if (result != EGL_TRUE)
    return nullptr;
  std::vector<EGLConfig> all_configs(num_config);
  result = eglChooseConfig(gl::GLSurfaceEGL::GetHardwareDisplay(), attrib_list,
                           all_configs.data(), num_config, &num_config);
  if (result != EGL_TRUE)
    return nullptr;
  for (EGLConfig config : all_configs) {
    EGLint bits;
    if (!eglGetConfigAttrib(display, config, EGL_RED_SIZE, &bits) ||
        bits != 8) {
      continue;
    }

    if (!eglGetConfigAttrib(display, config, EGL_BLUE_SIZE, &bits) ||
        bits != 8) {
      continue;
    }

    if (!eglGetConfigAttrib(display, config, EGL_GREEN_SIZE, &bits) ||
        bits != 8) {
      continue;
    }

    if (HasAlpha(format) &&
        (!eglGetConfigAttrib(display, config, EGL_ALPHA_SIZE, &bits) ||
         bits != 8)) {
      continue;
    }

    return config;
  }
  return nullptr;
}

EGLSurface CreatePbuffer(const Microsoft::WRL::ComPtr<ID3D11Texture2D>& texture,
                         gfx::BufferFormat format,
                         EGLConfig config,
                         unsigned target) {
  DCHECK(SupportedBindFormat(format));

  D3D11_TEXTURE2D_DESC desc;
  texture->GetDesc(&desc);
  EGLint width = desc.Width;
  EGLint height = desc.Height;

  EGLint pBufferAttributes[] = {
      EGL_WIDTH,
      width,
      EGL_HEIGHT,
      height,
      EGL_TEXTURE_TARGET,
      EGL_TEXTURE_2D,
      EGL_TEXTURE_FORMAT,
      HasAlpha(format) ? EGL_TEXTURE_RGBA : EGL_TEXTURE_RGB,
      EGL_NONE};

  return eglCreatePbufferFromClientBuffer(
      gl::GLSurfaceEGL::GetHardwareDisplay(), EGL_D3D_TEXTURE_ANGLE,
      texture.Get(), config, pBufferAttributes);
}

}  // namespace

GLImageDXGI::GLImageDXGI(const gfx::Size& size, EGLStreamKHR stream)
    : size_(size), stream_(stream) {}

// static
GLImageDXGI* GLImageDXGI::FromGLImage(GLImage* image) {
  if (!image || image->GetType() != Type::DXGI_IMAGE)
    return nullptr;
  return static_cast<GLImageDXGI*>(image);
}

GLImageDXGI::BindOrCopy GLImageDXGI::ShouldBindOrCopy() {
  return BIND;
}

bool GLImageDXGI::BindTexImage(unsigned target) {
  if (!handle_.Get())
    return true;

  DCHECK(texture_);
  DCHECK(keyed_mutex_);
  if (!SupportedBindFormat(buffer_format_))
    return false;

  // Lazy-initialize surface_, as it is only used for binding.
  if (surface_ == EGL_NO_SURFACE) {
    EGLConfig config = ChooseCompatibleConfig(buffer_format_);
    if (!config)
      return false;
    surface_ = CreatePbuffer(texture_, buffer_format_, config, target);
    if (surface_ == EGL_NO_SURFACE)
      return false;
  }

  // We don't wait, just return immediately.
  HRESULT hrWait = keyed_mutex_->AcquireSync(KEY_BIND, 0);

  if (hrWait == WAIT_TIMEOUT || hrWait == WAIT_ABANDONED || FAILED(hrWait)) {
    NOTREACHED();
    return false;
  }

  return eglBindTexImage(gl::GLSurfaceEGL::GetHardwareDisplay(), surface_,
                         EGL_BACK_BUFFER) == EGL_TRUE;
}

bool GLImageDXGI::CopyTexImage(unsigned target) {
  NOTREACHED();
  return false;
}

bool GLImageDXGI::CopyTexSubImage(unsigned target,
                                  const gfx::Point& offset,
                                  const gfx::Rect& rect) {
  return false;
}

void GLImageDXGI::Flush() {}

unsigned GLImageDXGI::GetInternalFormat() {
  if (!handle_.Get())
    return GL_BGRA_EXT;
  else
    return HasAlpha(buffer_format_) ? GL_RGBA : GL_RGB;
}

unsigned GLImageDXGI::GetDataType() {
  return GL_UNSIGNED_BYTE;
}

gfx::Size GLImageDXGI::GetSize() {
  return size_;
}

GLImage::Type GLImageDXGI::GetType() const {
  return Type::DXGI_IMAGE;
}

void GLImageDXGI::OnMemoryDump(base::trace_event::ProcessMemoryDump* pmd,
                               uint64_t process_tracing_id,
                               const std::string& dump_name) {}

void GLImageDXGI::ReleaseTexImage(unsigned target) {
  if (!handle_.Get())
    return;

  DCHECK(texture_);
  DCHECK(keyed_mutex_);

  keyed_mutex_->ReleaseSync(KEY_RELEASE);

  eglReleaseTexImage(gl::GLSurfaceEGL::GetHardwareDisplay(), surface_,
                     EGL_BACK_BUFFER);
}

bool GLImageDXGI::ScheduleOverlayPlane(
    gfx::AcceleratedWidget widget,
    int z_order,
    gfx::OverlayTransform transform,
    const gfx::Rect& bounds_rect,
    const gfx::RectF& crop_rect,
    bool enable_blend,
    std::unique_ptr<gfx::GpuFence> gpu_fence) {
  return false;
}

bool GLImageDXGI::InitializeHandle(base::win::ScopedHandle handle,
                                   uint32_t level,
                                   gfx::BufferFormat format) {
  level_ = level;
  buffer_format_ = format;
  Microsoft::WRL::ComPtr<ID3D11Device> d3d11_device =
      QueryD3D11DeviceObjectFromANGLE();
  if (!d3d11_device)
    return false;

  Microsoft::WRL::ComPtr<ID3D11Device1> d3d11_device1;
  if (FAILED(d3d11_device.CopyTo(d3d11_device1.GetAddressOf())))
    return false;

  if (FAILED(d3d11_device1->OpenSharedResource1(
          handle.Get(), IID_PPV_ARGS(texture_.GetAddressOf())))) {
    return false;
  }
  D3D11_TEXTURE2D_DESC desc;
  texture_->GetDesc(&desc);
  if (desc.ArraySize <= level_)
    return false;
  if (FAILED(texture_.CopyTo(keyed_mutex_.GetAddressOf())))
    return false;

  handle_ = std::move(handle);
  return true;
}

void GLImageDXGI::SetTexture(
    const Microsoft::WRL::ComPtr<ID3D11Texture2D>& texture,
    size_t level) {
  texture_ = texture;
  level_ = level;
}

GLImageDXGI::~GLImageDXGI() {
  if (handle_.Get()) {
    if (surface_ != EGL_NO_SURFACE) {
      eglDestroySurface(gl::GLSurfaceEGL::GetHardwareDisplay(), surface_);
    }
  } else if (stream_) {
    EGLDisplay egl_display = gl::GLSurfaceEGL::GetHardwareDisplay();
    eglDestroyStreamKHR(egl_display, stream_);
  }
}

CopyingGLImageDXGI::CopyingGLImageDXGI(
    const Microsoft::WRL::ComPtr<ID3D11Device>& d3d11_device,
    const gfx::Size& size,
    EGLStreamKHR stream)
    : GLImageDXGI(size, stream), d3d11_device_(d3d11_device) {}

bool CopyingGLImageDXGI::Initialize() {
  D3D11_TEXTURE2D_DESC desc;
  desc.Width = size_.width();
  desc.Height = size_.height();
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_NV12;
  desc.SampleDesc.Count = 1;
  desc.SampleDesc.Quality = 0;
  desc.Usage = D3D11_USAGE_DEFAULT;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
  desc.CPUAccessFlags = 0;
  desc.MiscFlags = 0;

  HRESULT hr = d3d11_device_->CreateTexture2D(
      &desc, nullptr, decoder_copy_texture_.GetAddressOf());
  if (FAILED(hr)) {
    DLOG(ERROR) << "CreateTexture2D failed: " << std::hex << hr;
    return false;
  }
  EGLDisplay egl_display = gl::GLSurfaceEGL::GetHardwareDisplay();

  EGLAttrib frame_attributes[] = {
      EGL_D3D_TEXTURE_SUBRESOURCE_ID_ANGLE, 0, EGL_NONE,
  };

  EGLBoolean result = eglStreamPostD3DTextureANGLE(
      egl_display, stream_, static_cast<void*>(decoder_copy_texture_.Get()),
      frame_attributes);
  if (!result) {
    DLOG(ERROR) << "eglStreamPostD3DTextureANGLE failed";
    return false;
  }
  result = eglStreamConsumerAcquireKHR(egl_display, stream_);
  if (!result) {
    DLOG(ERROR) << "eglStreamConsumerAcquireKHR failed";
    return false;
  }

  d3d11_device_.CopyTo(video_device_.GetAddressOf());
  Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
  d3d11_device_->GetImmediateContext(context.GetAddressOf());
  context.CopyTo(video_context_.GetAddressOf());

#if DCHECK_IS_ON()
  Microsoft::WRL::ComPtr<ID3D10Multithread> multithread;
  d3d11_device_.CopyTo(multithread.GetAddressOf());
  DCHECK(multithread->GetMultithreadProtected());
#endif  // DCHECK_IS_ON()

  return true;
}

bool CopyingGLImageDXGI::InitializeVideoProcessor(
    const Microsoft::WRL::ComPtr<ID3D11VideoProcessor>& video_processor,
    const Microsoft::WRL::ComPtr<ID3D11VideoProcessorEnumerator>& enumerator) {
  output_view_.Reset();

  Microsoft::WRL::ComPtr<ID3D11Device> processor_device;
  video_processor->GetDevice(processor_device.GetAddressOf());
  DCHECK_EQ(d3d11_device_.Get(), processor_device.Get());

  d3d11_processor_ = video_processor;
  enumerator_ = enumerator;
  D3D11_VIDEO_PROCESSOR_OUTPUT_VIEW_DESC output_view_desc = {
      D3D11_VPOV_DIMENSION_TEXTURE2D};
  output_view_desc.Texture2D.MipSlice = 0;
  Microsoft::WRL::ComPtr<ID3D11VideoProcessorOutputView> output_view;
  HRESULT hr = video_device_->CreateVideoProcessorOutputView(
      decoder_copy_texture_.Get(), enumerator_.Get(), &output_view_desc,
      output_view_.GetAddressOf());
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to get output view";
    return false;
  }
  return true;
}

void CopyingGLImageDXGI::UnbindFromTexture() {
  copied_ = false;
}

bool CopyingGLImageDXGI::BindTexImage(unsigned target) {
  if (copied_)
    return true;

  DCHECK(video_device_);
  Microsoft::WRL::ComPtr<ID3D11Device> texture_device;
  texture_->GetDevice(texture_device.GetAddressOf());
  DCHECK_EQ(d3d11_device_.Get(), texture_device.Get());

  D3D11_VIDEO_PROCESSOR_INPUT_VIEW_DESC input_view_desc = {0};
  input_view_desc.ViewDimension = D3D11_VPIV_DIMENSION_TEXTURE2D;
  input_view_desc.Texture2D.ArraySlice = (UINT)level_;
  input_view_desc.Texture2D.MipSlice = 0;
  Microsoft::WRL::ComPtr<ID3D11VideoProcessorInputView> input_view;
  HRESULT hr = video_device_->CreateVideoProcessorInputView(
      texture_.Get(), enumerator_.Get(), &input_view_desc,
      input_view.GetAddressOf());
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to create video processor input view.";
    return false;
  }

  D3D11_VIDEO_PROCESSOR_STREAM streams = {0};
  streams.Enable = TRUE;
  streams.pInputSurface = input_view.Get();

  hr = video_context_->VideoProcessorBlt(d3d11_processor_.Get(),
                                         output_view_.Get(), 0, 1, &streams);
  if (FAILED(hr)) {
    DLOG(ERROR) << "Failed to process video";
    return false;
  }
  copied_ = true;
  return true;
}

CopyingGLImageDXGI::~CopyingGLImageDXGI() {}

}  // namespace gl
