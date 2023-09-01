// Copyright (c) Microsoft Corporation

#include "Driver.h"
#include "Driver.tmh"

#include "Direct3DDevice.h"
#include "Edid.h"
#include "HelperMethods.h"
#include "IndirectMonitor.h"
#include "SwapChainProcessor.h"
#include "public/properties.h"

#pragma region SampleMonitors

namespace {

static constexpr DWORD IDD_SAMPLE_MONITOR_COUNT = 3;

// Default modes reported for edid-less monitors. The first mode is set as
// preferred
std::vector<Windows::IndirectSampleMonitor::SampleMonitorMode>
    s_SampleDefaultModes = {{1920, 1080, 60}, {1600, 900, 60}, {1024, 768, 75}};

// FOR SAMPLE PURPOSES ONLY, Static info about monitors that will be reported to
// OS
std::vector<Windows::IndirectSampleMonitor> s_SampleMonitors = {
    // Modified EDID from Dell S2719DGF
    {{0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x10, 0xAC, 0xE6, 0xD0,
      0x55, 0x5A, 0x4A, 0x30, 0x24, 0x1D, 0x01, 0x04, 0xA5, 0x3C, 0x22, 0x78,
      0xFB, 0x6C, 0xE5, 0xA5, 0x55, 0x50, 0xA0, 0x23, 0x0B, 0x50, 0x54, 0x00,
      0x02, 0x00, 0xD1, 0xC0, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
      0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x58, 0xE3, 0x00, 0xA0, 0xA0, 0xA0,
      0x29, 0x50, 0x30, 0x20, 0x35, 0x00, 0x55, 0x50, 0x21, 0x00, 0x00, 0x1A,
      0x00, 0x00, 0x00, 0xFF, 0x00, 0x37, 0x4A, 0x51, 0x58, 0x42, 0x59, 0x32,
      0x0A, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x00, 0x00, 0xFC, 0x00, 0x53,
      0x32, 0x37, 0x31, 0x39, 0x44, 0x47, 0x46, 0x0A, 0x20, 0x20, 0x20, 0x20,
      0x00, 0x00, 0x00, 0xFD, 0x00, 0x28, 0x9B, 0xFA, 0xFA, 0x40, 0x01, 0x0A,
      0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x00, 0x2C},
     {
         {2560, 1440, 144},
         {1920, 1080, 60},
         {1024, 768, 60},
     }},
    // Modified EDID from Lenovo Y27fA
    {{0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x00, 0x30, 0xAE, 0xBF, 0x65,
      0x01, 0x01, 0x01, 0x01, 0x20, 0x1A, 0x01, 0x04, 0xA5, 0x3C, 0x22, 0x78,
      0x3B, 0xEE, 0xD1, 0xA5, 0x55, 0x48, 0x9B, 0x26, 0x12, 0x50, 0x54, 0x00,
      0x08, 0x00, 0xA9, 0xC0, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
      0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x68, 0xD8, 0x00, 0x18, 0xF1, 0x70,
      0x2D, 0x80, 0x58, 0x2C, 0x45, 0x00, 0x53, 0x50, 0x21, 0x00, 0x00, 0x1E,
      0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFD, 0x00, 0x30,
      0x92, 0xB4, 0xB4, 0x22, 0x01, 0x0A, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
      0x00, 0x00, 0x00, 0xFC, 0x00, 0x4C, 0x45, 0x4E, 0x20, 0x59, 0x32, 0x37,
      0x66, 0x41, 0x0A, 0x20, 0x20, 0x20, 0x00, 0x11},
     {
         {3840, 2160, 60},
         {1600, 900, 60},
         {1024, 768, 60},
     }}};
}  // namespace

#pragma endregion

#pragma region EventDeclaration

extern "C" DRIVER_INITIALIZE DriverEntry;

EVT_WDF_DRIVER_DEVICE_ADD IddSampleDeviceAdd;
EVT_WDF_DEVICE_D0_ENTRY IddSampleDeviceD0Entry;

