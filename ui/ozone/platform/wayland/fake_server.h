// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_OZONE_PLATFORM_WAYLAND_FAKE_SERVER_H_
#define UI_OZONE_PLATFORM_WAYLAND_FAKE_SERVER_H_

#include <wayland-server-core.h>

#include "base/bind.h"
#include "base/files/scoped_file.h"
#include "base/message_loop/message_pump_libevent.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/gfx/geometry/rect.h"

struct wl_client;
struct wl_display;
struct wl_event_loop;
struct wl_global;
struct wl_resource;

namespace wl {

constexpr char kTextMimeTypeUtf8[] = "text/plain;charset=utf-8";
constexpr char kTextMimeTypeText[] = "text/plain";
constexpr char kSampleClipboardText[] = "This is a sample text for clipboard.";
constexpr char kSampleTextForDragAndDrop[] =
    "This is a sample text for drag-and-drop.";

// Base class for managing the life cycle of server objects.
class ServerObject {
 public:
  explicit ServerObject(wl_resource* resource);
  virtual ~ServerObject();

  wl_resource* resource() { return resource_; }

  static void OnResourceDestroyed(wl_resource* resource);

 private:
  wl_resource* resource_;

  DISALLOW_COPY_AND_ASSIGN(ServerObject);
};

class MockXdgTopLevel;

// Manage xdg_surface, zxdg_surface_v6 and zxdg_toplevel for providing desktop
// UI.
class MockXdgSurface : public ServerObject {
 public:
  MockXdgSurface(wl_resource* resource, const void* implementation);
  ~MockXdgSurface() override;

  // These mock methods are shared between xdg_surface and zxdg_toplevel
  // surface.
  MOCK_METHOD1(SetParent, void(wl_resource* parent));
  MOCK_METHOD1(SetTitle, void(const char* title));
  MOCK_METHOD1(SetAppId, void(const char* app_id));
  MOCK_METHOD1(Move, void(uint32_t serial));
  MOCK_METHOD2(Resize, void(uint32_t serial, uint32_t edges));
  MOCK_METHOD1(AckConfigure, void(uint32_t serial));
  MOCK_METHOD4(SetWindowGeometry,
               void(int32_t x, int32_t y, int32_t width, int32_t height));
  MOCK_METHOD0(SetMaximized, void());
  MOCK_METHOD0(UnsetMaximized, void());
  MOCK_METHOD0(SetFullscreen, void());
  MOCK_METHOD0(UnsetFullscreen, void());
  MOCK_METHOD0(SetMinimized, void());

  void set_xdg_toplevel(std::unique_ptr<MockXdgTopLevel> xdg_toplevel) {
    xdg_toplevel_ = std::move(xdg_toplevel);
  }
  MockXdgTopLevel* xdg_toplevel() { return xdg_toplevel_.get(); }

 private:
  // Used when xdg v6 is used.
  std::unique_ptr<MockXdgTopLevel> xdg_toplevel_;

  DISALLOW_COPY_AND_ASSIGN(MockXdgSurface);
};

// Manage zxdg_toplevel for providing desktop UI.
class MockXdgTopLevel : public MockXdgSurface {
 public:
  explicit MockXdgTopLevel(wl_resource* resource);
  ~MockXdgTopLevel() override;

  // TODO(msisov): mock other zxdg_toplevel specific methods once implementation
  // is done. example: MOCK_METHOD2(SetMaxSize, void(int32_t width, int32_t
  // height());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockXdgTopLevel);
};

// A mocked positioner object, which provides a collection of rules of a child
// surface relative to a parent surface.
class MockPositioner : public ServerObject {
 public:
  explicit MockPositioner(wl_resource* resource);
  ~MockPositioner() override;

  void set_size(gfx::Size size) { size_ = size; }
  gfx::Size size() const { return size_; }

  void set_anchor_rect(gfx::Rect anchor_rect) { anchor_rect_ = anchor_rect; }
  gfx::Rect anchor_rect() const { return anchor_rect_; }

