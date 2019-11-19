// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/gl/gl_surface_glx.h"

#include <utility>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/lazy_instance.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/single_thread_task_runner.h"
#include "base/synchronization/atomic_flag.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"
#include "ui/base/x/x11_display_util.h"
#include "ui/base/x/x11_util.h"
#include "ui/events/platform/platform_event_source.h"
#include "ui/gfx/x/x11.h"
#include "ui/gfx/x/x11_connection.h"
#include "ui/gfx/x/x11_types.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_presentation_helper.h"
#include "ui/gl/gl_visual_picker_glx.h"
#include "ui/gl/sync_control_vsync_provider.h"

namespace gl {

namespace {

bool g_glx_context_create = false;
bool g_glx_create_context_robustness_supported = false;
bool g_glx_create_context_profile_supported = false;
bool g_glx_create_context_profile_es2_supported = false;
bool g_glx_texture_from_pixmap_supported = false;
bool g_glx_oml_sync_control_supported = false;

// Track support of glXGetMscRateOML separately from GLX_OML_sync_control as a
// whole since on some platforms (e.g. crosbug.com/34585), glXGetMscRateOML
// always fails even though GLX_OML_sync_control is reported as being supported.
bool g_glx_get_msc_rate_oml_supported = false;
bool g_glx_ext_swap_control_supported = false;
bool g_glx_mesa_swap_control_supported = false;
bool g_glx_sgi_video_sync_supported = false;

// A 24-bit RGB visual and colormap to use when creating offscreen surfaces.
Visual* g_visual = nullptr;
int g_depth = CopyFromParent;
Colormap g_colormap = CopyFromParent;

GLXFBConfig GetConfigForWindow(Display* display,
                               gfx::AcceleratedWidget window) {
  DCHECK(window != 0);

  // This code path is expensive, but we only take it when
  // attempting to use GLX_ARB_create_context_robustness, in which
  // case we need a GLXFBConfig for the window in order to create a
  // context for it.
  //
  // TODO(kbr): this is not a reliable code path. On platforms which
  // support it, we should use glXChooseFBConfig in the browser
  // process to choose the FBConfig and from there the X Visual to
  // use when creating the window in the first place. Then we can
  // pass that FBConfig down rather than attempting to reconstitute
  // it.

  XWindowAttributes attributes;
  if (!XGetWindowAttributes(display, window, &attributes)) {
    LOG(ERROR) << "XGetWindowAttributes failed for window " << window << ".";
    return nullptr;
  }

  int visual_id = XVisualIDFromVisual(attributes.visual);

  int num_elements = 0;
  gfx::XScopedPtr<GLXFBConfig> configs(
      glXGetFBConfigs(display, DefaultScreen(display), &num_elements));
  if (!configs.get()) {
    LOG(ERROR) << "glXGetFBConfigs failed.";
    return nullptr;
  }
  if (!num_elements) {
    LOG(ERROR) << "glXGetFBConfigs returned 0 elements.";
    return nullptr;
  }
  bool found = false;
  int i;
  for (i = 0; i < num_elements; ++i) {
    int value;
    if (glXGetFBConfigAttrib(display, configs.get()[i], GLX_VISUAL_ID,
                             &value)) {
      LOG(ERROR) << "glXGetFBConfigAttrib failed.";
      return nullptr;
    }
    if (value == visual_id) {
      found = true;
      break;
    }
  }
  if (found) {
    return configs.get()[i];
  }
  return nullptr;
}

bool CreateDummyWindow(Display* display) {
  DCHECK(display);
  gfx::AcceleratedWidget parent_window =
      XRootWindow(display, DefaultScreen(display));
  gfx::AcceleratedWidget window =
      XCreateWindow(display, parent_window, 0, 0, 1, 1, 0, CopyFromParent,
                    InputOutput, CopyFromParent, 0, nullptr);
  if (!window) {
    LOG(ERROR) << "XCreateWindow failed";
    return false;
  }
  GLXFBConfig config = GetConfigForWindow(display, window);
  if (!config) {
    LOG(ERROR) << "Failed to get GLXConfig";
    XDestroyWindow(display, window);
    return false;
  }
  GLXWindow glx_window = glXCreateWindow(display, config, window, nullptr);
  if (!glx_window) {
    LOG(ERROR) << "glXCreateWindow failed";
    XDestroyWindow(display, window);
    return false;
  }
  glXDestroyWindow(display, glx_window);
  XDestroyWindow(display, window);
  return true;
}

class OMLSyncControlVSyncProvider : public SyncControlVSyncProvider {
 public:
  explicit OMLSyncControlVSyncProvider(GLXWindow glx_window)
      : SyncControlVSyncProvider(), glx_window_(glx_window) {}