EVT_IDD_CX_ADAPTER_INIT_FINISHED IddSampleAdapterInitFinished;
EVT_IDD_CX_ADAPTER_COMMIT_MODES IddSampleAdapterCommitModes;

EVT_IDD_CX_PARSE_MONITOR_DESCRIPTION IddSampleParseMonitorDescription;
EVT_IDD_CX_MONITOR_GET_DEFAULT_DESCRIPTION_MODES
IddSampleMonitorGetDefaultModes;
EVT_IDD_CX_MONITOR_QUERY_TARGET_MODES IddSampleMonitorQueryModes;

EVT_IDD_CX_MONITOR_ASSIGN_SWAPCHAIN IddSampleMonitorAssignSwapChain;
EVT_IDD_CX_MONITOR_UNASSIGN_SWAPCHAIN IddSampleMonitorUnassignSwapChain;

struct IndirectDeviceContextWrapper {
  Windows::IndirectDeviceContext* pContext;

  void Cleanup() {
    delete pContext;
    pContext = nullptr;
  }
};

struct IndirectMonitorContextWrapper {
  Windows::IndirectMonitorContext* pContext;

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

  WDF_DRIVER_CONFIG_INIT(&Config, IddSampleDeviceAdd);

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
IddSampleDeviceAdd(WDFDRIVER Driver, PWDFDEVICE_INIT pDeviceInit) {
  TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "IddSampleDeviceAdd");
  NTSTATUS Status = STATUS_SUCCESS;
  WDF_PNPPOWER_EVENT_CALLBACKS PnpPowerCallbacks;

  UNREFERENCED_PARAMETER(Driver);

  // Register for power callbacks - in this sample only power-on is needed
  WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&PnpPowerCallbacks);
  PnpPowerCallbacks.EvtDeviceD0Entry = IddSampleDeviceD0Entry;
  WdfDeviceInitSetPnpPowerEventCallbacks(pDeviceInit, &PnpPowerCallbacks);

  IDD_CX_CLIENT_CONFIG IddConfig;
  IDD_CX_CLIENT_CONFIG_INIT(&IddConfig);

  // If the driver wishes to handle custom IoDeviceControl requests, it's
  // necessary to use this callback since IddCx redirects IoDeviceControl
  // requests to an internal queue. This sample does not need this.
  // IddConfig.EvtIddCxDeviceIoControl = IddSampleIoDeviceControl;

  IddConfig.EvtIddCxAdapterInitFinished = IddSampleAdapterInitFinished;

  IddConfig.EvtIddCxParseMonitorDescription = IddSampleParseMonitorDescription;
  IddConfig.EvtIddCxMonitorGetDefaultDescriptionModes =
      IddSampleMonitorGetDefaultModes;
  IddConfig.EvtIddCxMonitorQueryTargetModes = IddSampleMonitorQueryModes;
  IddConfig.EvtIddCxAdapterCommitModes = IddSampleAdapterCommitModes;
  IddConfig.EvtIddCxMonitorAssignSwapChain = IddSampleMonitorAssignSwapChain;
  IddConfig.EvtIddCxMonitorUnassignSwapChain =
      IddSampleMonitorUnassignSwapChain;

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
  pContext->pContext = new Windows::IndirectDeviceContext(Device);

  // Read the properties structure sent from the client code that created
  // the software device.
  // TODO(crbug.com/1034772): Expand these properties and act on them to
  // control the displays created.
  WDF_DEVICE_PROPERTY_DATA propertyRead;
  WDF_DEVICE_PROPERTY_DATA_INIT(&propertyRead, &DisplayConfigurationProperty);
  propertyRead.Lcid = LOCALE_NEUTRAL;
  propertyRead.Flags = PLUGPLAY_PROPERTY_PERSISTENT;
  DriverProperties ConfiguredProperties(0);
  ULONG requiredSize = 0;
  DEVPROPTYPE propType;
  Status =
      WdfDeviceQueryPropertyEx(Device, &propertyRead, sizeof(DriverProperties),
                               &ConfiguredProperties, &requiredSize, &propType);
  if (!NT_SUCCESS(Status)) {
    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,
                "WdfDeviceQueryPropertyEx failed: %!STATUS!", Status);
    return Status;
  }
  TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "num_displays: %i",
              ConfiguredProperties.num_displays);

  return Status;
}