  void set_anchor(uint32_t anchor) { anchor_ = anchor; }

  void set_gravity(uint32_t gravity) { gravity_ = gravity; }

 private:
  gfx::Rect anchor_rect_;
  gfx::Size size_;
  uint32_t anchor_;
  uint32_t gravity_;

  DISALLOW_COPY_AND_ASSIGN(MockPositioner);
};

class MockXdgPopup : public ServerObject {
 public:
  MockXdgPopup(wl_resource* resource, const void* implementation);
  ~MockXdgPopup() override;

  MOCK_METHOD1(Grab, void(uint32_t serial));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockXdgPopup);
};

// Manage client surface
class MockSurface : public ServerObject {
 public:
  explicit MockSurface(wl_resource* resource);
  ~MockSurface() override;

  static MockSurface* FromResource(wl_resource* resource);

  MOCK_METHOD3(Attach, void(wl_resource* buffer, int32_t x, int32_t y));
  MOCK_METHOD4(Damage,
               void(int32_t x, int32_t y, int32_t width, int32_t height));
  MOCK_METHOD0(Commit, void());

  void set_xdg_surface(std::unique_ptr<MockXdgSurface> xdg_surface) {
    xdg_surface_ = std::move(xdg_surface);
  }
  MockXdgSurface* xdg_surface() { return xdg_surface_.get(); }

 private:
  std::unique_ptr<MockXdgSurface> xdg_surface_;

  DISALLOW_COPY_AND_ASSIGN(MockSurface);
};

class MockPointer : public ServerObject {
 public:
  explicit MockPointer(wl_resource* resource);
  ~MockPointer() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockPointer);
};

class MockKeyboard : public ServerObject {
 public:
  explicit MockKeyboard(wl_resource* resource);
  ~MockKeyboard() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockKeyboard);
};

class MockTouch : public ServerObject {
 public:
  explicit MockTouch(wl_resource* resource);
  ~MockTouch() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockTouch);
};

// Manage zwp_text_input_v1.
class MockZwpTextInput : public ServerObject {
 public:
  MockZwpTextInput(wl_resource* resource, const void* implementation);
  ~MockZwpTextInput() override;

  MOCK_METHOD0(Reset, void());
  MOCK_METHOD1(Activate, void(wl_resource* window));
  MOCK_METHOD0(Deactivate, void());
  MOCK_METHOD0(ShowInputPanel, void());
  MOCK_METHOD0(HideInputPanel, void());
  MOCK_METHOD4(SetCursorRect,
               void(int32_t x, int32_t y, int32_t width, int32_t height));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockZwpTextInput);
};

class MockDataOffer : public ServerObject {
 public:
  explicit MockDataOffer(wl_resource* resource);
  ~MockDataOffer() override;

  void Receive(const std::string& mime_type, base::ScopedFD fd);
  void OnOffer(const std::string& mime_type);

 private:
  base::Thread io_thread_;
  base::WeakPtrFactory<MockDataOffer> write_data_weak_ptr_factory_;
};

class MockDataSource : public ServerObject {
 public:
  explicit MockDataSource(wl_resource* resource);
  ~MockDataSource() override;

  void Offer(const std::string& mime_type);

  using ReadDataCallback =
      base::OnceCallback<void(const std::vector<uint8_t>&)>;
  void ReadData(ReadDataCallback);

  void OnCancelled();

 private:
  void DataReadCb(ReadDataCallback callback, const std::vector<uint8_t>& data);

  base::Thread io_thread_;
  base::WeakPtrFactory<MockDataSource> read_data_weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(MockDataSource);
};

class MockDataDevice : public ServerObject {
 public:
  MockDataDevice(wl_client* client, wl_resource* resource);
  ~MockDataDevice() override;

  void SetSelection(MockDataSource* data_source, uint32_t serial);

