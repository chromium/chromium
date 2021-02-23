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
#include "base/no_destructor.h"
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
#include "ui/gfx/native_widget_types.h"
#include "ui/gfx/x/xproto_util.h"
#include "ui/gl/gl_bindings.h"
#include "ui/gl/gl_context.h"
#include "ui/gl/gl_implementation.h"
#include "ui/gl/gl_surface_presentation_helper.h"
#include "ui/gl/gl_visual_picker_glx.h"
#include "ui/gl/glx_util.h"
#include "ui/gl/sync_control_vsync_provider.h"

namespace gl {

namespace {

bool g_glx_context_create = false;
bool g_glx_create_context_robustness_supported = false;
bool g_glx_robustness_video_memory_purge_supported = false;
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
x11::VisualId g_visual{};
int g_depth = static_cast<int>(x11::WindowClass::CopyFromParent);
x11::ColorMap g_colormap{};

bool CreateDummyWindow(x11::Connection* conn) {
  DCHECK(conn);
  auto parent_window = conn->default_root();
  auto window = conn->GenerateId<x11::Window>();
  auto create_window = conn->CreateWindow(x11::CreateWindowRequest{
      .wid = window,
      .parent = parent_window,
      .width = 1,
      .height = 1,
      .c_class = x11::WindowClass::InputOutput,
  });
  if (create_window.Sync().error) {
    LOG(ERROR) << "Failed to create window";
    return false;
  }
  GLXFBConfig config = GetFbConfigForWindow(conn, window);
  if (!config) {
    LOG(ERROR) << "Failed to get GLXConfig";
    conn->DestroyWindow({window});
    return false;
  }

  GLXWindow glx_window = glXCreateWindow(
      conn->GetXlibDisplay(), config, static_cast<uint32_t>(window), nullptr);
  if (!glx_window) {
    LOG(ERROR) << "glXCreateWindow failed";
    conn->DestroyWindow({window});
    return false;
  }
  glXDestroyWindow(conn->GetXlibDisplay(x11::XlibDisplayType::kFlushing),
                   glx_window);
  conn->DestroyWindow({window});
  return true;
}

class OMLSyncControlVSyncProvider : public SyncControlVSyncProvider {
 public:
  explicit OMLSyncControlVSyncProvider(GLXWindow glx_window)
      : SyncControlVSyncProvider(), glx_window_(glx_window) {}

  ~OMLSyncControlVSyncProvider() override = default;

 protected:
  bool GetSyncValues(int64_t* system_time,
                     int64_t* media_stream_counter,
                     int64_t* swap_buffer_counter) override {
    x11::Connection::Get()->Flush();
    return glXGetSyncValuesOML(x11::Connection::Get()->GetXlibDisplay(),
                               glx_window_, system_time, media_stream_counter,
                               swap_buffer_counter);
  }

