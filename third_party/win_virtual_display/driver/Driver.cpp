// Copyright (c) Microsoft Corporation

#include "Driver.h"
#include "Driver.tmh"

#include "Direct3DDevice.h"
#include "Edid.h"
#include "HelperMethods.h"
#include "IndirectMonitor.h"
#include "SwapChainProcessor.h"
#include "public/properties.h"

#include <algorithm>

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

  void Cleanup() {
    delete pContext;
    pContext = nullptr;
  }
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

  // Read the properties structure sent from the client code that created
  // the software device.
  WDF_DEVICE_PROPERTY_DATA propertyRead;
  WDF_DEVICE_PROPERTY_DATA_INIT(&propertyRead, &DisplayConfigurationProperty);
  propertyRead.Lcid = LOCALE_NEUTRAL;
  propertyRead.Flags = PLUGPLAY_PROPERTY_PERSISTENT;
  DriverProperties driver_properties;
  ULONG requiredSize = 0;
  DEVPROPTYPE propType;
  Status =
      WdfDeviceQueryPropertyEx(Device, &propertyRead, sizeof(DriverProperties),
                               &driver_properties, &requiredSize, &propType);
  if (!NT_SUCCESS(Status)) {
    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
                "WdfDeviceQueryPropertyEx failed: %!STATUS!", Status);
    return Status;
  }

  TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "num_displays: %d",
              driver_properties.monitor_count);

  for (int i = 0; i < driver_properties.monitor_count; i++) {
    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Checking for display #%d", i);
    display::test::IndirectMonitor indirect_monitor;
    auto mode = driver_properties.requested_modes[i];
    display::test::Edid edid(indirect_monitor.pEdidBlock.data());
    bool success =
        edid.GetTimingEntry(0)->SetMode(mode.width, mode.height, mode.vSync);
    if (!success) {
      TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "SetMode() unsuccessful");
    }
    edid.UpdateChecksum();
    indirect_monitor.pEdidBlock = edid.getEdidBlock();
    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
                "Width (Modified EDID,Chosen Mode) (Inside "
                "EvtWdfDriverDeviceAdd): %ld, %hu",
                edid.GetTimingEntry(0)->GetWidth(), mode.width);
    indirect_monitor.pModeList.push_back(mode);
    pContext->pContext->monitors.push_back(indirect_monitor);
  }

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

IndirectDeviceContext::~IndirectDeviceContext() {}

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

void IndirectDeviceContext::FinishInit(UINT ConnectorIndex) {
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
  MonitorInfo.ConnectorIndex = ConnectorIndex;

  MonitorInfo.MonitorDescription.Size = sizeof(MonitorInfo.MonitorDescription);
  MonitorInfo.MonitorDescription.Type = IDDCX_MONITOR_DESCRIPTION_TYPE_EDID;
  if (ConnectorIndex >= monitors.size()) {
    MonitorInfo.MonitorDescription.DataSize = 0;
    MonitorInfo.MonitorDescription.pData = nullptr;
  } else {
    MonitorInfo.MonitorDescription.DataSize = Edid::kBlockSize;
    MonitorInfo.MonitorDescription.pData =
        (monitors[ConnectorIndex].pEdidBlock.data());
  }

  TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "ConnectorIndex: %d",
              ConnectorIndex);

  display::test::Edid edid1(monitors[ConnectorIndex].pEdidBlock.data());
  TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
              "Width (Modified EDID) (Inside FinishInit): %ld",
              edid1.GetTimingEntry(0)->GetWidth());

  // ==============================
  // TODO: The monitor's container ID should be distinct from "this" device's
  // container ID if the monitor is not permanently attached to the display
  // adapter device object. The container ID is typically made unique for each
  // monitor and can be used to associate the monitor with other devices, like
  // audio or input devices. In this case we generate a random container ID
  // GUID, but it's best practice to choose a stable container ID for a unique
  // monitor or to use "this" device's container ID for a permanent/integrated
  // monitor.
  // ==============================

  // Create a container ID
  CoCreateGuid(&MonitorInfo.MonitorContainerId);

  IDARG_IN_MONITORCREATE MonitorCreate = {};
  MonitorCreate.ObjectAttributes = &Attr;
  MonitorCreate.pMonitorInfo = &MonitorInfo;

  // Create a monitor object with the specified monitor descriptor
  IDARG_OUT_MONITORCREATE MonitorCreateOut;
  NTSTATUS Status =
      IddCxMonitorCreate(m_Adapter, &MonitorCreate, &MonitorCreateOut);
  if (NT_SUCCESS(Status)) {
    // Create a new monitor context object and attach it to the Idd monitor
    // object
    auto* pMonitorContextWrapper = WdfObjectGet_IndirectMonitorContextWrapper(
        MonitorCreateOut.MonitorObject);
    pMonitorContextWrapper->pContext =
        new IndirectMonitorContext(MonitorCreateOut.MonitorObject);

    if (ConnectorIndex < monitors.size()) {
      pMonitorContextWrapper->pContext->default_mode_list =
          monitors[ConnectorIndex].pModeList;
    }

    // Tell the OS that the monitor has been plugged in
    IDARG_OUT_MONITORARRIVAL ArrivalOut;
    Status = IddCxMonitorArrival(MonitorCreateOut.MonitorObject, &ArrivalOut);
  }
}

