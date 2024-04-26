// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/ozone/platform/wayland/test/test_zaura_shell.h"

#include "base/notreached.h"
#include "ui/ozone/platform/wayland/test/mock_xdg_surface.h"
#include "ui/ozone/platform/wayland/test/server_object.h"
#include "ui/ozone/platform/wayland/test/test_output.h"
#include "ui/ozone/platform/wayland/test/test_zaura_output.h"
#include "ui/ozone/platform/wayland/test/test_zaura_popup.h"
#include "ui/ozone/platform/wayland/test/test_zaura_surface.h"
#include "ui/ozone/platform/wayland/test/test_zaura_toplevel.h"

namespace wl {

namespace {

constexpr uint32_t kZAuraShellVersion = 65;
constexpr uint32_t kZAuraOutputVersion = 44;

void GetAuraSurface(wl_client* client,
                    wl_resource* resource,
                    uint32_t id,
                    wl_resource* surface_resource) {
  CreateResourceWithImpl<TestZAuraSurface>(client, &zaura_surface_interface,
                                           kZAuraShellVersion,
                                           &kTestZAuraSurfaceImpl, id);
}

void GetAuraOutput(wl_client* client,
                   wl_resource* resource,
                   uint32_t id,
                   wl_resource* output_resource) {
  wl_resource* zaura_output_resource = CreateResourceWithImpl<TestZAuraOutput>(
      client, &zaura_output_interface, kZAuraOutputVersion,
      &kTestZAuraOutputImpl, id);
  auto* output = GetUserDataAs<TestOutput>(output_resource);
  output->SetAuraOutput(GetUserDataAs<TestZAuraOutput>(zaura_output_resource));
}

void SurfaceSubmissionInPixelCoordinates(wl_client* client,
                                         wl_resource* resource) {
  // TODO(crbug.com/40232463): Implement zaura-shell protocol requests and test
  // their usage.
  NOTIMPLEMENTED_LOG_ONCE();
}

void GetAuraToplevelForXdgToplevel(wl_client* client,
                                   wl_resource* resource,
                                   uint32_t id,
                                   wl_resource* toplevel) {
  wl_resource* zaura_toplevel_resource =
      CreateResourceWithImpl<TestZAuraToplevel>(
          client, &zaura_toplevel_interface, kZAuraShellVersion,
          &kTestZAuraToplevelImpl, id);
  auto* xdg_toplevel = GetUserDataAs<MockXdgTopLevel>(toplevel);
  xdg_toplevel->set_zaura_toplevel(
      GetUserDataAs<TestZAuraToplevel>(zaura_toplevel_resource));
}

void GetAuraPopupForXdgPopup(wl_client* client,
                             wl_resource* resource,
                             uint32_t id,
                             wl_resource* popup) {
  CreateResourceWithImpl<TestZAuraPopup>(client, &zaura_popup_interface,
                                         kZAuraShellVersion,
                                         &kTestZAuraPopupImpl, id);
}

const struct zaura_shell_interface kTestZAuraShellImpl = {
    &GetAuraSurface,
    &GetAuraOutput,
    &SurfaceSubmissionInPixelCoordinates,
    &GetAuraToplevelForXdgToplevel,
    &GetAuraPopupForXdgPopup,
    &DestroyResource,
};

}  // namespace

TestZAuraShell::TestZAuraShell()
    : GlobalObject(&zaura_shell_interface,
                   &kTestZAuraShellImpl,
                   kZAuraShellVersion) {}

TestZAuraShell::~TestZAuraShell() = default;

void TestZAuraShell::SetCompositorVersion(const std::string& version_string) {
  if (version_string == compositor_version_string_) {
    return;
  }
  compositor_version_string_ = version_string;
  MaybeSendCompositorVersion();
}

void TestZAuraShell::SetBugFixes(std::vector<uint32_t> bug_fixes) {
  bug_fixes_ = std::move(bug_fixes);
  SendBugFixes();
}

void TestZAuraShell::SendAllBugFixesSent() {
  if (resource() && wl_resource_get_version(resource()) >=
                        ZAURA_SHELL_ALL_BUG_FIXES_SENT_SINCE_VERSION) {
    zaura_shell_send_all_bug_fixes_sent(resource());
  }
}

void TestZAuraShell::OnBind() {
  MaybeSendCompositorVersion();
  SendBugFixes();
  SendAllBugFixesSent();
}

void TestZAuraShell::MaybeSendCompositorVersion() {
  if (resource() &&
      wl_resource_get_version(resource()) >=
          ZAURA_SHELL_COMPOSITOR_VERSION_SINCE_VERSION &&
      !compositor_version_string_.empty()) {
    zaura_shell_send_compositor_version(resource(),
                                        compositor_version_string_.c_str());
  }
}

void TestZAuraShell::SendBugFixes() {
  if (resource() && wl_resource_get_version(resource()) >=
                        ZAURA_SHELL_BUG_FIX_SINCE_VERSION) {
    for (const uint32_t bug_fix : bug_fixes_)
      zaura_shell_send_bug_fix(resource(), bug_fix);
  }
}

}  // namespace wl