  bool GetMscRate(int32_t* numerator, int32_t* denominator) override {
    if (!g_glx_get_msc_rate_oml_supported)
      return false;

    if (!glXGetMscRateOML(x11::Connection::Get()->GetXlibDisplay(), glx_window_,
                          numerator, denominator)) {
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
    auto* connection = GetConnectionImpl();
    if (!connection || !connection->Ready())
      return false;

    if (!CreateDummyWindow(connection)) {
      LOG(ERROR) << "CreateDummyWindow(display) failed";
      return false;
    }
    connection->DetachFromSequence();
    return true;
  }

  static scoped_refptr<SGIVideoSyncThread> Create() {
    if (!g_video_sync_thread) {
      g_video_sync_thread = new SGIVideoSyncThread();
      g_video_sync_thread->Start();
    }
    return g_video_sync_thread;
  }

  x11::Connection* GetConnection() {
    DCHECK(task_runner()->BelongsToCurrentThread());
    return GetConnectionImpl();
  }

  void MaybeCreateGLXContext(GLXFBConfig config) {
    DCHECK(task_runner()->BelongsToCurrentThread());
    if (!context_) {
      context_ = glXCreateNewContext(
          GetConnection()->GetXlibDisplay(x11::XlibDisplayType::kSyncing),
          config, GLX_RGBA_TYPE, nullptr, true);
    }
    LOG_IF(ERROR, !context_) << "video_sync: glXCreateNewContext failed";
  }

  // Destroy |context_| on the thread where it is used.
  void CleanUp() override {
    DCHECK(task_runner()->BelongsToCurrentThread());
    if (context_)
      glXDestroyContext(
          GetConnection()->GetXlibDisplay(x11::XlibDisplayType::kFlushing),
          context_);
    // Release the connection from this thread's sequence so that a new
    // SGIVideoSyncThread can reuse the connection.  The connection must be
    // reused since it can only be created before sandbox initialization.
    GetConnection()->DetachFromSequence();
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

  static x11::Connection* GetConnectionImpl() {
    if (!g_connection)
      g_connection = x11::Connection::Get()->Clone().release();
    return g_connection;
  }

  static SGIVideoSyncThread* g_video_sync_thread;
  static x11::Connection* g_connection;
  GLXContext context_ = nullptr;

  THREAD_CHECKER(thread_checker_);

  DISALLOW_COPY_AND_ASSIGN(SGIVideoSyncThread);
};

class SGIVideoSyncProviderThreadShim {
 public:
  SGIVideoSyncProviderThreadShim(gfx::AcceleratedWidget parent_window,
                                 SGIVideoSyncThread* vsync_thread)
      : parent_window_(parent_window),
        vsync_thread_(vsync_thread),
        glx_window_(0),
        task_runner_(base::ThreadTaskRunnerHandle::Get()),
        cancel_vsync_flag_(),
        vsync_lock_() {
    // This ensures that creation of |parent_window_| has occured when this shim
    // is executing in the same thread as the call to create |parent_window_|.
    x11::Connection::Get()->Sync();
  }

  ~SGIVideoSyncProviderThreadShim() {
    auto* connection = vsync_thread_->GetConnection();
    if (glx_window_) {
      glXDestroyWindow(
          connection->GetXlibDisplay(x11::XlibDisplayType::kFlushing),
          glx_window_);
    }

    if (window_ != x11::Window::None)
      connection->DestroyWindow({window_});
  }

  base::AtomicFlag* cancel_vsync_flag() { return &cancel_vsync_flag_; }

  base::Lock* vsync_lock() { return &vsync_lock_; }

  void Initialize() {
    auto* connection = vsync_thread_->GetConnection();
    DCHECK(connection);

    auto window = connection->GenerateId<x11::Window>();
    auto req = connection->CreateWindow(x11::CreateWindowRequest{
        .wid = window,
        .parent = static_cast<x11::Window>(parent_window_),
        .width = 1,
        .height = 1,
        .c_class = x11::WindowClass::InputOutput,
    });
    if (req.Sync().error) {
      LOG(ERROR) << "video_sync: XCreateWindow failed";
      return;
    }
    window_ = window;

    GLXFBConfig config = GetFbConfigForWindow(connection, window_);
    if (!config) {
      LOG(ERROR) << "video_sync: Failed to get GLXConfig";
      return;
    }

    glx_window_ = glXCreateWindow(
        connection->GetXlibDisplay(x11::XlibDisplayType::kSyncing), config,
        static_cast<uint32_t>(window_), nullptr);
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

    base::TimeDelta interval = ui::GetPrimaryDisplayRefreshIntervalFromXrandr();

    auto* connection = vsync_thread_->GetConnection();
    glXMakeContextCurrent(
        connection->GetXlibDisplay(x11::XlibDisplayType::kFlushing),
        glx_window_, glx_window_, vsync_thread_->GetGLXContext());

    unsigned int retrace_count = 0;
    if (glXWaitVideoSyncSGI(1, 0, &retrace_count) != 0)
      return;

    base::TimeTicks now = base::TimeTicks::Now();
    TRACE_EVENT_INSTANT0("gpu", "vblank", TRACE_EVENT_SCOPE_THREAD);

    glXMakeContextCurrent(
        connection->GetXlibDisplay(x11::XlibDisplayType::kFlushing), 0, 0,
        nullptr);

    task_runner_->PostTask(FROM_HERE,
                           base::BindOnce(std::move(callback), now, interval));
  }

 private:
  gfx::AcceleratedWidget parent_window_;
  SGIVideoSyncThread* vsync_thread_;
  x11::Window window_ = x11::Window::None;
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
x11::Connection* SGIVideoSyncThread::g_connection = nullptr;

}  // namespace

bool GLSurfaceGLX::initialized_ = false;

GLSurfaceGLX::GLSurfaceGLX() = default;

bool GLSurfaceGLX::InitializeOneOff() {
  if (initialized_)
    return true;

  // http://crbug.com/245466
  setenv("force_s3tc_enable", "true", 1);

  if (!x11::Connection::Get()->Ready()) {
    LOG(ERROR) << "Could not open X11 connection.";
    return false;
  }

  int major = 0, minor = 0;
  if (!glXQueryVersion(x11::Connection::Get()->GetXlibDisplay(), &major,
                       &minor)) {
    LOG(ERROR) << "glxQueryVersion failed";
    return false;
  }

  if (major == 1 && minor < 3) {
    LOG(ERROR) << "GLX 1.3 or later is required.";
    return false;
  }

  auto* visual_picker = gl::GLVisualPickerGLX::GetInstance();
  auto visual_id = visual_picker->rgba_visual();
  if (visual_id == x11::VisualId{})
    visual_id = visual_picker->system_visual();
  g_visual = visual_id;
  auto* connection = x11::Connection::Get();
  g_depth = connection->GetVisualInfoFromId(visual_id)->format->depth;
  g_colormap = connection->GenerateId<x11::ColorMap>();
  connection->CreateColormap({x11::ColormapAlloc::None, g_colormap,
                              connection->default_root(), g_visual});
  // We create a dummy unmapped window for both the main Display and the video
  // sync Display so that the Nvidia driver can initialize itself before the
  // sandbox is set up.
  // Unfortunately some fds e.g. /dev/nvidia0 are cached per thread and because
  // we can't start threads before the sandbox is set up, these are accessed
  // through the broker process. See GpuProcessPolicy::InitGpuBrokerProcess.
  if (!CreateDummyWindow(x11::Connection::Get())) {
    LOG(ERROR) << "CreateDummyWindow() failed";
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
  g_glx_robustness_video_memory_purge_supported =
      HasGLXExtension("GLX_NV_robustness_video_memory_purge");
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
  g_glx_robustness_video_memory_purge_supported = false;
  g_glx_create_context_profile_supported = false;
  g_glx_create_context_profile_es2_supported = false;
  g_glx_texture_from_pixmap_supported = false;
  g_glx_oml_sync_control_supported = false;

  g_glx_get_msc_rate_oml_supported = false;
  g_glx_ext_swap_control_supported = false;
  g_glx_mesa_swap_control_supported = false;
  g_glx_sgi_video_sync_supported = false;

  g_visual = {};
  g_depth = static_cast<int>(x11::WindowClass::CopyFromParent);
  g_colormap = {};
}

// static
std::string GLSurfaceGLX::QueryGLXExtensions() {
  auto* connection = x11::Connection::Get();
  const int screen = connection ? connection->DefaultScreenId() : 0;
  const char* extensions =
      glXQueryExtensionsString(connection->GetXlibDisplay(), screen);
  if (extensions)
    return std::string(extensions);
  return "";
}

// static
const char* GLSurfaceGLX::GetGLXExtensions() {
  static base::NoDestructor<std::string> glx_extensions("");
  if (glx_extensions->empty()) {
    *glx_extensions = QueryGLXExtensions();
  }
  return glx_extensions->c_str();
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
bool GLSurfaceGLX::IsRobustnessVideoMemoryPurgeSupported() {
  return g_glx_robustness_video_memory_purge_supported;
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
  return x11::Connection::Get()->GetXlibDisplay();
}

GLSurfaceGLX::~GLSurfaceGLX() = default;

NativeViewGLSurfaceGLX::NativeViewGLSurfaceGLX(gfx::AcceleratedWidget window)
    : parent_window_(window),
      window_(x11::Window::None),
      glx_window_(),
      config_(nullptr),
      has_swapped_buffers_(false) {}

bool NativeViewGLSurfaceGLX::Initialize(GLSurfaceFormat format) {
  auto* conn = x11::Connection::Get();

  auto parent = static_cast<x11::Window>(parent_window_);
  auto attributes_req = conn->GetWindowAttributes({parent});
  auto geometry_req = conn->GetGeometry(parent);
  conn->Flush();
  auto attributes = attributes_req.Sync();
  auto geometry = geometry_req.Sync();

  if (!attributes || !geometry) {
    LOG(ERROR) << "GetGeometry/GetWindowAttribues failed for window "
               << static_cast<uint32_t>(parent_window_) << ".";
    return false;
  }
  size_ = gfx::Size(geometry->width, geometry->height);

  window_ = conn->GenerateId<x11::Window>();
  x11::CreateWindowRequest req{
      .depth = g_depth,
      .wid = window_,
      .parent = static_cast<x11::Window>(parent_window_),
      .width = size_.width(),
      .height = size_.height(),
      .c_class = x11::WindowClass::InputOutput,
      .visual = g_visual,
      .background_pixmap = x11::Pixmap::None,
      .border_pixel = 0,
      .bit_gravity = x11::Gravity::NorthWest,
      .colormap = g_colormap,
  };
  if (ui::IsCompositingManagerPresent() && attributes->visual == g_visual) {
    // When parent and child are using the same visual, the back buffer will be
    // shared between parent and child. If WM compositing is enabled, we set
    // child's background pixel to ARGB(0,0,0,0), so ARGB(0,0,0,0) will be
    // filled to the shared buffer, when the child window is mapped. It can
    // avoid an annoying flash when the child window is mapped below.
    // If WM compositing is disabled, we don't set the background pixel, so
    // nothing will be draw when the child window is mapped.
    req.background_pixel = 0;  // ARGB(0,0,0,0) for compositing WM
  }
  conn->CreateWindow(req);
  conn->MapWindow({window_});

  RegisterEvents();
  conn->Sync();

  GetConfig();
  if (!config_) {
    LOG(ERROR) << "Failed to get GLXConfig";
    return false;
  }
  glx_window_ = static_cast<x11::Glx::Window>(
      glXCreateWindow(conn->GetXlibDisplay(x11::XlibDisplayType::kSyncing),
                      config_, static_cast<uint32_t>(window_), nullptr));
  if (!GetDrawableHandle()) {
    LOG(ERROR) << "glXCreateWindow failed";
    return false;
  }

  if (g_glx_oml_sync_control_supported) {
    vsync_provider_ = std::make_unique<OMLSyncControlVSyncProvider>(
        static_cast<GLXWindow>(glx_window_));
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
  if (GetDrawableHandle()) {
    glXDestroyWindow(
        x11::Connection::Get()->GetXlibDisplay(x11::XlibDisplayType::kFlushing),
        GetDrawableHandle());
    glx_window_ = {};
  }
  if (window_ != x11::Window::None) {
    UnregisterEvents();
    x11::Connection::Get()->DestroyWindow({window_});
    window_ = x11::Window::None;
    x11::Connection::Get()->Flush();
  }
}

bool NativeViewGLSurfaceGLX::Resize(const gfx::Size& size,
                                    float scale_factor,
                                    const gfx::ColorSpace& color_space,
                                    bool has_alpha) {
  size_ = size;
  glXWaitGL();
  x11::Connection::Get()->ConfigureWindow(
      {.window = window_, .width = size.width(), .height = size.height()});
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

  auto* connection = x11::Connection::Get();
  glXSwapBuffers(connection->GetXlibDisplay(x11::XlibDisplayType::kFlushing),
                 GetDrawableHandle());

  // We need to restore the background pixel that we set to WhitePixel on
  // views::DesktopWindowTreeHostX11::InitX11Window back to None for the
  // XWindow associated to this surface after the first SwapBuffers has
  // happened, to avoid showing a weird white background while resizing.
  if (!has_swapped_buffers_) {
    connection->ChangeWindowAttributes({
        .window = static_cast<x11::Window>(parent_window_),
        .background_pixmap = x11::Pixmap::None,
    });
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
    config_ = GetFbConfigForWindow(x11::Connection::Get(), window_);
  return config_;
}

GLSurfaceFormat NativeViewGLSurfaceGLX::GetFormat() {
  return GLSurfaceFormat();
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
  glXCopySubBufferMESA(
      x11::Connection::Get()->GetXlibDisplay(x11::XlibDisplayType::kFlushing),
      GetDrawableHandle(), x, y, width, height);
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
    glXSwapIntervalEXT(
        x11::Connection::Get()->GetXlibDisplay(x11::XlibDisplayType::kFlushing),
        GetDrawableHandle(), interval);
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

void NativeViewGLSurfaceGLX::ForwardExposeEvent(const x11::Event& event) {
  auto forwarded_event = *event.As<x11::ExposeEvent>();
  auto window = static_cast<x11::Window>(parent_window_);
  forwarded_event.window = window;
  x11::SendEvent(forwarded_event, window, x11::EventMask::Exposure);
  x11::Connection::Get()->Flush();
}

bool NativeViewGLSurfaceGLX::CanHandleEvent(const x11::Event& x11_event) {
  auto* expose = x11_event.As<x11::ExposeEvent>();
  return expose && expose->window == static_cast<x11::Window>(window_);
}

uint32_t NativeViewGLSurfaceGLX::GetDrawableHandle() const {
  return static_cast<uint32_t>(glx_window_);
}

UnmappedNativeViewGLSurfaceGLX::UnmappedNativeViewGLSurfaceGLX(
    const gfx::Size& size)
    : size_(size), config_(nullptr), window_(x11::Window::None), glx_window_() {
  // Ensure that we don't create a window with zero size.
  if (size_.GetArea() == 0)
    size_.SetSize(1, 1);
}

bool UnmappedNativeViewGLSurfaceGLX::Initialize(GLSurfaceFormat format) {
  DCHECK_EQ(window_, x11::Window::None);

  auto parent_window = ui::GetX11RootWindow();

  auto* conn = x11::Connection::Get();
  window_ = conn->GenerateId<x11::Window>();
  conn->CreateWindow(x11::CreateWindowRequest{
                         .depth = g_depth,
                         .wid = window_,
                         .parent = parent_window,
                         .width = size_.width(),
                         .height = size_.height(),
                         .c_class = x11::WindowClass::InputOutput,
                         .visual = g_visual,
                         .border_pixel = 0,
                         .colormap = g_colormap,
                     })
      .Sync();
  GetConfig();
  if (!config_) {
    LOG(ERROR) << "Failed to get GLXConfig";
    return false;
  }
  glx_window_ = static_cast<x11::Glx::Window>(
      glXCreateWindow(conn->GetXlibDisplay(x11::XlibDisplayType::kSyncing),
                      config_, static_cast<uint32_t>(window_), nullptr));
  if (glx_window_ == x11::Glx::Window{}) {
    LOG(ERROR) << "glXCreateWindow failed";
    return false;
  }
  return true;
}

void UnmappedNativeViewGLSurfaceGLX::Destroy() {
  config_ = nullptr;
  if (glx_window_ != x11::Glx::Window{}) {
    glXDestroyWindow(
        x11::Connection::Get()->GetXlibDisplay(x11::XlibDisplayType::kFlushing),
        static_cast<uint32_t>(glx_window_));
    glx_window_ = {};
  }
  if (window_ != x11::Window::None) {
    x11::Connection::Get()->DestroyWindow({window_});
    window_ = x11::Window::None;
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
    config_ = GetFbConfigForWindow(x11::Connection::Get(), window_);
  return config_;
}

GLSurfaceFormat UnmappedNativeViewGLSurfaceGLX::GetFormat() {
  return GLSurfaceFormat();
}

UnmappedNativeViewGLSurfaceGLX::~UnmappedNativeViewGLSurfaceGLX() {
  Destroy();
}

}  // namespace gl