  ~OMLSyncControlVSyncProvider() override {}

 protected:
  bool GetSyncValues(int64_t* system_time,
                     int64_t* media_stream_counter,
                     int64_t* swap_buffer_counter) override {
    return glXGetSyncValuesOML(gfx::GetXDisplay(), glx_window_, system_time,
                               media_stream_counter, swap_buffer_counter);
  }

  bool GetMscRate(int32_t* numerator, int32_t* denominator) override {
    if (!g_glx_get_msc_rate_oml_supported)
      return false;

    if (!glXGetMscRateOML(gfx::GetXDisplay(), glx_window_, numerator,
                          denominator)) {
      // Once glXGetMscRateOML has been found to fail, don't try again,
      // since each failing call may spew an error message.
      g_glx_get_msc_rate_oml_supported = false;
      return false;
    }

    return true;
  }

  bool IsHWClock() const override { return true; }

 private:
  GLXWindow glx_window_;

  DISALLOW_COPY_AND_ASSIGN(OMLSyncControlVSyncProvider);
};

class SGIVideoSyncThread : public base::Thread,
                           public base::RefCounted<SGIVideoSyncThread> {
 public:
  // Create a connection to the X server for use on g_video_sync_thread before
  // the sandbox starts.
  static bool InitializeBeforeSandboxStarts() {
    auto* display = GetDisplayImpl();
    if (!display)
      return false;

    if (!CreateDummyWindow(display)) {
      LOG(ERROR) << "CreateDummyWindow(display) failed";
      return false;
    }
    return true;
  }

  static scoped_refptr<SGIVideoSyncThread> Create() {
    if (!g_video_sync_thread) {
      g_video_sync_thread = new SGIVideoSyncThread();
      g_video_sync_thread->Start();
    }
    return g_video_sync_thread;
  }

  Display* GetDisplay() {
    DCHECK(task_runner()->BelongsToCurrentThread());
    return GetDisplayImpl();
  }

  void MaybeCreateGLXContext(GLXFBConfig config) {
    DCHECK(task_runner()->BelongsToCurrentThread());
    if (!context_) {
      context_ = glXCreateNewContext(GetDisplay(), config, GLX_RGBA_TYPE,
                                     nullptr, x11::True);
    }
    LOG_IF(ERROR, !context_) << "video_sync: glXCreateNewContext failed";
  }

  // Destroy |context_| on the thread where it is used.
  void CleanUp() override {
    DCHECK(task_runner()->BelongsToCurrentThread());
    if (context_)
      glXDestroyContext(GetDisplay(), context_);
  }

  GLXContext GetGLXContext() {
    DCHECK(task_runner()->BelongsToCurrentThread());
    return context_;
  }

 private:
  friend class base::RefCounted<SGIVideoSyncThread>;

  SGIVideoSyncThread() : base::Thread("SGI_video_sync") {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  }

  ~SGIVideoSyncThread() override {
    DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
    g_video_sync_thread = nullptr;
    Stop();
  }

  static Display* GetDisplayImpl() {
    static Display* display = gfx::OpenNewXDisplay();
    return display;
  }

  static SGIVideoSyncThread* g_video_sync_thread;
  GLXContext context_ = 0;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(SGIVideoSyncThread);
};

class SGIVideoSyncProviderThreadShim {
 public:
  SGIVideoSyncProviderThreadShim(gfx::AcceleratedWidget parent_window,
                                 SGIVideoSyncThread* vsync_thread)
      : parent_window_(parent_window),
        vsync_thread_(vsync_thread),
        window_(0),
        glx_window_(0),
        task_runner_(base::ThreadTaskRunnerHandle::Get()),
        cancel_vsync_flag_(),
        vsync_lock_() {
    // This ensures that creation of |parent_window_| has occured when this shim
    // is executing in the same thread as the call to create |parent_window_|.
    XSync(gfx::GetXDisplay(), x11::False);
  }

