// Copyright (c) Microsoft Corporation

#include "Driver.h"
#include "Driver.tmh"

#include "Direct3DDevice.h"
#include "Edid.h"
#include "HelperMethods.h"
#include "IndirectMonitor.h"
#include "SwapChainProcessor.h"
#include "public/properties.h"

#include <memory>

#pragma region EventDeclaration

extern "C" DRIVER_INITIALIZE DriverEntry;

EVT_WDF_DRIVER_DEVICE_ADD EvtWdfDriverDeviceAdd;
EVT_WDF_DEVICE_D0_ENTRY EvtWdfDeviceD0Entry;

EVT_IDD_CX_ADAPTER_INIT_FINISHED EvtIddCxAdapterInitFinished;
EVT_IDD_CX_ADAPTER_COMMIT_MODES EvtIddCxAdapterCommitModes;

EVT_IDD_CX_PARSE_MONITOR_DESCRIPTION EvtIddCxParseMonitorDescription;
EVT_IDD_CX_MONITOR_GET_DEFAULT_DESCRIPTION_MODES EvtIddCxMonitorGetDefaultModes;
EVT_IDD_CX_MONITOR_QUERY_TARGET_MODES EvtIddCxMonitorQueryModes;

EVT_IDD_CX_MONITOR_ASSIGN_SWAPCHAIN EvtIddCxMonitorAssignSwapChain;
EVT_IDD_CX_MONITOR_UNASSIGN_SWAPCHAIN EvtIddCxMonitorUnassignSwapChain;

struct IndirectDeviceContextWrapper {
  display::test::IndirectDeviceContext* pContext;

  void Cleanup() {
    delete pContext;
    pContext = nullptr;
  }
};

struct IndirectMonitorContextWrapper {
  display::test::IndirectMonitorContext* pContext;

  void Cleanup() { pContext = nullptr; }
};

#pragma endregion

#pragma region EventDefinations

// This macro creates the methods for accessing an IndirectDeviceContextWrapper
// as a context for a WDF object
WDF_DECLARE_CONTEXT_TYPE(IndirectDeviceContextWrapper);

WDF_DECLARE_CONTEXT_TYPE(IndirectMonitorContextWrapper);

extern "C" BOOL WINAPI DllMain(_In_ HINSTANCE hInstance,
                               _In_ UINT dwReason,
                               _In_opt_ LPVOID lpReserved) {
  UNREFERENCED_PARAMETER(hInstance);
  UNREFERENCED_PARAMETER(lpReserved);
  UNREFERENCED_PARAMETER(dwReason);

  return TRUE;
}

_Use_decl_annotations_ extern "C" NTSTATUS DriverEntry(
    PDRIVER_OBJECT pDriverObject,
    PUNICODE_STRING pRegistryPath) {
  WDF_DRIVER_CONFIG Config;
  NTSTATUS Status;

  WDF_OBJECT_ATTRIBUTES Attributes;
  WDF_OBJECT_ATTRIBUTES_INIT(&Attributes);

  WDF_DRIVER_CONFIG_INIT(&Config, EvtWdfDriverDeviceAdd);

  Status = WdfDriverCreate(pDriverObject, pRegistryPath, &Attributes, &Config,
                           WDF_NO_HANDLE);
  if (!NT_SUCCESS(Status)) {
    return Status;
  }
  WPP_INIT_TRACING(pDriverObject, pRegistryPath);
  // TODO: Call WPP_CLEANUP somewhere..

  return Status;
}