IndirectMonitorContext::IndirectMonitorContext(_In_ IDDCX_MONITOR Monitor)
    : m_Monitor(Monitor) {}

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
    for (DWORD i = 0; i < pDeviceContextWrapper->pContext->monitors.size();
         i++) {
      pDeviceContextWrapper->pContext->FinishInit(i);
    }
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

    // TODO: Return STATUS_INVALID_PARAMETER for monitors not belonging to this
    // driver.
    bool success = false;
    for (size_t i = 0; i < DriverProperties::kSupportedModesCount; i++) {
      if (edid.GetTimingEntry(0)->GetWidth() ==
              DriverProperties::kSupportedModes[i].width &&
          edid.GetTimingEntry(0)->GetHeight() ==
              DriverProperties::kSupportedModes[i].height &&
          edid.GetTimingEntry(0)->GetVerticalFrequency() ==
              DriverProperties::kSupportedModes[i].vSync) {
        success = true;
        break;
      }
    }
    if (!success) {
      return STATUS_INVALID_PARAMETER;
    }

    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
                "Making the displays requested: %ld, %ld, %ld",
                edid.GetTimingEntry(0)->GetWidth(),
                edid.GetTimingEntry(0)->GetHeight(),
                edid.GetTimingEntry(0)->GetVerticalFrequency());

    pInArgs->pMonitorModes[0] = display::test::Methods::CreateIddCxMonitorMode(
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

  if (pInArgs->DefaultMonitorModeBufferInputCount == 0) {
    pOutArgs->DefaultMonitorModeBufferOutputCount = static_cast<UINT>(
        pMonitorContextWrapper->pContext->default_mode_list.size());
  } else {
    for (DWORD ModeIndex = 0;
         ModeIndex < pMonitorContextWrapper->pContext->default_mode_list.size();
         ModeIndex++) {
      TraceEvents(
          TRACE_LEVEL_ERROR, TRACE_DRIVER,
          "Making the default modes: %hu, %hu, %hu",
          pMonitorContextWrapper->pContext->default_mode_list[ModeIndex].width,
          pMonitorContextWrapper->pContext->default_mode_list[ModeIndex].height,
          pMonitorContextWrapper->pContext->default_mode_list[ModeIndex].vSync);
      pInArgs->pDefaultMonitorModes
          [ModeIndex] = display::test::Methods::CreateIddCxMonitorMode(
          pMonitorContextWrapper->pContext->default_mode_list[ModeIndex].width,
          pMonitorContextWrapper->pContext->default_mode_list[ModeIndex].height,
          pMonitorContextWrapper->pContext->default_mode_list[ModeIndex].vSync,
          IDDCX_MONITOR_MODE_ORIGIN_DRIVER);
    }

    pOutArgs->DefaultMonitorModeBufferOutputCount = static_cast<UINT>(
        pMonitorContextWrapper->pContext->default_mode_list.size());
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

  TargetModes.push_back(
      display::test::Methods::CreateIddCxTargetMode(3840, 2160, 60));
  TargetModes.push_back(
      display::test::Methods::CreateIddCxTargetMode(2560, 1440, 144));
  TargetModes.push_back(
      display::test::Methods::CreateIddCxTargetMode(2560, 1440, 90));
  TargetModes.push_back(
      display::test::Methods::CreateIddCxTargetMode(2560, 1440, 60));
  TargetModes.push_back(
      display::test::Methods::CreateIddCxTargetMode(1920, 1080, 144));
  TargetModes.push_back(
      display::test::Methods::CreateIddCxTargetMode(1920, 1080, 90));
  TargetModes.push_back(
      display::test::Methods::CreateIddCxTargetMode(1920, 1080, 60));
  TargetModes.push_back(
      display::test::Methods::CreateIddCxTargetMode(1600, 900, 60));
  TargetModes.push_back(
      display::test::Methods::CreateIddCxTargetMode(1024, 768, 75));
  TargetModes.push_back(
      display::test::Methods::CreateIddCxTargetMode(1024, 768, 60));

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