  ~SGIVideoSyncProviderThreadShim() {
    if (glx_window_)
      glXDestroyWindow(vsync_thread_->GetDisplay(), glx_window_);

    if (window_)
      XDestroyWindow(vsync_thread_->GetDisplay(), window_);
  }

  base::AtomicFlag* cancel_vsync_flag() { return &cancel_vsync_flag_; }

  base::Lock* vsync_lock() { return &vsync_lock_; }

  void Initialize() {
    DCHECK(vsync_thread_->GetDisplay());

    window_ = XCreateWindow(vsync_thread_->GetDisplay(), parent_window_, 0, 0,
                            1, 1, 0, CopyFromParent, InputOutput,
                            CopyFromParent, 0, nullptr);
    if (!window_) {
      LOG(ERROR) << "video_sync: XCreateWindow failed";
      return;
    }

    GLXFBConfig config =
        GetConfigForWindow(vsync_thread_->GetDisplay(), window_);
    if (!config) {
      LOG(ERROR) << "video_sync: Failed to get GLXConfig";
      return;
    }

    glx_window_ =
        glXCreateWindow(vsync_thread_->GetDisplay(), config, window_, nullptr);
    if (!glx_window_) {
      LOG(ERROR) << "video_sync: glXCreateWindow failed";
      return;
    }

    vsync_thread_->MaybeCreateGLXContext(config);
  }

  void GetVSyncParameters(gfx::VSyncProvider::UpdateVSyncCallback callback) {
    // Don't allow |window_| destruction while we're probing vsync.
    base::AutoLock locked(vsync_lock_);

    if (!vsync_thread_->GetGLXContext() || cancel_vsync_flag_.IsSet())
      return;

    base::TimeDelta interval = ui::GetPrimaryDisplayRefreshIntervalFromXrandr(
        vsync_thread_->GetDisplay());

    glXMakeContextCurrent(vsync_thread_->GetDisplay(), glx_window_, glx_window_,
                          vsync_thread_->GetGLXContext());

    unsigned int retrace_count = 0;
    if (glXWaitVideoSyncSGI(1, 0, &retrace_count) != 0)
      return;

    base::TimeTicks now = base::TimeTicks::Now();
    TRACE_EVENT_INSTANT0("gpu", "vblank", TRACE_EVENT_SCOPE_THREAD);

    glXMakeContextCurrent(vsync_thread_->GetDisplay(), 0, 0, nullptr);

    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), now, interval));
  }

 private:
  gfx::AcceleratedWidget parent_window_;
  SGIVideoSyncThread* vsync_thread_;
  gfx::AcceleratedWidget window_;
  GLXWindow glx_window_;

  scoped_refptr<base::SingleThreadTaskRunner> task_runner_;

  base::AtomicFlag cancel_vsync_flag_;
  base::Lock vsync_lock_;

  DISALLOW_COPY_AND_ASSIGN(SGIVideoSyncProviderThreadShim);
};