_Use_decl_annotations_ NTSTATUS
EvtWdfDriverDeviceAdd(WDFDRIVER Driver, PWDFDEVICE_INIT pDeviceInit) {
  TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "EvtWdfDriverDeviceAdd");

  NTSTATUS Status = STATUS_SUCCESS;
  WDF_PNPPOWER_EVENT_CALLBACKS PnpPowerCallbacks;

  UNREFERENCED_PARAMETER(Driver);

  // Register for power callbacks - in this driver only power-on is needed
  WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&PnpPowerCallbacks);
  PnpPowerCallbacks.EvtDeviceD0Entry = EvtWdfDeviceD0Entry;
  WdfDeviceInitSetPnpPowerEventCallbacks(pDeviceInit, &PnpPowerCallbacks);

  IDD_CX_CLIENT_CONFIG IddConfig;
  IDD_CX_CLIENT_CONFIG_INIT(&IddConfig);

  // If the driver wishes to handle custom IoDeviceControl requests, it's
  // necessary to use this callback since IddCx redirects IoDeviceControl
  // requests to an internal queue. This driver does not require this.
  // IddConfig.EvtIddCxDeviceIoControl = IoDeviceControl;

  IddConfig.EvtIddCxAdapterInitFinished = EvtIddCxAdapterInitFinished;
  IddConfig.EvtIddCxParseMonitorDescription = EvtIddCxParseMonitorDescription;
  IddConfig.EvtIddCxMonitorGetDefaultDescriptionModes =
      EvtIddCxMonitorGetDefaultModes;
  IddConfig.EvtIddCxMonitorQueryTargetModes = EvtIddCxMonitorQueryModes;
  IddConfig.EvtIddCxAdapterCommitModes = EvtIddCxAdapterCommitModes;
  IddConfig.EvtIddCxMonitorAssignSwapChain = EvtIddCxMonitorAssignSwapChain;
  IddConfig.EvtIddCxMonitorUnassignSwapChain = EvtIddCxMonitorUnassignSwapChain;

  Status = IddCxDeviceInitConfig(pDeviceInit, &IddConfig);
  if (!NT_SUCCESS(Status)) {
    return Status;
  }

  WDF_OBJECT_ATTRIBUTES Attr;
  WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&Attr, IndirectDeviceContextWrapper);
  Attr.EvtCleanupCallback = [](WDFOBJECT Object) {
    // Automatically cleanup the context when the WDF object is about to be
    // deleted
    auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(Object);
    if (pContext) {
      pContext->Cleanup();
    }
  };

  WDFDEVICE Device = nullptr;
  Status = WdfDeviceCreate(&pDeviceInit, &Attr, &Device);
  if (!NT_SUCCESS(Status)) {
    return Status;
  }

  Status = IddCxDeviceInitialize(Device);

  // Create a new device context object and attach it to the WDF device object
  auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(Device);
  pContext->pContext = new display::test::IndirectDeviceContext(Device);
  return Status;
}

_Use_decl_annotations_ NTSTATUS
EvtWdfDeviceD0Entry(WDFDEVICE Device, WDF_POWER_DEVICE_STATE PreviousState) {
  UNREFERENCED_PARAMETER(PreviousState);

  // This function is called by WDF to start the device in the fully-on power
  // state.

  auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(Device);
  pContext->pContext->InitAdapter();

  return STATUS_SUCCESS;
}

#pragma endregion

#pragma region IndirectContext

namespace display::test {
IndirectDeviceContext::IndirectDeviceContext(_In_ WDFDEVICE WdfDevice)
    : m_WdfDevice(WdfDevice) {
  m_Adapter = {};
}

IndirectDeviceContext::~IndirectDeviceContext() {
  if (m_hThread.Get()) {
    TerminateThread(m_hThread.Get(), 0);
    // Wait for the thread to terminate
    WaitForSingleObject(m_hThread.Get(), INFINITE);
  }
}

void IndirectDeviceContext::InitAdapter() {
  // ==============================
  // TODO: Update the below diagnostic information in accordance with the target
  // hardware. The strings and version numbers are used for telemetry and may be
  // displayed to the user in some situations.
  //
  // This is also where static per-adapter capabilities are determined.
  // ==============================

  IDDCX_ADAPTER_CAPS AdapterCaps = {};
  AdapterCaps.Size = sizeof(AdapterCaps);

  // Declare basic feature support for the adapter (required)
  AdapterCaps.MaxMonitorsSupported =
      static_cast<DWORD>(DriverProperties::kMaxMonitors);
  AdapterCaps.EndPointDiagnostics.Size =
      sizeof(AdapterCaps.EndPointDiagnostics);
  AdapterCaps.EndPointDiagnostics.GammaSupport =
      IDDCX_FEATURE_IMPLEMENTATION_NONE;
  AdapterCaps.EndPointDiagnostics.TransmissionType =
      IDDCX_TRANSMISSION_TYPE_WIRED_OTHER;

  // Declare your device strings for telemetry (required)
  AdapterCaps.EndPointDiagnostics.pEndPointFriendlyName = L"IDD Virtual Device";
  AdapterCaps.EndPointDiagnostics.pEndPointManufacturerName =
      L"IDD Virtual Manufacturer";
  AdapterCaps.EndPointDiagnostics.pEndPointModelName = L"IDD Virtual Model";

  // Declare your hardware and firmware versions (required)
  IDDCX_ENDPOINT_VERSION Version = {};
  Version.Size = sizeof(Version);
  Version.MajorVer = 1;
  AdapterCaps.EndPointDiagnostics.pFirmwareVersion = &Version;
  AdapterCaps.EndPointDiagnostics.pHardwareVersion = &Version;

  // Initialize a WDF context that can store a pointer to the device context
  // object
  WDF_OBJECT_ATTRIBUTES Attr;
  WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&Attr, IndirectDeviceContextWrapper);