  MockDataOffer* OnDataOffer();
  void OnEnter(uint32_t serial,
               wl_resource* surface,
               wl_fixed_t x,
               wl_fixed_t y,
               MockDataOffer& data_offer);
  void OnLeave();
  void OnMotion(uint32_t time, wl_fixed_t x, wl_fixed_t y);
  void OnDrop();
  void OnSelection(MockDataOffer& data_offer);

 private:
  std::unique_ptr<MockDataOffer> data_offer_;
  wl_client* client_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(MockDataDevice);
};

struct GlobalDeleter {
  void operator()(wl_global* global);
};

// Base class for managing the life cycle of global objects:
// It presents a global object used to emit global events to all clients.
class Global {
 public:
  Global(const wl_interface* interface,
         const void* implementation,
         uint32_t version);
  virtual ~Global();

  // Create a global object.
  bool Initialize(wl_display* display);
  // Called from Bind() to send additional information to clients.
  virtual void OnBind() {}

  // The first bound resource to this global, which is usually all that is
  // useful when testing a simple client.
  wl_resource* resource() { return resource_; }

  // Send the global event to clients.
  static void Bind(wl_client* client,
                   void* data,
                   uint32_t version,
                   uint32_t id);
  static void OnResourceDestroyed(wl_resource* resource);

 private:
  std::unique_ptr<wl_global, GlobalDeleter> global_;

  const wl_interface* interface_;
  const void* implementation_;
  const uint32_t version_;
  wl_resource* resource_ = nullptr;

  DISALLOW_COPY_AND_ASSIGN(Global);
};

// Manage wl_compositor object.
class MockCompositor : public Global {
 public:
  MockCompositor();
  ~MockCompositor() override;

  void AddSurface(std::unique_ptr<MockSurface> surface);

 private:
  std::vector<std::unique_ptr<MockSurface>> surfaces_;

  DISALLOW_COPY_AND_ASSIGN(MockCompositor);
};

// Manage wl_data_device_manager object.
class MockDataDeviceManager : public Global {
 public:
  MockDataDeviceManager();
  ~MockDataDeviceManager() override;

  MockDataDevice* data_device() { return data_device_.get(); }
  void set_data_device(std::unique_ptr<MockDataDevice> data_device) {
    data_device_ = std::move(data_device);
  }

  MockDataSource* data_source() { return data_source_.get(); }
  void set_data_source(std::unique_ptr<MockDataSource> data_source) {
    data_source_ = std::move(data_source);
  }

 private:
  std::unique_ptr<MockDataDevice> data_device_;
  std::unique_ptr<MockDataSource> data_source_;

  DISALLOW_COPY_AND_ASSIGN(MockDataDeviceManager);
};

// Handle wl_output object.
class MockOutput : public Global {
 public:
  MockOutput();
  ~MockOutput() override;
  void SetRect(const gfx::Rect rect) { rect_ = rect; }
  const gfx::Rect GetRect() { return rect_; }
  void OnBind() override;

 private:
  gfx::Rect rect_;

  DISALLOW_COPY_AND_ASSIGN(MockOutput);
};

// Manage wl_seat object. A seat is a group of keyboards, pointer and touch
// devices. This object is published as a global during start up, or when such a
// device is hot plugged. A seat typically has a pointer and maintains a
// keyboard focus and a pointer focus.
// https://people.freedesktop.org/~whot/wayland-doxygen/wayland/Server/structwl__seat__interface.html
class MockSeat : public Global {
 public:
  MockSeat();
  ~MockSeat() override;

  void set_pointer(std::unique_ptr<MockPointer> pointer) {
    pointer_ = std::move(pointer);
  }
  MockPointer* pointer() { return pointer_.get(); }

  void set_keyboard(std::unique_ptr<MockKeyboard> keyboard) {
    keyboard_ = std::move(keyboard);
  }
  MockKeyboard* keyboard() { return keyboard_.get(); }

  void set_touch(std::unique_ptr<MockTouch> touch) {
    touch_ = std::move(touch);
  }
  MockTouch* touch() { return touch_.get(); }