class SGIVideoSyncVSyncProvider
    : public gfx::VSyncProvider,
      public base::SupportsWeakPtr<SGIVideoSyncVSyncProvider> {
 public:
  explicit SGIVideoSyncVSyncProvider(gfx::AcceleratedWidget parent_window)
      : vsync_thread_(SGIVideoSyncThread::Create()),
        shim_(new SGIVideoSyncProviderThreadShim(parent_window,
                                                 vsync_thread_.get())),
        cancel_vsync_flag_(shim_->cancel_vsync_flag()),
        vsync_lock_(shim_->vsync_lock()) {
    vsync_thread_->task_runner()->PostTask(
        FROM_HERE, base::BindOnce(&SGIVideoSyncProviderThreadShim::Initialize,
                                  base::Unretained(shim_.get())));
  }

  ~SGIVideoSyncVSyncProvider() override {
    {
      base::AutoLock locked(*vsync_lock_);
      cancel_vsync_flag_->Set();
    }

    // Hand-off |shim_| to be deleted on the |vsync_thread_|.
    vsync_thread_->task_runner()->DeleteSoon(FROM_HERE, shim_.release());
  }

  void GetVSyncParameters(
      gfx::VSyncProvider::UpdateVSyncCallback callback) override {
    // Only one outstanding request per surface.
    if (!pending_callback_) {
      DCHECK(callback);
      pending_callback_ = std::move(callback);
      vsync_thread_->task_runner()->PostTask(
          FROM_HERE,
          base::BindOnce(&SGIVideoSyncProviderThreadShim::GetVSyncParameters,
                         base::Unretained(shim_.get()),
                         base::BindRepeating(
                             &SGIVideoSyncVSyncProvider::PendingCallbackRunner,
                             AsWeakPtr())));
    }
  }

  bool GetVSyncParametersIfAvailable(base::TimeTicks* timebase,
                                     base::TimeDelta* interval) override {
    return false;
  }

  bool SupportGetVSyncParametersIfAvailable() const override { return false; }
  bool IsHWClock() const override { return false; }

 private:
  void PendingCallbackRunner(const base::TimeTicks timebase,
                             const base::TimeDelta interval) {
    DCHECK(pending_callback_);
    std::move(pending_callback_).Run(timebase, interval);
  }

  scoped_refptr<SGIVideoSyncThread> vsync_thread_;

  // Thread shim through which the sync provider is accessed on |vsync_thread_|.
  std::unique_ptr<SGIVideoSyncProviderThreadShim> shim_;

  gfx::VSyncProvider::UpdateVSyncCallback pending_callback_;

  // Raw pointers to sync primitives owned by the shim_.
  // These will only be referenced before we post a task to destroy
  // the shim_, so they are safe to access.
  base::AtomicFlag* cancel_vsync_flag_;
  base::Lock* vsync_lock_;

  DISALLOW_COPY_AND_ASSIGN(SGIVideoSyncVSyncProvider);
};

SGIVideoSyncThread* SGIVideoSyncThread::g_video_sync_thread = nullptr;

}  // namespace

bool GLSurfaceGLX::initialized_ = false;

GLSurfaceGLX::GLSurfaceGLX() {}

bool GLSurfaceGLX::InitializeOneOff() {
  if (initialized_)
    return true;

  // http://crbug.com/245466
  setenv("force_s3tc_enable", "true", 1);

  // SGIVideoSyncProviderShim (if instantiated) will issue X commands on
  // it's own thread.
  gfx::InitializeThreadedX11();

  if (!gfx::GetXDisplay()) {
    LOG(ERROR) << "XOpenDisplay failed.";
    return false;
  }

  int major = 0, minor = 0;
  if (!glXQueryVersion(gfx::GetXDisplay(), &major, &minor)) {
    LOG(ERROR) << "glxQueryVersion failed";
    return false;
  }

  if (major == 1 && minor < 3) {
    LOG(ERROR) << "GLX 1.3 or later is required.";
    return false;
  }

  auto* visual_picker = gl::GLVisualPickerGLX::GetInstance();
  XVisualInfo visual_info = visual_picker->rgba_visual();
  if (!visual_info.visual)
    visual_info = visual_picker->system_visual();
  g_visual = visual_info.visual;
  g_depth = visual_info.depth;
  g_colormap =
      XCreateColormap(gfx::GetXDisplay(), DefaultRootWindow(gfx::GetXDisplay()),
                      visual_info.visual, AllocNone);
  // We create a dummy unmapped window for both the main Display and the video
  // sync Display so that the Nvidia driver can initialize itself before the
  // sandbox is set up.
  // Unfortunately some fds e.g. /dev/nvidia0 are cached per thread and because
  // we can't start threads before the sandbox is set up, these are accessed
  // through the broker process. See GpuProcessPolicy::InitGpuBrokerProcess.
  if (!CreateDummyWindow(gfx::GetXDisplay())) {
    LOG(ERROR) << "CreateDummyWindow(gfx::GetXDisplay()) failed";
    return false;
  }

  initialized_ = true;
  return true;
}