  IDARG_IN_ADAPTER_INIT AdapterInit = {};
  AdapterInit.WdfDevice = m_WdfDevice;
  AdapterInit.pCaps = &AdapterCaps;
  AdapterInit.ObjectAttributes = &Attr;

  // Start the initialization of the adapter, which will trigger the
  // AdapterFinishInit callback later
  IDARG_OUT_ADAPTER_INIT AdapterInitOut;
  NTSTATUS Status = IddCxAdapterInitAsync(&AdapterInit, &AdapterInitOut);

  if (NT_SUCCESS(Status)) {
    // Store a reference to the WDF adapter handle
    m_Adapter = AdapterInitOut.AdapterObject;

    // Store the device context object into the WDF object context
    auto* pContext =
        WdfObjectGet_IndirectDeviceContextWrapper(AdapterInitOut.AdapterObject);
    pContext->pContext = this;
  }
}

void IndirectDeviceContext::FinishInit() {
  SyncRequestedConfig();
  m_hThread.Attach(CreateThread(nullptr, 0, RunThread, this, 0, nullptr));
}

void IndirectDeviceContext::SyncRequestedConfig() {
  // Read the properties structure sent from the client code that created
  // the software device.
  WDF_DEVICE_PROPERTY_DATA propertyRead;
  WDF_DEVICE_PROPERTY_DATA_INIT(&propertyRead, &DisplayConfigurationProperty);
  propertyRead.Lcid = LOCALE_NEUTRAL;
  propertyRead.Flags = PLUGPLAY_PROPERTY_PERSISTENT;
  DriverProperties driver_properties;
  ULONG requiredSize = 0;
  DEVPROPTYPE propType;
  NTSTATUS Status = WdfDeviceQueryPropertyEx(
      m_WdfDevice, &propertyRead, sizeof(DriverProperties), &driver_properties,
      &requiredSize, &propType);
  if (!NT_SUCCESS(Status)) {
    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
                "WdfDeviceQueryPropertyEx failed: %!STATUS!", Status);
    return;
  }
  const std::vector<MonitorConfig>& requested_configs =
      driver_properties.requested_configs();
  std::vector<IndirectMonitor> requested_monitors;
  for (const auto& config : requested_configs) {
    IndirectMonitor indirect_monitor;

    Edid edid(indirect_monitor.pEdidBlock.data());
    bool success = edid.GetTimingEntry(0)->SetMode(
        config.width(), config.height(), config.v_sync());
    edid.SetProductCode(config.product_code());
    edid.SetSerialNumber(config.product_code());
    if (!success) {
      TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "SetMode() unsuccessful");
    }
    edid.UpdateChecksum();
    indirect_monitor.pEdidBlock = edid.getEdidBlock();
    indirect_monitor.pConfigList.push_back(config);
    indirect_monitor.id = config.product_code();
    requested_monitors.push_back(indirect_monitor);
  }

  // Attach monitors that were added but not attached yet.
  for (const auto& indirect_monitor : requested_monitors) {
    auto it = std::find_if(
        monitors.begin(), monitors.end(),
        [&indirect_monitor](const std::unique_ptr<IndirectMonitorContext>& m) {
          return m && m->monitor_config().id == indirect_monitor.id;
        });
    if (it == monitors.end()) {
      TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
                  "New monitor config detected. Attaching monitor. %u",
                  indirect_monitor.id);
      Status = AddMonitor(indirect_monitor);
      if (!NT_SUCCESS(Status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
                    "AttachMonitor failed: %!STATUS!", Status);
      }
    }
  }

  // Detach monitors that are no longer in the requested config.
  for (auto& monitor : monitors) {
    if (monitor &&
        std::find_if(requested_monitors.begin(), requested_monitors.end(),
                     [&monitor](const IndirectMonitor& m) {
                       return m.id == monitor->monitor_config().id;
                     }) == requested_monitors.end()) {
      TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
                  "Monitor removed from config. Detaching. %u. %ix%i",
                  monitor->monitor_config().id,
                  monitor->monitor_config().pConfigList[0].width(),
                  monitor->monitor_config().pConfigList[0].height());
      Status = monitor->Detach();
      if (!NT_SUCCESS(Status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
                    "Monitor detach failed: %!STATUS!", Status);
      }
      monitor.reset();
    }
  }
}