_Use_decl_annotations_ NTSTATUS
IddSampleDeviceD0Entry(WDFDEVICE Device, WDF_POWER_DEVICE_STATE PreviousState) {
  UNREFERENCED_PARAMETER(PreviousState);

  // This function is called by WDF to start the device in the fully-on power
  // state.

  auto* pContext = WdfObjectGet_IndirectDeviceContextWrapper(Device);
  pContext->pContext->InitAdapter();

  return STATUS_SUCCESS;
}

#pragma endregion

#pragma region IndirectContext

namespace Windows {
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
  AdapterCaps.MaxMonitorsSupported = IDD_SAMPLE_MONITOR_COUNT;
  AdapterCaps.EndPointDiagnostics.Size =
      sizeof(AdapterCaps.EndPointDiagnostics);
  AdapterCaps.EndPointDiagnostics.GammaSupport =
      IDDCX_FEATURE_IMPLEMENTATION_NONE;
  AdapterCaps.EndPointDiagnostics.TransmissionType =
      IDDCX_TRANSMISSION_TYPE_WIRED_OTHER;

  // Declare your device strings for telemetry (required)
  AdapterCaps.EndPointDiagnostics.pEndPointFriendlyName = L"IddSample Device";
  AdapterCaps.EndPointDiagnostics.pEndPointManufacturerName = L"Microsoft";
  AdapterCaps.EndPointDiagnostics.pEndPointModelName = L"IddSample Model";

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

  // In the sample driver, we report a monitor right away but a real driver
  // would do this when a monitor connection event occurs
  IDDCX_MONITOR_INFO MonitorInfo = {};
  MonitorInfo.Size = sizeof(MonitorInfo);
  MonitorInfo.MonitorType = DISPLAYCONFIG_OUTPUT_TECHNOLOGY_HDMI;
  MonitorInfo.ConnectorIndex = ConnectorIndex;

  MonitorInfo.MonitorDescription.Size = sizeof(MonitorInfo.MonitorDescription);
  MonitorInfo.MonitorDescription.Type = IDDCX_MONITOR_DESCRIPTION_TYPE_EDID;
  if (ConnectorIndex >= s_SampleMonitors.size()) {
    MonitorInfo.MonitorDescription.DataSize = 0;
    MonitorInfo.MonitorDescription.pData = nullptr;
  } else {
    MonitorInfo.MonitorDescription.DataSize = Windows::Edid::kBlockSize;
    MonitorInfo.MonitorDescription.pData =
        (s_SampleMonitors[ConnectorIndex].pEdidBlock);
  }

  // ==============================
  // TODO: The monitor's container ID should be distinct from "this" device's
  // container ID if the monitor is not permanently attached to the display
  // adapter device object. The container ID is typically made unique for each
  // monitor and can be used to associate the monitor with other devices, like
  // audio or input devices. In this sample we generate a random container ID
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
}  // namespace Windows

#pragma endregion

#pragma region DDI Callbacks

_Use_decl_annotations_ NTSTATUS
IddSampleAdapterInitFinished(IDDCX_ADAPTER AdapterObject,
                             const IDARG_IN_ADAPTER_INIT_FINISHED* pInArgs) {
  // This is called when the OS has finished setting up the adapter for use by
  // the IddCx driver. It's now possible to report attached monitors.

  auto* pDeviceContextWrapper =
      WdfObjectGet_IndirectDeviceContextWrapper(AdapterObject);
  if (NT_SUCCESS(pInArgs->AdapterInitStatus)) {
    for (DWORD i = 0; i < IDD_SAMPLE_MONITOR_COUNT; i++) {
      pDeviceContextWrapper->pContext->FinishInit(i);
    }
  }

  return STATUS_SUCCESS;
}