// static
bool GLSurfaceGLX::InitializeExtensionSettingsOneOff() {
  if (!initialized_)
    return false;

  g_driver_glx.InitializeExtensionBindings();

  g_glx_context_create = HasGLXExtension("GLX_ARB_create_context");
  g_glx_create_context_robustness_supported =
      HasGLXExtension("GLX_ARB_create_context_robustness");
  g_glx_create_context_profile_supported =
      HasGLXExtension("GLX_ARB_create_context_profile");
  g_glx_create_context_profile_es2_supported =
      HasGLXExtension("GLX_ARB_create_context_es2_profile");
  g_glx_texture_from_pixmap_supported =
      HasGLXExtension("GLX_EXT_texture_from_pixmap");
  g_glx_oml_sync_control_supported = HasGLXExtension("GLX_OML_sync_control");
  g_glx_get_msc_rate_oml_supported = g_glx_oml_sync_control_supported;
  g_glx_ext_swap_control_supported = HasGLXExtension("GLX_EXT_swap_control");
  g_glx_mesa_swap_control_supported = HasGLXExtension("GLX_MESA_swap_control");
  g_glx_sgi_video_sync_supported = HasGLXExtension("GLX_SGI_video_sync");

  if (!g_glx_get_msc_rate_oml_supported && g_glx_sgi_video_sync_supported) {
    if (!SGIVideoSyncThread::InitializeBeforeSandboxStarts())
      return false;
  }
  return true;
}

// static
void GLSurfaceGLX::ShutdownOneOff() {
  initialized_ = false;
  g_glx_context_create = false;
  g_glx_create_context_robustness_supported = false;
  g_glx_create_context_profile_supported = false;
  g_glx_create_context_profile_es2_supported = false;
  g_glx_texture_from_pixmap_supported = false;
  g_glx_oml_sync_control_supported = false;

  g_glx_get_msc_rate_oml_supported = false;
  g_glx_ext_swap_control_supported = false;
  g_glx_mesa_swap_control_supported = false;
  g_glx_sgi_video_sync_supported = false;

  g_visual = nullptr;
  g_depth = CopyFromParent;
  g_colormap = CopyFromParent;
}

// static
const char* GLSurfaceGLX::GetGLXExtensions() {
  return glXQueryExtensionsString(gfx::GetXDisplay(), 0);
}

// static
bool GLSurfaceGLX::HasGLXExtension(const char* name) {
  return ExtensionsContain(GetGLXExtensions(), name);
}

// static
bool GLSurfaceGLX::IsCreateContextSupported() {
  return g_glx_context_create;
}

// static
bool GLSurfaceGLX::IsCreateContextRobustnessSupported() {
  return g_glx_create_context_robustness_supported;
}

// static
bool GLSurfaceGLX::IsCreateContextProfileSupported() {
  return g_glx_create_context_profile_supported;
}

// static
bool GLSurfaceGLX::IsCreateContextES2ProfileSupported() {
  return g_glx_create_context_profile_es2_supported;
}

// static
bool GLSurfaceGLX::IsTextureFromPixmapSupported() {
  return g_glx_texture_from_pixmap_supported;
}

// static
bool GLSurfaceGLX::IsEXTSwapControlSupported() {
  return g_glx_ext_swap_control_supported;
}

// static
bool GLSurfaceGLX::IsMESASwapControlSupported() {
  return g_glx_mesa_swap_control_supported;
}

// static
bool GLSurfaceGLX::IsOMLSyncControlSupported() {
  return g_glx_oml_sync_control_supported;
}

void* GLSurfaceGLX::GetDisplay() {
  return gfx::GetXDisplay();
}