DWORD CALLBACK IndirectDeviceContext::RunThread(LPVOID Argument) {
  IndirectDeviceContext* context =
      reinterpret_cast<IndirectDeviceContext*>(Argument);
  // Continually poll for changes to the requested monitor config.
  while (true) {
    context->SyncRequestedConfig();
    Sleep(200);
  }
}

NTSTATUS IndirectDeviceContext::AddMonitor(IndirectMonitor monitor) {
  NTSTATUS Status = STATUS_SUCCESS;
  auto it = std::find(monitors.begin(), monitors.end(), nullptr);
  if (it == monitors.end()) {
    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "All connectors are in use.");
    return STATUS_INVALID_PARAMETER;
  }
  size_t connector_index = it - monitors.begin();
  TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Connector index: %llu",
              connector_index);
  // ==============================
  // TODO: In a real driver, the EDID should be retrieved dynamically from a
  // connected physical monitor. The EDIDs provided here are purely for
  // demonstration. Monitor manufacturers are required to correctly fill in
  // physical monitor attributes in order to allow the OS to optimize settings
  // like viewing distance and scale factor. Manufacturers should also use a
  // unique serial number every single device to ensure the OS can tell the
  // monitors apart.
  // ==============================

  WDF_OBJECT_ATTRIBUTES Attr;
  WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&Attr, IndirectMonitorContextWrapper);

  // In this driver, we report a monitor right away but a real driver
  // would do this when a monitor connection event occurs
  IDDCX_MONITOR_INFO MonitorInfo = {};
  MonitorInfo.Size = sizeof(MonitorInfo);
  MonitorInfo.MonitorType = DISPLAYCONFIG_OUTPUT_TECHNOLOGY_HDMI;
  MonitorInfo.ConnectorIndex = static_cast<UINT>(connector_index);

  MonitorInfo.MonitorDescription.Size = sizeof(MonitorInfo.MonitorDescription);
  MonitorInfo.MonitorDescription.Type = IDDCX_MONITOR_DESCRIPTION_TYPE_EDID;
  MonitorInfo.MonitorDescription.DataSize = Edid::kBlockSize;
  MonitorInfo.MonitorDescription.pData = monitor.pEdidBlock.data();

  // Create a container ID
  CoCreateGuid(&MonitorInfo.MonitorContainerId);

  IDARG_IN_MONITORCREATE MonitorCreate = {};
  MonitorCreate.ObjectAttributes = &Attr;
  MonitorCreate.pMonitorInfo = &MonitorInfo;

  // Create a monitor object with the specified monitor descriptor
  IDARG_OUT_MONITORCREATE MonitorCreateOut;
  Status = IddCxMonitorCreate(m_Adapter, &MonitorCreate, &MonitorCreateOut);
  if (NT_SUCCESS(Status)) {
    // Create a new monitor context object and attach it to the Idd monitor
    // object
    auto* pMonitorContextWrapper = WdfObjectGet_IndirectMonitorContextWrapper(
        MonitorCreateOut.MonitorObject);
    auto monitor_ctx = std::make_unique<IndirectMonitorContext>(
        MonitorCreateOut.MonitorObject, monitor);
    pMonitorContextWrapper->pContext = monitor_ctx.get();
    // Tell the OS that the monitor has been plugged in
    Status = monitor_ctx->Attach();
    monitors[connector_index] = std::move(monitor_ctx);
  }
  return Status;
}

IndirectMonitorContext::IndirectMonitorContext(_In_ IDDCX_MONITOR Monitor,
                                               IndirectMonitor config)
    : m_Monitor(Monitor), monitor_config_(std::move(config)) {}

IndirectMonitorContext::~IndirectMonitorContext() {
  m_ProcessingThread.reset();
}