_Use_decl_annotations_ NTSTATUS
IddSampleAdapterCommitModes(IDDCX_ADAPTER AdapterObject,
                            const IDARG_IN_COMMITMODES* pInArgs) {
  UNREFERENCED_PARAMETER(AdapterObject);
  UNREFERENCED_PARAMETER(pInArgs);

  // For the sample, do nothing when modes are picked - the swap-chain is taken
  // care of by IddCx

  // ==============================
  // TODO: In a real driver, this function would be used to reconfigure the
  // device to commit the new modes. Loop through pInArgs->pPaths and look for
  // IDDCX_PATH_FLAGS_ACTIVE. Any path not active is inactive (e.g. the monitor
  // should be turned off).
  // ==============================

  return STATUS_SUCCESS;
}

_Use_decl_annotations_ NTSTATUS IddSampleParseMonitorDescription(
    const IDARG_IN_PARSEMONITORDESCRIPTION* pInArgs,
    IDARG_OUT_PARSEMONITORDESCRIPTION* pOutArgs) {
  // ==============================
  // TODO: In a real driver, this function would be called to generate monitor
  // modes for an EDID by parsing it. In this sample driver, we hard-code the
  // EDID, so this function can generate known modes.
  // ==============================

  pOutArgs->MonitorModeBufferOutputCount =
      Windows::IndirectSampleMonitor::szModeList;

  if (pInArgs->MonitorModeBufferInputCount <
      Windows::IndirectSampleMonitor::szModeList) {
    // Return success if there was no buffer, since the caller was only asking
    // for a count of modes
    return (pInArgs->MonitorModeBufferInputCount > 0) ? STATUS_BUFFER_TOO_SMALL
                                                      : STATUS_SUCCESS;
  } else {
    // In the sample driver, we have reported some static information about
    // connected monitors Check which of the reported monitors this call is for
    // by comparing it to the pointer of our known EDID blocks.

    if (pInArgs->MonitorDescription.DataSize != Windows::Edid::kBlockSize) {
      return STATUS_INVALID_PARAMETER;
    }

    for (const auto& monitor : s_SampleMonitors) {
      if (memcmp(pInArgs->MonitorDescription.pData, monitor.pEdidBlock,
                 Windows::Edid::kBlockSize) == 0) {
        // Copy the known modes to the output buffer
        for (DWORD ModeIndex = 0;
             ModeIndex < Windows::IndirectSampleMonitor::szModeList;
             ModeIndex++) {
          pInArgs->pMonitorModes[ModeIndex] =
              Windows::Methods::CreateIddCxMonitorMode(
                  monitor.pModeList[ModeIndex].Width,
                  monitor.pModeList[ModeIndex].Height,
                  monitor.pModeList[ModeIndex].VSync,
                  IDDCX_MONITOR_MODE_ORIGIN_MONITORDESCRIPTOR);
        }

        // Set the preferred mode as represented in the EDID
        pOutArgs->PreferredMonitorModeIdx = monitor.ulPreferredModeIdx;

        return STATUS_SUCCESS;
      }
    }

    // This EDID block does not belong to the monitors we reported earlier
    return STATUS_INVALID_PARAMETER;
  }
}