GLSurfaceGLX::~GLSurfaceGLX() {}

NativeViewGLSurfaceGLX::NativeViewGLSurfaceGLX(gfx::AcceleratedWidget window)
    : parent_window_(window),
      window_(0),
      glx_window_(0),
      config_(nullptr),
      has_swapped_buffers_(false) {}

bool NativeViewGLSurfaceGLX::Initialize(GLSurfaceFormat format) {
  XWindowAttributes attributes;
  if (!XGetWindowAttributes(gfx::GetXDisplay(), parent_window_, &attributes)) {
    LOG(ERROR) << "XGetWindowAttributes failed for window " << parent_window_
               << ".";
    return false;
  }
  size_ = gfx::Size(attributes.width, attributes.height);

  XSetWindowAttributes swa = {
      .background_pixmap = 0,
      .background_pixel = 0,  // ARGB(0,0,0,0) for compositing WM
      .border_pixel = 0,
      .bit_gravity = NorthWestGravity,
      .colormap = g_colormap,
  };
  auto value_mask = CWBackPixmap | CWBitGravity | CWColormap | CWBorderPixel;
  if (ui::IsCompositingManagerPresent() &&
      XVisualIDFromVisual(attributes.visual) == XVisualIDFromVisual(g_visual)) {
    // When parent and child are using the same visual, the back buffer will be
    // shared between parent and child. If WM compositing is enabled, we set
    // child's background pixel to ARGB(0,0,0,0), so ARGB(0,0,0,0) will be
    // filled to the shared buffer, when the child window is mapped. It can
    // avoid an annoying flash when the child window is mapped below.
    // If WM compositing is disabled, we don't set the background pixel, so
    // nothing will be draw when the child window is mapped.
    value_mask |= CWBackPixel;
  }

  window_ =
      XCreateWindow(gfx::GetXDisplay(), parent_window_, 0 /* x */, 0 /* y */,
                    size_.width(), size_.height(), 0 /* border_width */,
                    g_depth, InputOutput, g_visual, value_mask, &swa);
  if (!window_) {
    LOG(ERROR) << "XCreateWindow failed";
    return false;
  }
  XMapWindow(gfx::GetXDisplay(), window_);

  RegisterEvents();
  XFlush(gfx::GetXDisplay());

  GetConfig();
  if (!config_) {
    LOG(ERROR) << "Failed to get GLXConfig";
    return false;
  }
  glx_window_ = glXCreateWindow(gfx::GetXDisplay(), config_, window_, NULL);
  if (!glx_window_) {
    LOG(ERROR) << "glXCreateWindow failed";
    return false;
  }

  if (g_glx_oml_sync_control_supported) {
    vsync_provider_ =
        std::make_unique<OMLSyncControlVSyncProvider>(glx_window_);
    presentation_helper_ =
        std::make_unique<GLSurfacePresentationHelper>(vsync_provider_.get());
  } else if (g_glx_sgi_video_sync_supported) {
    vsync_provider_ =
        std::make_unique<SGIVideoSyncVSyncProvider>(parent_window_);
    presentation_helper_ =
        std::make_unique<GLSurfacePresentationHelper>(vsync_provider_.get());
  } else {
    // Assume a refresh rate of 59.9 Hz, which will cause us to skip
    // 1 frame every 10 seconds on a 60Hz monitor, but will prevent us
    // from blocking the GPU service due to back pressure. This would still
    // encounter backpressure on a <60Hz monitor, but hopefully that is
    // not common.
    const base::TimeTicks kDefaultTimebase;
    const base::TimeDelta kDefaultInterval =
        base::TimeDelta::FromSeconds(1) / 59.9;
    vsync_provider_ = std::make_unique<gfx::FixedVSyncProvider>(
        kDefaultTimebase, kDefaultInterval);
    presentation_helper_ = std::make_unique<GLSurfacePresentationHelper>(
        kDefaultTimebase, kDefaultInterval);
  }

  return true;
}