void IndirectMonitorContext::AssignSwapChain(IDDCX_SWAPCHAIN SwapChain,
                                             LUID RenderAdapter,
                                             HANDLE NewFrameEvent) {
  m_ProcessingThread.reset();

  auto Device = std::make_unique<Direct3DDevice>(RenderAdapter);
  if (FAILED(Device->Init())) {
    // It's important to delete the swap-chain if D3D initialization fails, so
    // that the OS knows to generate a new swap-chain and try again.
    WdfObjectDelete(SwapChain);
  } else {
    // Create a new swap-chain processing thread
    m_ProcessingThread.reset(
        new SwapChainProcessor(SwapChain, std::move(Device), NewFrameEvent));
  }
}

void IndirectMonitorContext::UnassignSwapChain() {
  // Stop processing the last swap-chain
  m_ProcessingThread.reset();
}

NTSTATUS IndirectMonitorContext::Attach() {
  IDARG_OUT_MONITORARRIVAL ArrivalOut;
  return IddCxMonitorArrival(m_Monitor, &ArrivalOut);
}

NTSTATUS IndirectMonitorContext::Detach() {
  return IddCxMonitorDeparture(m_Monitor);
}
}  // namespace display::test

#pragma endregion

#pragma region DDI Callbacks

_Use_decl_annotations_ NTSTATUS
EvtIddCxAdapterInitFinished(IDDCX_ADAPTER AdapterObject,
                            const IDARG_IN_ADAPTER_INIT_FINISHED* pInArgs) {
  // This is called when the OS has finished setting up the adapter for use by
  // the IddCx driver. It's now possible to report attached monitors.

  auto* pDeviceContextWrapper =
      WdfObjectGet_IndirectDeviceContextWrapper(AdapterObject);
  if (NT_SUCCESS(pInArgs->AdapterInitStatus)) {
    pDeviceContextWrapper->pContext->FinishInit();
  }

  return STATUS_SUCCESS;
}

_Use_decl_annotations_ NTSTATUS
EvtIddCxAdapterCommitModes(IDDCX_ADAPTER AdapterObject,
                           const IDARG_IN_COMMITMODES* pInArgs) {
  UNREFERENCED_PARAMETER(AdapterObject);
  UNREFERENCED_PARAMETER(pInArgs);

  // Do nothing when modes are picked - the swap-chain is taken
  // care of by IddCx

  // ==============================
  // TODO: In a real driver, this function would be used to reconfigure the
  // device to commit the new modes. Loop through pInArgs->pPaths and look for
  // IDDCX_PATH_FLAGS_ACTIVE. Any path not active is inactive (e.g. the monitor
  // should be turned off).
  // ==============================

  return STATUS_SUCCESS;
}

_Use_decl_annotations_ NTSTATUS
EvtIddCxParseMonitorDescription(const IDARG_IN_PARSEMONITORDESCRIPTION* pInArgs,
                                IDARG_OUT_PARSEMONITORDESCRIPTION* pOutArgs) {
  // ==============================
  // TODO: In a real driver, this function would be called to generate monitor
  // modes for an EDID by parsing it. In this driver, the client's requested
  // mode for each virtual display is encoded in the first timing entry of the
  // EDID.
  // ==============================
  TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
              "Inside ParseMonitorDescription");

  pOutArgs->MonitorModeBufferOutputCount =
      display::test::IndirectMonitor::kModeListLength;

  if (pInArgs->MonitorModeBufferInputCount <
      display::test::IndirectMonitor::kModeListLength) {
    // Return success if there was no buffer, since the caller was only asking
    // for a count of modes
    return (pInArgs->MonitorModeBufferInputCount > 0) ? STATUS_BUFFER_TOO_SMALL
                                                      : STATUS_SUCCESS;
  } else {
    if (pInArgs->MonitorDescription.DataSize !=
        display::test::Edid::kBlockSize) {
      return STATUS_INVALID_PARAMETER;
    }

    display::test::Edid edid(
        reinterpret_cast<unsigned char*>(pInArgs->MonitorDescription.pData));

    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
                "Making the displays requested: %ld, %ld, %ld",
                edid.GetTimingEntry(0)->GetWidth(),
                edid.GetTimingEntry(0)->GetHeight(),
                edid.GetTimingEntry(0)->GetVerticalFrequency());

    pInArgs->pMonitorModes[0] = display::test::CreateIddCxMonitorMode(
        edid.GetTimingEntry(0)->GetWidth(), edid.GetTimingEntry(0)->GetHeight(),
        edid.GetTimingEntry(0)->GetVerticalFrequency(),
        IDDCX_MONITOR_MODE_ORIGIN_MONITORDESCRIPTOR);
    pOutArgs->PreferredMonitorModeIdx = 0;  // Always prefer the first mode.
    return STATUS_SUCCESS;
  }
}