_Use_decl_annotations_ NTSTATUS IddSampleMonitorGetDefaultModes(
    IDDCX_MONITOR MonitorObject,
    const IDARG_IN_GETDEFAULTDESCRIPTIONMODES* pInArgs,
    IDARG_OUT_GETDEFAULTDESCRIPTIONMODES* pOutArgs) {
  UNREFERENCED_PARAMETER(MonitorObject);

  // ==============================
  // TODO: In a real driver, this function would be called to generate monitor
  // modes for a monitor with no EDID. Drivers should report modes that are
  // guaranteed to be supported by the transport protocol and by nearly all
  // monitors (such 640x480, 800x600, or 1024x768). If the driver has access to
  // monitor modes from a descriptor other than an EDID, those modes would also
  // be reported here.
  // ==============================

  if (pInArgs->DefaultMonitorModeBufferInputCount == 0) {
    pOutArgs->DefaultMonitorModeBufferOutputCount =
        static_cast<UINT>(s_SampleDefaultModes.size());
  } else {
    for (DWORD ModeIndex = 0; ModeIndex < s_SampleDefaultModes.size();
         ModeIndex++) {
      pInArgs->pDefaultMonitorModes[ModeIndex] =
          Windows::Methods::CreateIddCxMonitorMode(
              s_SampleDefaultModes[ModeIndex].Width,
              s_SampleDefaultModes[ModeIndex].Height,
              s_SampleDefaultModes[ModeIndex].VSync,
              IDDCX_MONITOR_MODE_ORIGIN_DRIVER);
    }

    pOutArgs->DefaultMonitorModeBufferOutputCount =
        static_cast<UINT>(s_SampleDefaultModes.size());
    pOutArgs->PreferredMonitorModeIdx = 0;
  }

  return STATUS_SUCCESS;
}

_Use_decl_annotations_ NTSTATUS
IddSampleMonitorQueryModes(IDDCX_MONITOR MonitorObject,
                           const IDARG_IN_QUERYTARGETMODES* pInArgs,
                           IDARG_OUT_QUERYTARGETMODES* pOutArgs) {
  UNREFERENCED_PARAMETER(MonitorObject);

  std::vector<IDDCX_TARGET_MODE> TargetModes;

  // Create a set of modes supported for frame processing and scan-out. These
  // are typically not based on the monitor's descriptor and instead are based
  // on the static processing capability of the device. The OS will report the
  // available set of modes for a given output as the intersection of monitor
  // modes with target modes.

  TargetModes.push_back(
      Windows::Methods::CreateIddCxTargetMode(3840, 2160, 60));
  TargetModes.push_back(
      Windows::Methods::CreateIddCxTargetMode(2560, 1440, 144));
  TargetModes.push_back(
      Windows::Methods::CreateIddCxTargetMode(2560, 1440, 90));
  TargetModes.push_back(
      Windows::Methods::CreateIddCxTargetMode(2560, 1440, 60));
  TargetModes.push_back(
      Windows::Methods::CreateIddCxTargetMode(1920, 1080, 144));
  TargetModes.push_back(
      Windows::Methods::CreateIddCxTargetMode(1920, 1080, 90));
  TargetModes.push_back(
      Windows::Methods::CreateIddCxTargetMode(1920, 1080, 60));
  TargetModes.push_back(Windows::Methods::CreateIddCxTargetMode(1600, 900, 60));
  TargetModes.push_back(Windows::Methods::CreateIddCxTargetMode(1024, 768, 75));
  TargetModes.push_back(Windows::Methods::CreateIddCxTargetMode(1024, 768, 60));

  pOutArgs->TargetModeBufferOutputCount = static_cast<UINT>(TargetModes.size());

  if (pInArgs->TargetModeBufferInputCount >= TargetModes.size()) {
    copy(TargetModes.begin(), TargetModes.end(), pInArgs->pTargetModes);
  }

  return STATUS_SUCCESS;
}

_Use_decl_annotations_ NTSTATUS
IddSampleMonitorAssignSwapChain(IDDCX_MONITOR MonitorObject,
                                const IDARG_IN_SETSWAPCHAIN* pInArgs) {
  auto* pMonitorContextWrapper =
      WdfObjectGet_IndirectMonitorContextWrapper(MonitorObject);
  pMonitorContextWrapper->pContext->AssignSwapChain(
      pInArgs->hSwapChain, pInArgs->RenderAdapterLuid,
      pInArgs->hNextSurfaceAvailable);
  return STATUS_SUCCESS;
}

_Use_decl_annotations_ NTSTATUS
IddSampleMonitorUnassignSwapChain(IDDCX_MONITOR MonitorObject) {
  auto* pMonitorContextWrapper =
      WdfObjectGet_IndirectMonitorContextWrapper(MonitorObject);
  pMonitorContextWrapper->pContext->UnassignSwapChain();
  return STATUS_SUCCESS;
}

#pragma endregion