void NativeViewGLSurfaceGLX::Destroy() {
  presentation_helper_ = nullptr;
  vsync_provider_ = nullptr;
  if (glx_window_) {
    glXDestroyWindow(gfx::GetXDisplay(), glx_window_);
    glx_window_ = 0;
  }
  if (window_) {
    UnregisterEvents();
    XDestroyWindow(gfx::GetXDisplay(), window_);
    window_ = 0;
    XFlush(gfx::GetXDisplay());
  }
}

bool NativeViewGLSurfaceGLX::Resize(const gfx::Size& size,
                                    float scale_factor,
                                    ColorSpace color_space,
                                    bool has_alpha) {
  size_ = size;
  glXWaitGL();
  XResizeWindow(gfx::GetXDisplay(), window_, size.width(), size.height());
  glXWaitX();
  return true;
}

bool NativeViewGLSurfaceGLX::IsOffscreen() {
  return false;
}

gfx::SwapResult NativeViewGLSurfaceGLX::SwapBuffers(
    PresentationCallback callback) {
  TRACE_EVENT2("gpu", "NativeViewGLSurfaceGLX:RealSwapBuffers", "width",
               GetSize().width(), "height", GetSize().height());
  GLSurfacePresentationHelper::ScopedSwapBuffers scoped_swap_buffers(
      presentation_helper_.get(), std::move(callback));

  XDisplay* display = gfx::GetXDisplay();
  glXSwapBuffers(display, GetDrawableHandle());

  // We need to restore the background pixel that we set to WhitePixel on
  // views::DesktopWindowTreeHostX11::InitX11Window back to None for the
  // XWindow associated to this surface after the first SwapBuffers has
  // happened, to avoid showing a weird white background while resizing.
  if (!has_swapped_buffers_) {
    XSetWindowBackgroundPixmap(display, parent_window_, 0);
    has_swapped_buffers_ = true;
  }

  return scoped_swap_buffers.result();
}

gfx::Size NativeViewGLSurfaceGLX::GetSize() {
  return size_;
}

void* NativeViewGLSurfaceGLX::GetHandle() {
  return reinterpret_cast<void*>(GetDrawableHandle());
}

bool NativeViewGLSurfaceGLX::SupportsPostSubBuffer() {
  return g_driver_glx.ext.b_GLX_MESA_copy_sub_buffer;
}

void* NativeViewGLSurfaceGLX::GetConfig() {
  if (!config_)
    config_ = GetConfigForWindow(gfx::GetXDisplay(), window_);
  return config_;
}

GLSurfaceFormat NativeViewGLSurfaceGLX::GetFormat() {
  return GLSurfaceFormat();
}

unsigned long NativeViewGLSurfaceGLX::GetCompatibilityKey() {
  return XVisualIDFromVisual(g_visual);
}

gfx::SwapResult NativeViewGLSurfaceGLX::PostSubBuffer(
    int x,
    int y,
    int width,
    int height,
    PresentationCallback callback) {
  DCHECK(g_driver_glx.ext.b_GLX_MESA_copy_sub_buffer);

  GLSurfacePresentationHelper::ScopedSwapBuffers scoped_swap_buffers(
      presentation_helper_.get(), std::move(callback));
  glXCopySubBufferMESA(gfx::GetXDisplay(), GetDrawableHandle(), x, y, width,
                       height);
  return scoped_swap_buffers.result();
}

bool NativeViewGLSurfaceGLX::OnMakeCurrent(GLContext* context) {
  presentation_helper_->OnMakeCurrent(context, this);
  return GLSurfaceGLX::OnMakeCurrent(context);
}

gfx::VSyncProvider* NativeViewGLSurfaceGLX::GetVSyncProvider() {
  return vsync_provider_.get();
}

void NativeViewGLSurfaceGLX::SetVSyncEnabled(bool enabled) {
  DCHECK(GLContext::GetCurrent() && GLContext::GetCurrent()->IsCurrent(this));
  int interval = enabled ? 1 : 0;
  if (GLSurfaceGLX::IsEXTSwapControlSupported()) {
    glXSwapIntervalEXT(gfx::GetXDisplay(), glx_window_, interval);
  } else if (GLSurfaceGLX::IsMESASwapControlSupported()) {
    glXSwapIntervalMESA(interval);
  } else if (interval == 0) {
    LOG(WARNING)
        << "Could not disable vsync: driver does not support swap control";
  }
}