_Use_decl_annotations_ NTSTATUS EvtIddCxMonitorGetDefaultModes(
    IDDCX_MONITOR MonitorObject,
    const IDARG_IN_GETDEFAULTDESCRIPTIONMODES* pInArgs,
    IDARG_OUT_GETDEFAULTDESCRIPTIONMODES* pOutArgs) {
  // ==============================
  // TODO: In a real driver, this function would be called to generate monitor
  // modes for a monitor with no EDID. Drivers should report modes that are
  // guaranteed to be supported by the transport protocol and by nearly all
  // monitors (such 640x480, 800x600, or 1024x768). If the driver has access to
  // monitor modes from a descriptor other than an EDID, those modes would also
  // be reported here.
  // ==============================

  // TODO: Remove or simplify this code, if it is not needed

  auto* pMonitorContextWrapper =
      WdfObjectGet_IndirectMonitorContextWrapper(MonitorObject);
  auto& monitor_config_list =
      pMonitorContextWrapper->pContext->monitor_config().pConfigList;
  pOutArgs->DefaultMonitorModeBufferOutputCount =
      static_cast<UINT>(monitor_config_list.size());
  if (pInArgs->DefaultMonitorModeBufferInputCount != 0) {
    for (DWORD i = 0;
         i < std::min(pInArgs->DefaultMonitorModeBufferInputCount,
                      pOutArgs->DefaultMonitorModeBufferOutputCount);
         i++) {
      const display::test::MonitorConfig config = monitor_config_list[i];
      TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
                  "Making the default modes: %hu, %hu, %hu", config.width(),
                  config.height(), config.v_sync());
      pInArgs->pDefaultMonitorModes[i] = display::test::CreateIddCxMonitorMode(
          config.width(), config.height(), config.v_sync(),
          IDDCX_MONITOR_MODE_ORIGIN_DRIVER);
    }
    pOutArgs->PreferredMonitorModeIdx = 0;
  }

  return STATUS_SUCCESS;
}

_Use_decl_annotations_ NTSTATUS
EvtIddCxMonitorQueryModes(IDDCX_MONITOR MonitorObject,
                          const IDARG_IN_QUERYTARGETMODES* pInArgs,
                          IDARG_OUT_QUERYTARGETMODES* pOutArgs) {
  UNREFERENCED_PARAMETER(MonitorObject);
  std::vector<IDDCX_TARGET_MODE> TargetModes;

  // Create a set of modes supported for frame processing and scan-out. These
  // are typically not based on the monitor's descriptor and instead are based
  // on the static processing capability of the device. The OS will report the
  // available set of modes for a given output as the intersection of monitor
  // modes with target modes.

  TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "MonitorQueryModes");

  auto* pMonitorContextWrapper =
      WdfObjectGet_IndirectMonitorContextWrapper(MonitorObject);
  for (auto mode :
       pMonitorContextWrapper->pContext->monitor_config().pConfigList) {
    TargetModes.push_back(display::test::CreateIddCxTargetMode(
        mode.width(), mode.height(), mode.v_sync()));
  }

  pOutArgs->TargetModeBufferOutputCount = static_cast<UINT>(TargetModes.size());

  if (pInArgs->TargetModeBufferInputCount >= TargetModes.size()) {
    copy(TargetModes.begin(), TargetModes.end(), pInArgs->pTargetModes);
  }

  return STATUS_SUCCESS;
}

_Use_decl_annotations_ NTSTATUS
EvtIddCxMonitorAssignSwapChain(IDDCX_MONITOR MonitorObject,
                               const IDARG_IN_SETSWAPCHAIN* pInArgs) {
  auto* pMonitorContextWrapper =
      WdfObjectGet_IndirectMonitorContextWrapper(MonitorObject);
  pMonitorContextWrapper->pContext->AssignSwapChain(
      pInArgs->hSwapChain, pInArgs->RenderAdapterLuid,
      pInArgs->hNextSurfaceAvailable);
  return STATUS_SUCCESS;
}

_Use_decl_annotations_ NTSTATUS
EvtIddCxMonitorUnassignSwapChain(IDDCX_MONITOR MonitorObject) {
  auto* pMonitorContextWrapper =
      WdfObjectGet_IndirectMonitorContextWrapper(MonitorObject);
  pMonitorContextWrapper->pContext->UnassignSwapChain();
  return STATUS_SUCCESS;
}

#pragma endregion