 private:
  std::unique_ptr<MockPointer> pointer_;
  std::unique_ptr<MockKeyboard> keyboard_;
  std::unique_ptr<MockTouch> touch_;

  DISALLOW_COPY_AND_ASSIGN(MockSeat);
};

// Manage xdg_shell object.
class MockXdgShell : public Global {
 public:
  MockXdgShell();
  ~MockXdgShell() override;

  MOCK_METHOD1(UseUnstableVersion, void(int32_t version));
  MOCK_METHOD1(Pong, void(uint32_t serial));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockXdgShell);
};

// Manage zxdg_shell_v6 object.
class MockXdgShellV6 : public Global {
 public:
  MockXdgShellV6();
  ~MockXdgShellV6() override;

  MOCK_METHOD1(Pong, void(uint32_t serial));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockXdgShellV6);
};

// Manage zwp_text_input_manager_v1 object.
class MockTextInputManagerV1 : public Global {
 public:
  MockTextInputManagerV1();
  ~MockTextInputManagerV1() override;

  std::unique_ptr<MockZwpTextInput> text_input;

 private:
  DISALLOW_COPY_AND_ASSIGN(MockTextInputManagerV1);
};

struct DisplayDeleter {
  void operator()(wl_display* display);
};

class FakeServer : public base::Thread, base::MessagePumpLibevent::FdWatcher {
 public:
  FakeServer();
  ~FakeServer() override;

  // Start the fake Wayland server. If this succeeds, the WAYLAND_SOCKET
  // environment variable will be set to the string representation of a file
  // descriptor that a client can connect to. The caller is responsible for
  // ensuring that this file descriptor gets closed (for example, by calling
  // wl_display_connect). Start instantiates an xdg_shell version 5 or 6
  // according to |shell_version| passed.
  bool Start(uint32_t shell_version);

  // Pause the server when it becomes idle.
  void Pause();

  // Resume the server after flushing client connections.
  void Resume();

  template <typename T>
  T* GetObject(uint32_t id) {
    wl_resource* resource = wl_client_get_object(client_, id);
    return resource ? T::FromResource(resource) : nullptr;
  }

  void CreateAndInitializeOutput() {
    auto output = std::make_unique<MockOutput>();
    output->Initialize(display());
    globals_.push_back(std::move(output));
  }

  MockDataDeviceManager* data_device_manager() { return &data_device_manager_; }
  MockSeat* seat() { return &seat_; }
  MockXdgShell* xdg_shell() { return &xdg_shell_; }
  MockOutput* output() { return &output_; }
  MockTextInputManagerV1* text_input_manager_v1() {
    return &zwp_text_input_manager_v1_;
  }

  wl_display* display() const { return display_.get(); }

 private:
  void DoPause();

  std::unique_ptr<base::MessagePump> CreateMessagePump();

  // base::MessagePumpLibevent::FdWatcher
  void OnFileCanReadWithoutBlocking(int fd) override;
  void OnFileCanWriteWithoutBlocking(int fd) override;

  std::unique_ptr<wl_display, DisplayDeleter> display_;
  wl_client* client_ = nullptr;
  wl_event_loop* event_loop_ = nullptr;

  base::WaitableEvent pause_event_;
  base::WaitableEvent resume_event_;

  // Represent Wayland global objects
  MockCompositor compositor_;
  MockDataDeviceManager data_device_manager_;
  MockOutput output_;
  MockSeat seat_;
  MockXdgShell xdg_shell_;
  MockXdgShellV6 zxdg_shell_v6_;
  MockTextInputManagerV1 zwp_text_input_manager_v1_;

  std::vector<std::unique_ptr<Global>> globals_;

  base::MessagePumpLibevent::FdWatchController controller_;

  DISALLOW_COPY_AND_ASSIGN(FakeServer);
};

}  // namespace wl

#endif  // UI_OZONE_PLATFORM_WAYLAND_FAKE_SERVER_H_