NativeViewGLSurfaceGLX::~NativeViewGLSurfaceGLX() {
  Destroy();
}

void NativeViewGLSurfaceGLX::ForwardExposeEvent(XEvent* event) {
  XEvent forwarded_event = *event;
  forwarded_event.xexpose.window = parent_window_;
  XSendEvent(gfx::GetXDisplay(), parent_window_, x11::False, ExposureMask,
             &forwarded_event);
  XFlush(gfx::GetXDisplay());
}

bool NativeViewGLSurfaceGLX::CanHandleEvent(XEvent* event) {
  return event->type == Expose &&
         event->xexpose.window == static_cast<Window>(window_);
}

GLXDrawable NativeViewGLSurfaceGLX::GetDrawableHandle() const {
  return glx_window_;
}

UnmappedNativeViewGLSurfaceGLX::UnmappedNativeViewGLSurfaceGLX(
    const gfx::Size& size)
    : size_(size), config_(nullptr), window_(0), glx_window_(0) {
  // Ensure that we don't create a window with zero size.
  if (size_.GetArea() == 0)
    size_.SetSize(1, 1);
}

bool UnmappedNativeViewGLSurfaceGLX::Initialize(GLSurfaceFormat format) {
  DCHECK(!window_);

  gfx::AcceleratedWidget parent_window = DefaultRootWindow(gfx::GetXDisplay());

  XSetWindowAttributes attrs;
  attrs.border_pixel = 0;
  attrs.colormap = g_colormap;
  window_ = XCreateWindow(
      gfx::GetXDisplay(), parent_window, 0, 0, size_.width(), size_.height(), 0,
      g_depth, InputOutput, g_visual, CWBorderPixel | CWColormap, &attrs);
  if (!window_) {
    LOG(ERROR) << "XCreateWindow failed";
    return false;
  }
  GetConfig();
  if (!config_) {
    LOG(ERROR) << "Failed to get GLXConfig";
    return false;
  }
  glx_window_ = glXCreateWindow(gfx::GetXDisplay(), config_, window_, NULL);
  if (!glx_window_) {
    LOG(ERROR) << "glXCreateWindow failed";
    return false;
  }
  return true;
}

void UnmappedNativeViewGLSurfaceGLX::Destroy() {
  config_ = nullptr;
  if (glx_window_) {
    glXDestroyWindow(gfx::GetXDisplay(), glx_window_);
    glx_window_ = 0;
  }
  if (window_) {
    XDestroyWindow(gfx::GetXDisplay(), window_);
    window_ = 0;
  }
}

bool UnmappedNativeViewGLSurfaceGLX::IsOffscreen() {
  return true;
}

gfx::SwapResult UnmappedNativeViewGLSurfaceGLX::SwapBuffers(
    PresentationCallback callback) {
  NOTREACHED() << "Attempted to call SwapBuffers on an unmapped window.";
  return gfx::SwapResult::SWAP_FAILED;
}

gfx::Size UnmappedNativeViewGLSurfaceGLX::GetSize() {
  return size_;
}

void* UnmappedNativeViewGLSurfaceGLX::GetHandle() {
  return reinterpret_cast<void*>(glx_window_);
}

void* UnmappedNativeViewGLSurfaceGLX::GetConfig() {
  if (!config_)
    config_ = GetConfigForWindow(gfx::GetXDisplay(), window_);
  return config_;
}

GLSurfaceFormat UnmappedNativeViewGLSurfaceGLX::GetFormat() {
  return GLSurfaceFormat();
}

unsigned long UnmappedNativeViewGLSurfaceGLX::GetCompatibilityKey() {
  return XVisualIDFromVisual(g_visual);
}

UnmappedNativeViewGLSurfaceGLX::~UnmappedNativeViewGLSurfaceGLX() {
  Destroy();
}

}  // namespace gl
