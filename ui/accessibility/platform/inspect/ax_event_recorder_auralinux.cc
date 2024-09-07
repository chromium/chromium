// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "ui/accessibility/platform/inspect/ax_event_recorder_auralinux.h"

#include <atk/atk.h>
#include <atk/atkutil.h>
#include <atspi/atspi.h>

#include "base/no_destructor.h"
#include "base/process/process_handle.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "ui/accessibility/platform/ax_platform_tree_manager.h"
#include "ui/accessibility/platform/inspect/ax_inspect_utils_auralinux.h"

namespace ui {

// static
AXEventRecorderAuraLinux* AXEventRecorderAuraLinux::instance_ = nullptr;

// static
std::vector<unsigned int>& GetATKListenerIds() {
  static base::NoDestructor<std::vector<unsigned int>> atk_listener_ids;
  return *atk_listener_ids;
}

// static
gboolean AXEventRecorderAuraLinux::OnATKEventReceived(
    GSignalInvocationHint* hint,
    unsigned int n_params,
    const GValue* params,
    gpointer data) {
  GSignalQuery query;
  g_signal_query(hint->signal_id, &query);

  if (instance_) {
    // "add" and "remove" are details; not part of the signal name itself.
    gchar* signal_name =
        g_strcmp0(query.signal_name, "children-changed")
            ? g_strdup(query.signal_name)
            : g_strconcat(query.signal_name, ":",
                          g_quark_to_string(hint->detail), nullptr);
    instance_->ProcessATKEvent(signal_name, n_params, params);
    g_free(signal_name);
  }
  return true;
}

bool AXEventRecorderAuraLinux::ShouldUseATSPI() {
  return pid_ != base::GetCurrentProcId() || !selector_.empty();
}

AXEventRecorderAuraLinux::AXEventRecorderAuraLinux(
    base::WeakPtr<AXPlatformTreeManager> manager,
    base::ProcessId pid,
    const AXTreeSelector& selector)
    : manager_(manager), pid_(pid), selector_(selector) {
  CHECK(!instance_) << "There can be only one instance of"
                    << " AccessibilityEventRecorder at a time.";

  if (ShouldUseATSPI()) {
    AddATSPIEventListeners();
  } else {
    AddATKEventListeners();
  }

  instance_ = this;
}

AXEventRecorderAuraLinux::~AXEventRecorderAuraLinux() {
  RemoveATSPIEventListeners();
  instance_ = nullptr;
}

void AXEventRecorderAuraLinux::AddATKEventListener(const char* event_name) {
  unsigned id = atk_add_global_event_listener(OnATKEventReceived, event_name);
  if (!id)
    LOG(FATAL) << "atk_add_global_event_listener failed for " << event_name;

  std::vector<unsigned int>& atk_listener_ids = GetATKListenerIds();
  atk_listener_ids.push_back(id);
}

void AXEventRecorderAuraLinux::AddATKEventListeners() {
  if (GetATKListenerIds().size() >= 1)
    return;
  GObject* gobject = G_OBJECT(g_object_new(G_TYPE_OBJECT, nullptr, nullptr));
  g_object_unref(atk_no_op_object_new(gobject));
  g_object_unref(gobject);

  AddATKEventListener("ATK:AtkDocument:load-complete");
  AddATKEventListener("ATK:AtkObject:state-change");
  AddATKEventListener("ATK:AtkObject:focus-event");
  AddATKEventListener("ATK:AtkObject:property-change");
  AddATKEventListener("ATK:AtkObject:children-changed");
  AddATKEventListener("ATK:AtkText:text-insert");
  AddATKEventListener("ATK:AtkText:text-remove");
  AddATKEventListener("ATK:AtkText:text-selection-changed");
  AddATKEventListener("ATK:AtkText:text-caret-moved");
  AddATKEventListener("ATK:AtkText:text-attributes-changed");
  AddATKEventListener("ATK:AtkSelection:selection-changed");
  AddATKEventListener("ATK:AtkTable:column-reordered");
  AddATKEventListener("ATK:AtkTable:row-reordered");
}

void AXEventRecorderAuraLinux::RemoveATKEventListeners() {
  std::vector<unsigned int>& atk_listener_ids = GetATKListenerIds();
  for (const auto& id : atk_listener_ids)
    atk_remove_global_event_listener(id);

  atk_listener_ids.clear();
}

std::string AXEventRecorderAuraLinux::AtkObjectToString(AtkObject* obj,
                                                        bool include_name) {
  std::string role = AtkRoleToString(atk_object_get_role(obj));
  base::ReplaceChars(role, " ", "_", &role);
  std::string str =
      base::StringPrintf("role=ROLE_%s", base::ToUpperASCII(role).c_str());
  // Getting the name breaks firing of name-change events. Allow disabling of
  // logging the name in those situations.
  if (include_name) {
    // Supplying null to the corresponding argument of a "%s" specifier is UB.
    // Explicitly avoid this.
    const gchar* name = atk_object_get_name(obj);
    str += base::StringPrintf(" name='%s'", name ? name : "(null)");
  }
  return str;
}

void AXEventRecorderAuraLinux::ProcessATKEvent(const char* event,
                                               unsigned int n_params,
                                               const GValue* params) {
  // If we don't have a root object, it means the tree is being destroyed.
  if (!manager_ || !manager_->RootDelegate()) {
    RemoveATKEventListeners();
    return;
  }

  bool log_name = true;
  std::string event_name(event);
  std::string log;
  if (event_name.find("property-change") != std::string::npos) {
    DCHECK_GE(n_params, 2u);
    AtkPropertyValues* property_values =
        static_cast<AtkPropertyValues*>(g_value_get_pointer(&params[1]));

    if (g_strcmp0(property_values->property_name, "accessible-value") == 0) {
      log += "VALUE-CHANGED:";
      log +=
          base::NumberToString(g_value_get_double(&property_values->new_value));
    } else if (g_strcmp0(property_values->property_name, "accessible-name") ==
               0) {
      const char* new_name = g_value_get_string(&property_values->new_value);
      log += "NAME-CHANGED:";
      log += (new_name) ? new_name : "(null)";
    } else if (g_strcmp0(property_values->property_name,
                         "accessible-description") == 0) {
      const char* new_description =
          g_value_get_string(&property_values->new_value);
      log += "DESCRIPTION-CHANGED:";
      log += (new_description) ? new_description : "(null)";
    } else if (g_strcmp0(property_values->property_name, "accessible-parent") ==
               0) {
      log += "PARENT-CHANGED";
      if (AtkObject* new_parent = static_cast<AtkObject*>(
              g_value_get_object(&property_values->new_value)))
        log += " PARENT:(" + AtkObjectToString(new_parent, log_name) + ")";
    } else {
      return;
    }
  } else if (event_name.find("children-changed") != std::string::npos) {
    log_name = false;
    log += base::ToUpperASCII(event);
    // Despite this actually being a signed integer, it's defined as a uint.
    int index = static_cast<int>(g_value_get_uint(&params[1]));
    log += base::StringPrintf(" index:%d", index);
    AtkObject* child = static_cast<AtkObject*>(g_value_get_pointer(&params[2]));
    if (child)
      log += " CHILD:(" + AtkObjectToString(child, log_name) + ")";
    else
      log += " CHILD:(NULL)";
  } else if (event_name.find("focus-event") != std::string::npos) {
    log += base::ToUpperASCII(event);
    gchar* parameter = g_strdup_value_contents(&params[1]);
    log += base::StringPrintf(":%s", parameter);
    g_free(parameter);
  } else {
    log += base::ToUpperASCII(event);
    if (event_name.find("state-change") != std::string::npos) {
      std::string state_type = g_value_get_string(&params[1]);
      log += ":" + base::ToUpperASCII(state_type);

      gchar* parameter = g_strdup_value_contents(&params[2]);
      log += base::StringPrintf(":%s", parameter);
      g_free(parameter);

    } else if (event_name.find("text-insert") != std::string::npos ||
               event_name.find("text-remove") != std::string::npos) {
      DCHECK_GE(n_params, 4u);
      log += base::StringPrintf(
          " (start=%i length=%i '%s')", g_value_get_int(&params[1]),
          g_value_get_int(&params[2]), g_value_get_string(&params[3]));
    }
  }

  AtkObject* obj = ATK_OBJECT(g_value_get_object(&params[0]));
  log += " " + AtkObjectToString(obj, log_name);

  std::string states;
  AtkStateSet* state_set = atk_object_ref_state_set(obj);
  for (int i = ATK_STATE_INVALID; i < ATK_STATE_LAST_DEFINED; i++) {
    AtkStateType state_type = static_cast<AtkStateType>(i);
    if (atk_state_set_contains_state(state_set, state_type))
      states += " " + base::ToUpperASCII(atk_state_type_get_name(state_type));
  }
  states = base::CollapseWhitespaceASCII(states, false);
  base::ReplaceChars(states, " ", ",", &states);
  log += base::StringPrintf(" %s", states.c_str());
  g_object_unref(state_set);

  OnEvent(log);
}

// This list is composed of the sorted event names taken from the list provided
// in the libatspi documentation at:
// https://developer.gnome.org/libatspi/stable/AtspiEventListener.html#atspi-event-listener-register
const char* const kEventNames[] = {
    "document:load-complete",
    "object:active-descendant-changed",
    "object:children-changed",
    "object:column-deleted",
    "object:column-inserted",
    "object:column-reordered",
    "object:model-changed",
    "object:property-change",
    "object:property-change:accessible-description",
    "object:property-change:accessible-name",
    "object:property-change:accessible-parent",
    "object:property-change:accessible-role",
    "object:property-change:accessible-table-caption",
    "object:property-change:accessible-table-column-description",
    "object:property-change:accessible-table-column-header",
    "object:property-change:accessible-table-row-description",
    "object:property-change:accessible-table-row-header",
    "object:property-change:accessible-table-summary",
    "object:property-change:accessible-value",
    "object:row-deleted",
    "object:row-inserted",
    "object:row-reordered",
    "object:selection-changed",
    "object:state-changed",
    "object:text-attributes-changed",
    "object:text-caret-moved",
    "object:text-changed",
    "object:text-selection-changed",
    "object:visible-data-changed",
    "window:activate",
    "window:close",
    "window:create",
    "window:deactivate",
    "window:desktop-create",
    "window:desktop-destroy",
    "window:lower",
    "window:maximize",
    "window:minimize",
    "window:move",
    "window:raise",
    "window:reparent",
    "window:resize",
    "window:restore",
    "window:restyle",
    "window:shade",
    "window:unshade",
};

static void OnATSPIEventReceived(AtspiEvent* event, void* data) {
  static_cast<AXEventRecorderAuraLinux*>(data)->ProcessATSPIEvent(event);
  g_boxed_free(ATSPI_TYPE_EVENT, static_cast<void*>(event));
}

void AXEventRecorderAuraLinux::AddATSPIEventListeners() {
  atspi_init();
  atspi_event_listener_ =
      atspi_event_listener_new(OnATSPIEventReceived, this, nullptr);

  GError* error = nullptr;
  for (size_t i = 0; i < std::size(kEventNames); i++) {
    atspi_event_listener_register(atspi_event_listener_, kEventNames[i],
                                  &error);
    if (error) {
      LOG(ERROR) << "Could not register event listener for " << kEventNames[i];
      g_clear_error(&error);
    }
  }
}

void AXEventRecorderAuraLinux::RemoveATSPIEventListeners() {
  if (!atspi_event_listener_)
    return;

  GError* error = nullptr;
  for (size_t i = 0; i < std::size(kEventNames); i++) {
    atspi_event_listener_deregister(atspi_event_listener_, kEventNames[i],
                                    nullptr);
    if (error) {
      LOG(ERROR) << "Could not deregister event listener for "
                 << kEventNames[i];
      g_clear_error(&error);
    }
  }

  g_object_unref(atspi_event_listener_);
  atspi_event_listener_ = nullptr;
}

void AXEventRecorderAuraLinux::ProcessATSPIEvent(const AtspiEvent* event) {
  GError* error = nullptr;

  // Ignore irrelevant events, i.e. fired for other applications.
  if (!pid_ && !selector_.empty()) {
    AtspiAccessible* application =
        atspi_accessible_get_application(event->source, &error);
    if (error) {
      g_clear_error(&error);
      return;
    }
    if (!application || application != FindAccessible(selector_))
      return;
  }

  if (pid_) {
    int pid = atspi_accessible_get_process_id(event->source, &error);
    if (!error && pid != pid_)
      return;
    g_clear_error(&error);
  }

  std::stringstream output;
  output << event->type << " ";

  GHashTable* attributes =
      atspi_accessible_get_attributes(event->source, &error);
  std::string html_tag, html_class, html_id;
  if (!error && attributes) {
    if (char* tag = static_cast<char*>(g_hash_table_lookup(attributes, "tag")))
      html_tag = tag;
    if (char* id = static_cast<char*>(g_hash_table_lookup(attributes, "id")))
      html_id = id;
    if (char* class_chars =
            static_cast<char*>(g_hash_table_lookup(attributes, "class")))
      html_class = std::string(".") + class_chars;
    g_hash_table_unref(attributes);
  }
  g_clear_error(&error);

  if (!html_tag.empty())
    output << "<" << html_tag << html_id << html_class << ">";

  AtspiRole role = atspi_accessible_get_role(event->source, &error);
  output << "role=";
  if (!error)
    output << ATSPIRoleToString(role);
  else
    output << "#error";
  g_clear_error(&error);

  char* name = atspi_accessible_get_name(event->source, &error);
  output << " name=";
  if (!error && name)
    output << name;
  else
    output << "#error";
  g_clear_error(&error);
  free(name);

  AtspiStateSet* atspi_states = atspi_accessible_get_state_set(event->source);
  GArray* state_array = atspi_state_set_get_states(atspi_states);
  std::vector<std::string> states;
  for (unsigned i = 0; i < state_array->len; i++) {
    AtspiStateType state_type = g_array_index(state_array, AtspiStateType, i);
    states.push_back(ATSPIStateToString(state_type));
  }
  g_array_free(state_array, TRUE);
  g_object_unref(atspi_states);
  output << " ";
  base::ranges::copy(states, std::ostream_iterator<std::string>(output, ", "));

  OnEvent(output.str());
}

}  // namespace ui
