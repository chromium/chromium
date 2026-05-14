
#pragma warning( disable: 4049 )  /* more than 64k source lines */

/* verify that the <rpcndr.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCNDR_H_VERSION__
#define __REQUIRED_RPCNDR_H_VERSION__ 500
#endif

/* verify that the <rpcsal.h> version is high enough to compile this file*/
#ifndef __REQUIRED_RPCSAL_H_VERSION__
#define __REQUIRED_RPCSAL_H_VERSION__ 100
#endif

#include <rpc.h>
#include <rpcndr.h>

#ifndef __RPCNDR_H_VERSION__
#error this stub requires an updated version of <rpcndr.h>
#endif /* __RPCNDR_H_VERSION__ */

#ifndef COM_NO_WINDOWS_H
#include <windows.h>
#include <ole2.h>
#endif /*COM_NO_WINDOWS_H*/
#ifndef __microsoft2Ewindows2Eai2Emachinelearning_h__
#define __microsoft2Ewindows2Eai2Emachinelearning_h__
#ifndef __microsoft2Ewindows2Eai2Emachinelearning_p_h__
#define __microsoft2Ewindows2Eai2Emachinelearning_p_h__


#pragma once

// Ensure that the setting of the /ns_prefix command line switch is consistent for all headers.
// If you get an error from the compiler indicating "warning C4005: 'CHECK_NS_PREFIX_STATE': macro redefinition", this
// indicates that you have included two different headers with different settings for the /ns_prefix MIDL command line switch
#if !defined(DISABLE_NS_PREFIX_CHECKS)
#define CHECK_NS_PREFIX_STATE "always"
#endif // !defined(DISABLE_NS_PREFIX_CHECKS)


#pragma push_macro("MIDL_CONST_ID")
#undef MIDL_CONST_ID
#define MIDL_CONST_ID const __declspec(selectany)


//  API Contract Inclusion Definitions
#if !defined(SPECIFIC_API_CONTRACT_DEFINITIONS)
#if !defined(MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION)
#define MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION 0x20000
#endif // defined(MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION)

#if !defined(WINDOWS_APPLICATIONMODEL_STARTUPTASKCONTRACT_VERSION)
#define WINDOWS_APPLICATIONMODEL_STARTUPTASKCONTRACT_VERSION 0x30000
#endif // defined(WINDOWS_APPLICATIONMODEL_STARTUPTASKCONTRACT_VERSION)

#if !defined(WINDOWS_FOUNDATION_FOUNDATIONCONTRACT_VERSION)
#define WINDOWS_FOUNDATION_FOUNDATIONCONTRACT_VERSION 0x40000
#endif // defined(WINDOWS_FOUNDATION_FOUNDATIONCONTRACT_VERSION)

#if !defined(WINDOWS_FOUNDATION_UNIVERSALAPICONTRACT_VERSION)
#define WINDOWS_FOUNDATION_UNIVERSALAPICONTRACT_VERSION 0x130000
#endif // defined(WINDOWS_FOUNDATION_UNIVERSALAPICONTRACT_VERSION)

#endif // defined(SPECIFIC_API_CONTRACT_DEFINITIONS)


// Header files for imported files
#include "inspectable.h"
#include "AsyncInfo.h"
#include "EventToken.h"
#include "windowscontracts.h"
#include "Windows.Foundation.h"
#include "Windows.ApplicationModel.h"
// Importing Collections header
#include <windows.foundation.collections.h>

#if defined(__cplusplus) && !defined(CINTERFACE)
/* Forward Declarations */
#ifndef ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo_FWD_DEFINED__
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo_FWD_DEFINED__
namespace ABI {
    namespace Microsoft {
        namespace Windows {
            namespace AI {
                namespace MachineLearning {
                    interface ICatalogModelInfo;
                } /* MachineLearning */
            } /* AI */
        } /* Windows */
    } /* Microsoft */
} /* ABI */
#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo ABI::Microsoft::Windows::AI::MachineLearning::ICatalogModelInfo

#endif // ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo_FWD_DEFINED__

#ifndef ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstance_FWD_DEFINED__
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstance_FWD_DEFINED__
namespace ABI {
    namespace Microsoft {
        namespace Windows {
            namespace AI {
                namespace MachineLearning {
                    interface ICatalogModelInstance;
                } /* MachineLearning */
            } /* AI */
        } /* Windows */
    } /* Microsoft */
} /* ABI */
#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstance ABI::Microsoft::Windows::AI::MachineLearning::ICatalogModelInstance

#endif // ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstance_FWD_DEFINED__

#ifndef ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResult_FWD_DEFINED__
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResult_FWD_DEFINED__
namespace ABI {
    namespace Microsoft {
        namespace Windows {
            namespace AI {
                namespace MachineLearning {
                    interface ICatalogModelInstanceResult;
                } /* MachineLearning */
            } /* AI */
        } /* Windows */
    } /* Microsoft */
} /* ABI */
#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResult ABI::Microsoft::Windows::AI::MachineLearning::ICatalogModelInstanceResult

#endif // ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResult_FWD_DEFINED__

#ifndef ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider_FWD_DEFINED__
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider_FWD_DEFINED__
namespace ABI {
    namespace Microsoft {
        namespace Windows {
            namespace AI {
                namespace MachineLearning {
                    interface IExecutionProvider;
                } /* MachineLearning */
            } /* AI */
        } /* Windows */
    } /* Microsoft */
} /* ABI */
#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider ABI::Microsoft::Windows::AI::MachineLearning::IExecutionProvider

#endif // ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider_FWD_DEFINED__

#ifndef ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalog_FWD_DEFINED__
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalog_FWD_DEFINED__
namespace ABI {
    namespace Microsoft {
        namespace Windows {
            namespace AI {
                namespace MachineLearning {
                    interface IExecutionProviderCatalog;
                } /* MachineLearning */
            } /* AI */
        } /* Windows */
    } /* Microsoft */
} /* ABI */
#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalog ABI::Microsoft::Windows::AI::MachineLearning::IExecutionProviderCatalog

#endif // ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalog_FWD_DEFINED__

#ifndef ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalogStatics_FWD_DEFINED__
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalogStatics_FWD_DEFINED__
namespace ABI {
    namespace Microsoft {
        namespace Windows {
            namespace AI {
                namespace MachineLearning {
                    interface IExecutionProviderCatalogStatics;
                } /* MachineLearning */
            } /* AI */
        } /* Windows */
    } /* Microsoft */
} /* ABI */
#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalogStatics ABI::Microsoft::Windows::AI::MachineLearning::IExecutionProviderCatalogStatics

#endif // ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalogStatics_FWD_DEFINED__

#ifndef ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderReadyResult_FWD_DEFINED__
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderReadyResult_FWD_DEFINED__
namespace ABI {
    namespace Microsoft {
        namespace Windows {
            namespace AI {
                namespace MachineLearning {
                    interface IExecutionProviderReadyResult;
                } /* MachineLearning */
            } /* AI */
        } /* Windows */
    } /* Microsoft */
} /* ABI */
#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderReadyResult ABI::Microsoft::Windows::AI::MachineLearning::IExecutionProviderReadyResult

#endif // ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderReadyResult_FWD_DEFINED__

#ifndef ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog_FWD_DEFINED__
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog_FWD_DEFINED__
namespace ABI {
    namespace Microsoft {
        namespace Windows {
            namespace AI {
                namespace MachineLearning {
                    interface IModelCatalog;
                } /* MachineLearning */
            } /* AI */
        } /* Windows */
    } /* Microsoft */
} /* ABI */
#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog ABI::Microsoft::Windows::AI::MachineLearning::IModelCatalog

#endif // ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog_FWD_DEFINED__

#ifndef ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogFactory_FWD_DEFINED__
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogFactory_FWD_DEFINED__
namespace ABI {
    namespace Microsoft {
        namespace Windows {
            namespace AI {
                namespace MachineLearning {
                    interface IModelCatalogFactory;
                } /* MachineLearning */
            } /* AI */
        } /* Windows */
    } /* Microsoft */
} /* ABI */
#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogFactory ABI::Microsoft::Windows::AI::MachineLearning::IModelCatalogFactory

#endif // ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogFactory_FWD_DEFINED__

#ifndef ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource_FWD_DEFINED__
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource_FWD_DEFINED__
namespace ABI {
    namespace Microsoft {
        namespace Windows {
            namespace AI {
                namespace MachineLearning {
                    interface IModelCatalogSource;
                } /* MachineLearning */
            } /* AI */
        } /* Windows */
    } /* Microsoft */
} /* ABI */
#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource ABI::Microsoft::Windows::AI::MachineLearning::IModelCatalogSource

#endif // ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource_FWD_DEFINED__

#ifndef ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSourceStatics_FWD_DEFINED__
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSourceStatics_FWD_DEFINED__
namespace ABI {
    namespace Microsoft {
        namespace Windows {
            namespace AI {
                namespace MachineLearning {
                    interface IModelCatalogSourceStatics;
                } /* MachineLearning */
            } /* AI */
        } /* Windows */
    } /* Microsoft */
} /* ABI */
#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSourceStatics ABI::Microsoft::Windows::AI::MachineLearning::IModelCatalogSourceStatics

#endif // ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSourceStatics_FWD_DEFINED__

// Parameterized interface forward declarations (C++)

// Collection interface definitions
namespace ABI {
    namespace Microsoft {
        namespace Windows {
            namespace AI {
                namespace MachineLearning {
                    class CatalogModelInfo;
                } /* MachineLearning */
            } /* AI */
        } /* Windows */
    } /* Microsoft */
} /* ABI */

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#ifndef DEF___FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_USE
#define DEF___FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_USE
#if !defined(RO_NO_TEMPLATE_NAME)
namespace ABI { namespace Windows { namespace Foundation {
template <>
struct __declspec(uuid("88b61d2e-ea3e-5b47-bc5d-741650afda63"))
IAsyncOperation<ABI::Microsoft::Windows::AI::MachineLearning::CatalogModelInfo*> : IAsyncOperation_impl<ABI::Windows::Foundation::Internal::AggregateType<ABI::Microsoft::Windows::AI::MachineLearning::CatalogModelInfo*, ABI::Microsoft::Windows::AI::MachineLearning::ICatalogModelInfo*>>
{
    static const wchar_t* z_get_rc_name_impl()
    {
        return L"Windows.Foundation.IAsyncOperation`1<Microsoft.Windows.AI.MachineLearning.CatalogModelInfo>";
    }
};
// Define a typedef for the parameterized interface specialization's mangled name.
// This allows code which uses the mangled name for the parameterized interface to access the
// correct parameterized interface specialization.
typedef IAsyncOperation<ABI::Microsoft::Windows::AI::MachineLearning::CatalogModelInfo*> __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_t;
#define __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo ABI::Windows::Foundation::__FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_t
/* Foundation */ } /* Windows */ } /* ABI */ }

#endif // !defined(RO_NO_TEMPLATE_NAME)
#endif /* DEF___FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_USE */

#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#ifndef DEF___FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_USE
#define DEF___FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_USE
#if !defined(RO_NO_TEMPLATE_NAME)
namespace ABI { namespace Windows { namespace Foundation {
template <>
struct __declspec(uuid("5a72be24-3605-5b35-9cc3-6b5ebfe5c2c7"))
IAsyncOperationCompletedHandler<ABI::Microsoft::Windows::AI::MachineLearning::CatalogModelInfo*> : IAsyncOperationCompletedHandler_impl<ABI::Windows::Foundation::Internal::AggregateType<ABI::Microsoft::Windows::AI::MachineLearning::CatalogModelInfo*, ABI::Microsoft::Windows::AI::MachineLearning::ICatalogModelInfo*>>
{
    static const wchar_t* z_get_rc_name_impl()
    {
        return L"Windows.Foundation.AsyncOperationCompletedHandler`1<Microsoft.Windows.AI.MachineLearning.CatalogModelInfo>";
    }
};
// Define a typedef for the parameterized interface specialization's mangled name.
// This allows code which uses the mangled name for the parameterized interface to access the
// correct parameterized interface specialization.
typedef IAsyncOperationCompletedHandler<ABI::Microsoft::Windows::AI::MachineLearning::CatalogModelInfo*> __FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_t;
#define __FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo ABI::Windows::Foundation::__FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_t
/* Foundation */ } /* Windows */ } /* ABI */ }

#endif // !defined(RO_NO_TEMPLATE_NAME)
#endif /* DEF___FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_USE */

#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

namespace ABI {
    namespace Microsoft {
        namespace Windows {
            namespace AI {
                namespace MachineLearning {
                    class ModelCatalogSource;
                } /* MachineLearning */
            } /* AI */
        } /* Windows */
    } /* Microsoft */
} /* ABI */

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#ifndef DEF___FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_USE
#define DEF___FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_USE
#if !defined(RO_NO_TEMPLATE_NAME)
namespace ABI { namespace Windows { namespace Foundation {
template <>
struct __declspec(uuid("65eb0fb5-5af9-56f2-9556-8541c3db67bb"))
IAsyncOperation<ABI::Microsoft::Windows::AI::MachineLearning::ModelCatalogSource*> : IAsyncOperation_impl<ABI::Windows::Foundation::Internal::AggregateType<ABI::Microsoft::Windows::AI::MachineLearning::ModelCatalogSource*, ABI::Microsoft::Windows::AI::MachineLearning::IModelCatalogSource*>>
{
    static const wchar_t* z_get_rc_name_impl()
    {
        return L"Windows.Foundation.IAsyncOperation`1<Microsoft.Windows.AI.MachineLearning.ModelCatalogSource>";
    }
};
// Define a typedef for the parameterized interface specialization's mangled name.
// This allows code which uses the mangled name for the parameterized interface to access the
// correct parameterized interface specialization.
typedef IAsyncOperation<ABI::Microsoft::Windows::AI::MachineLearning::ModelCatalogSource*> __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_t;
#define __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource ABI::Windows::Foundation::__FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_t
/* Foundation */ } /* Windows */ } /* ABI */ }

#endif // !defined(RO_NO_TEMPLATE_NAME)
#endif /* DEF___FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_USE */

#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#ifndef DEF___FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_USE
#define DEF___FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_USE
#if !defined(RO_NO_TEMPLATE_NAME)
namespace ABI { namespace Windows { namespace Foundation {
template <>
struct __declspec(uuid("917fef8e-e0b8-5fee-9e66-5baaa9a841c9"))
IAsyncOperationCompletedHandler<ABI::Microsoft::Windows::AI::MachineLearning::ModelCatalogSource*> : IAsyncOperationCompletedHandler_impl<ABI::Windows::Foundation::Internal::AggregateType<ABI::Microsoft::Windows::AI::MachineLearning::ModelCatalogSource*, ABI::Microsoft::Windows::AI::MachineLearning::IModelCatalogSource*>>
{
    static const wchar_t* z_get_rc_name_impl()
    {
        return L"Windows.Foundation.AsyncOperationCompletedHandler`1<Microsoft.Windows.AI.MachineLearning.ModelCatalogSource>";
    }
};
// Define a typedef for the parameterized interface specialization's mangled name.
// This allows code which uses the mangled name for the parameterized interface to access the
// correct parameterized interface specialization.
typedef IAsyncOperationCompletedHandler<ABI::Microsoft::Windows::AI::MachineLearning::ModelCatalogSource*> __FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_t;
#define __FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource ABI::Windows::Foundation::__FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_t
/* Foundation */ } /* Windows */ } /* ABI */ }

#endif // !defined(RO_NO_TEMPLATE_NAME)
#endif /* DEF___FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_USE */

#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#ifndef DEF___FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_USE
#define DEF___FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_USE
#if !defined(RO_NO_TEMPLATE_NAME)
namespace ABI { namespace Windows { namespace Foundation { namespace Collections {
template <>
struct __declspec(uuid("1c1b27d0-fc9b-5a44-9746-1a617e48b153"))
IIterator<ABI::Microsoft::Windows::AI::MachineLearning::CatalogModelInfo*> : IIterator_impl<ABI::Windows::Foundation::Internal::AggregateType<ABI::Microsoft::Windows::AI::MachineLearning::CatalogModelInfo*, ABI::Microsoft::Windows::AI::MachineLearning::ICatalogModelInfo*>>
{
    static const wchar_t* z_get_rc_name_impl()
    {
        return L"Windows.Foundation.Collections.IIterator`1<Microsoft.Windows.AI.MachineLearning.CatalogModelInfo>";
    }
};
// Define a typedef for the parameterized interface specialization's mangled name.
// This allows code which uses the mangled name for the parameterized interface to access the
// correct parameterized interface specialization.
typedef IIterator<ABI::Microsoft::Windows::AI::MachineLearning::CatalogModelInfo*> __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_t;
#define __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo ABI::Windows::Foundation::Collections::__FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_t
/* Collections */ } /* Foundation */ } /* Windows */ } /* ABI */ }

#endif // !defined(RO_NO_TEMPLATE_NAME)
#endif /* DEF___FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_USE */

#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#ifndef DEF___FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_USE
#define DEF___FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_USE
#if !defined(RO_NO_TEMPLATE_NAME)
namespace ABI { namespace Windows { namespace Foundation { namespace Collections {
template <>
struct __declspec(uuid("7a270079-a95f-5a7e-8859-9df94e2c8a7d"))
IIterable<ABI::Microsoft::Windows::AI::MachineLearning::CatalogModelInfo*> : IIterable_impl<ABI::Windows::Foundation::Internal::AggregateType<ABI::Microsoft::Windows::AI::MachineLearning::CatalogModelInfo*, ABI::Microsoft::Windows::AI::MachineLearning::ICatalogModelInfo*>>
{
    static const wchar_t* z_get_rc_name_impl()
    {
        return L"Windows.Foundation.Collections.IIterable`1<Microsoft.Windows.AI.MachineLearning.CatalogModelInfo>";
    }
};
// Define a typedef for the parameterized interface specialization's mangled name.
// This allows code which uses the mangled name for the parameterized interface to access the
// correct parameterized interface specialization.
typedef IIterable<ABI::Microsoft::Windows::AI::MachineLearning::CatalogModelInfo*> __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_t;
#define __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo ABI::Windows::Foundation::Collections::__FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_t
/* Collections */ } /* Foundation */ } /* Windows */ } /* ABI */ }

#endif // !defined(RO_NO_TEMPLATE_NAME)
#endif /* DEF___FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_USE */

#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#ifndef DEF___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_USE
#define DEF___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_USE
#if !defined(RO_NO_TEMPLATE_NAME)
namespace ABI { namespace Windows { namespace Foundation { namespace Collections {
template <>
struct __declspec(uuid("d88a7cca-1865-56c0-bd1f-b6a043b82022"))
IVectorView<ABI::Microsoft::Windows::AI::MachineLearning::CatalogModelInfo*> : IVectorView_impl<ABI::Windows::Foundation::Internal::AggregateType<ABI::Microsoft::Windows::AI::MachineLearning::CatalogModelInfo*, ABI::Microsoft::Windows::AI::MachineLearning::ICatalogModelInfo*>>
{
    static const wchar_t* z_get_rc_name_impl()
    {
        return L"Windows.Foundation.Collections.IVectorView`1<Microsoft.Windows.AI.MachineLearning.CatalogModelInfo>";
    }
};
// Define a typedef for the parameterized interface specialization's mangled name.
// This allows code which uses the mangled name for the parameterized interface to access the
// correct parameterized interface specialization.
typedef IVectorView<ABI::Microsoft::Windows::AI::MachineLearning::CatalogModelInfo*> __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_t;
#define __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo ABI::Windows::Foundation::Collections::__FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_t
/* Collections */ } /* Foundation */ } /* Windows */ } /* ABI */ }

#endif // !defined(RO_NO_TEMPLATE_NAME)
#endif /* DEF___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_USE */

#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#ifndef DEF___FIAsyncOperation_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_USE
#define DEF___FIAsyncOperation_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_USE
#if !defined(RO_NO_TEMPLATE_NAME)
namespace ABI { namespace Windows { namespace Foundation {
template <>
struct __declspec(uuid("46c71e51-145c-5fbf-aac9-075db1fd3aa9"))
IAsyncOperation<__FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo*> : IAsyncOperation_impl<__FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo*>
{
    static const wchar_t* z_get_rc_name_impl()
    {
        return L"Windows.Foundation.IAsyncOperation`1<Windows.Foundation.Collections.IVectorView`1<Microsoft.Windows.AI.MachineLearning.CatalogModelInfo>>";
    }
};
// Define a typedef for the parameterized interface specialization's mangled name.
// This allows code which uses the mangled name for the parameterized interface to access the
// correct parameterized interface specialization.
typedef IAsyncOperation<__FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo*> __FIAsyncOperation_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_t;
#define __FIAsyncOperation_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo ABI::Windows::Foundation::__FIAsyncOperation_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_t
/* Foundation */ } /* Windows */ } /* ABI */ }

#endif // !defined(RO_NO_TEMPLATE_NAME)
#endif /* DEF___FIAsyncOperation_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_USE */

#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#ifndef DEF___FIAsyncOperationCompletedHandler_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_USE
#define DEF___FIAsyncOperationCompletedHandler_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_USE
#if !defined(RO_NO_TEMPLATE_NAME)
namespace ABI { namespace Windows { namespace Foundation {
template <>
struct __declspec(uuid("72270921-e7eb-5773-9fb7-5000064772e5"))
IAsyncOperationCompletedHandler<__FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo*> : IAsyncOperationCompletedHandler_impl<__FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo*>
{
    static const wchar_t* z_get_rc_name_impl()
    {
        return L"Windows.Foundation.AsyncOperationCompletedHandler`1<Windows.Foundation.Collections.IVectorView`1<Microsoft.Windows.AI.MachineLearning.CatalogModelInfo>>";
    }
};
// Define a typedef for the parameterized interface specialization's mangled name.
// This allows code which uses the mangled name for the parameterized interface to access the
// correct parameterized interface specialization.
typedef IAsyncOperationCompletedHandler<__FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo*> __FIAsyncOperationCompletedHandler_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_t;
#define __FIAsyncOperationCompletedHandler_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo ABI::Windows::Foundation::__FIAsyncOperationCompletedHandler_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_t
/* Foundation */ } /* Windows */ } /* ABI */ }

#endif // !defined(RO_NO_TEMPLATE_NAME)
#endif /* DEF___FIAsyncOperationCompletedHandler_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_USE */

#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

namespace ABI {
    namespace Microsoft {
        namespace Windows {
            namespace AI {
                namespace MachineLearning {
                    class CatalogModelInstanceResult;
                } /* MachineLearning */
            } /* AI */
        } /* Windows */
    } /* Microsoft */
} /* ABI */

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#ifndef DEF___FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_USE
#define DEF___FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_USE
#if !defined(RO_NO_TEMPLATE_NAME)
namespace ABI { namespace Windows { namespace Foundation {
template <>
struct __declspec(uuid("f75608eb-c2ca-51b8-b261-df6b2927deb5"))
IAsyncOperationWithProgressCompletedHandler<ABI::Microsoft::Windows::AI::MachineLearning::CatalogModelInstanceResult*, double> : IAsyncOperationWithProgressCompletedHandler_impl<ABI::Windows::Foundation::Internal::AggregateType<ABI::Microsoft::Windows::AI::MachineLearning::CatalogModelInstanceResult*, ABI::Microsoft::Windows::AI::MachineLearning::ICatalogModelInstanceResult*>, double>
{
    static const wchar_t* z_get_rc_name_impl()
    {
        return L"Windows.Foundation.AsyncOperationWithProgressCompletedHandler`2<Microsoft.Windows.AI.MachineLearning.CatalogModelInstanceResult, Double>";
    }
};
// Define a typedef for the parameterized interface specialization's mangled name.
// This allows code which uses the mangled name for the parameterized interface to access the
// correct parameterized interface specialization.
typedef IAsyncOperationWithProgressCompletedHandler<ABI::Microsoft::Windows::AI::MachineLearning::CatalogModelInstanceResult*, double> __FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_t;
#define __FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double ABI::Windows::Foundation::__FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_t
/* Foundation */ } /* Windows */ } /* ABI */ }

#endif // !defined(RO_NO_TEMPLATE_NAME)
#endif /* DEF___FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_USE */

#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#ifndef DEF___FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_USE
#define DEF___FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_USE
#if !defined(RO_NO_TEMPLATE_NAME)
namespace ABI { namespace Windows { namespace Foundation {
template <>
struct __declspec(uuid("65638493-e646-5c03-b731-510573fd21dd"))
IAsyncOperationWithProgress<ABI::Microsoft::Windows::AI::MachineLearning::CatalogModelInstanceResult*, double> : IAsyncOperationWithProgress_impl<ABI::Windows::Foundation::Internal::AggregateType<ABI::Microsoft::Windows::AI::MachineLearning::CatalogModelInstanceResult*, ABI::Microsoft::Windows::AI::MachineLearning::ICatalogModelInstanceResult*>, double>
{
    static const wchar_t* z_get_rc_name_impl()
    {
        return L"Windows.Foundation.IAsyncOperationWithProgress`2<Microsoft.Windows.AI.MachineLearning.CatalogModelInstanceResult, Double>";
    }
};
// Define a typedef for the parameterized interface specialization's mangled name.
// This allows code which uses the mangled name for the parameterized interface to access the
// correct parameterized interface specialization.
typedef IAsyncOperationWithProgress<ABI::Microsoft::Windows::AI::MachineLearning::CatalogModelInstanceResult*, double> __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_t;
#define __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double ABI::Windows::Foundation::__FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_t
/* Foundation */ } /* Windows */ } /* ABI */ }

#endif // !defined(RO_NO_TEMPLATE_NAME)
#endif /* DEF___FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_USE */

#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#ifndef DEF___FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_USE
#define DEF___FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_USE
#if !defined(RO_NO_TEMPLATE_NAME)
namespace ABI { namespace Windows { namespace Foundation {
template <>
struct __declspec(uuid("aa6cdca2-fc5a-584a-9781-4a8ccf2d5c84"))
IAsyncOperationProgressHandler<ABI::Microsoft::Windows::AI::MachineLearning::CatalogModelInstanceResult*, double> : IAsyncOperationProgressHandler_impl<ABI::Windows::Foundation::Internal::AggregateType<ABI::Microsoft::Windows::AI::MachineLearning::CatalogModelInstanceResult*, ABI::Microsoft::Windows::AI::MachineLearning::ICatalogModelInstanceResult*>, double>
{
    static const wchar_t* z_get_rc_name_impl()
    {
        return L"Windows.Foundation.AsyncOperationProgressHandler`2<Microsoft.Windows.AI.MachineLearning.CatalogModelInstanceResult, Double>";
    }
};
// Define a typedef for the parameterized interface specialization's mangled name.
// This allows code which uses the mangled name for the parameterized interface to access the
// correct parameterized interface specialization.
typedef IAsyncOperationProgressHandler<ABI::Microsoft::Windows::AI::MachineLearning::CatalogModelInstanceResult*, double> __FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_t;
#define __FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double ABI::Windows::Foundation::__FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_t
/* Foundation */ } /* Windows */ } /* ABI */ }

#endif // !defined(RO_NO_TEMPLATE_NAME)
#endif /* DEF___FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_USE */

#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

namespace ABI {
    namespace Microsoft {
        namespace Windows {
            namespace AI {
                namespace MachineLearning {
                    class ExecutionProviderReadyResult;
                } /* MachineLearning */
            } /* AI */
        } /* Windows */
    } /* Microsoft */
} /* ABI */

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

#ifndef DEF___FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_USE
#define DEF___FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_USE
#if !defined(RO_NO_TEMPLATE_NAME)
namespace ABI { namespace Windows { namespace Foundation {
template <>
struct __declspec(uuid("cb503403-2777-55e8-a053-58a39c93e645"))
IAsyncOperationWithProgressCompletedHandler<ABI::Microsoft::Windows::AI::MachineLearning::ExecutionProviderReadyResult*, double> : IAsyncOperationWithProgressCompletedHandler_impl<ABI::Windows::Foundation::Internal::AggregateType<ABI::Microsoft::Windows::AI::MachineLearning::ExecutionProviderReadyResult*, ABI::Microsoft::Windows::AI::MachineLearning::IExecutionProviderReadyResult*>, double>
{
    static const wchar_t* z_get_rc_name_impl()
    {
        return L"Windows.Foundation.AsyncOperationWithProgressCompletedHandler`2<Microsoft.Windows.AI.MachineLearning.ExecutionProviderReadyResult, Double>";
    }
};
// Define a typedef for the parameterized interface specialization's mangled name.
// This allows code which uses the mangled name for the parameterized interface to access the
// correct parameterized interface specialization.
typedef IAsyncOperationWithProgressCompletedHandler<ABI::Microsoft::Windows::AI::MachineLearning::ExecutionProviderReadyResult*, double> __FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_t;
#define __FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double ABI::Windows::Foundation::__FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_t
/* Foundation */ } /* Windows */ } /* ABI */ }

#endif // !defined(RO_NO_TEMPLATE_NAME)
#endif /* DEF___FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_USE */

#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

#ifndef DEF___FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_USE
#define DEF___FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_USE
#if !defined(RO_NO_TEMPLATE_NAME)
namespace ABI { namespace Windows { namespace Foundation {
template <>
struct __declspec(uuid("4f16a3c6-1230-5503-b92f-7d9c97139ad4"))
IAsyncOperationWithProgress<ABI::Microsoft::Windows::AI::MachineLearning::ExecutionProviderReadyResult*, double> : IAsyncOperationWithProgress_impl<ABI::Windows::Foundation::Internal::AggregateType<ABI::Microsoft::Windows::AI::MachineLearning::ExecutionProviderReadyResult*, ABI::Microsoft::Windows::AI::MachineLearning::IExecutionProviderReadyResult*>, double>
{
    static const wchar_t* z_get_rc_name_impl()
    {
        return L"Windows.Foundation.IAsyncOperationWithProgress`2<Microsoft.Windows.AI.MachineLearning.ExecutionProviderReadyResult, Double>";
    }
};
// Define a typedef for the parameterized interface specialization's mangled name.
// This allows code which uses the mangled name for the parameterized interface to access the
// correct parameterized interface specialization.
typedef IAsyncOperationWithProgress<ABI::Microsoft::Windows::AI::MachineLearning::ExecutionProviderReadyResult*, double> __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_t;
#define __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double ABI::Windows::Foundation::__FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_t
/* Foundation */ } /* Windows */ } /* ABI */ }

#endif // !defined(RO_NO_TEMPLATE_NAME)
#endif /* DEF___FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_USE */

#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

#ifndef DEF___FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_USE
#define DEF___FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_USE
#if !defined(RO_NO_TEMPLATE_NAME)
namespace ABI { namespace Windows { namespace Foundation {
template <>
struct __declspec(uuid("cc529045-fc8f-5897-bb77-f3459844ef0b"))
IAsyncOperationProgressHandler<ABI::Microsoft::Windows::AI::MachineLearning::ExecutionProviderReadyResult*, double> : IAsyncOperationProgressHandler_impl<ABI::Windows::Foundation::Internal::AggregateType<ABI::Microsoft::Windows::AI::MachineLearning::ExecutionProviderReadyResult*, ABI::Microsoft::Windows::AI::MachineLearning::IExecutionProviderReadyResult*>, double>
{
    static const wchar_t* z_get_rc_name_impl()
    {
        return L"Windows.Foundation.AsyncOperationProgressHandler`2<Microsoft.Windows.AI.MachineLearning.ExecutionProviderReadyResult, Double>";
    }
};
// Define a typedef for the parameterized interface specialization's mangled name.
// This allows code which uses the mangled name for the parameterized interface to access the
// correct parameterized interface specialization.
typedef IAsyncOperationProgressHandler<ABI::Microsoft::Windows::AI::MachineLearning::ExecutionProviderReadyResult*, double> __FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_t;
#define __FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double ABI::Windows::Foundation::__FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_t
/* Foundation */ } /* Windows */ } /* ABI */ }

#endif // !defined(RO_NO_TEMPLATE_NAME)
#endif /* DEF___FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_USE */

#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

namespace ABI {
    namespace Microsoft {
        namespace Windows {
            namespace AI {
                namespace MachineLearning {
                    class ExecutionProvider;
                } /* MachineLearning */
            } /* AI */
        } /* Windows */
    } /* Microsoft */
} /* ABI */

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

#ifndef DEF___FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_USE
#define DEF___FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_USE
#if !defined(RO_NO_TEMPLATE_NAME)
namespace ABI { namespace Windows { namespace Foundation { namespace Collections {
template <>
struct __declspec(uuid("511a99cf-fd7c-535c-b039-359b71f8dff5"))
IIterator<ABI::Microsoft::Windows::AI::MachineLearning::ExecutionProvider*> : IIterator_impl<ABI::Windows::Foundation::Internal::AggregateType<ABI::Microsoft::Windows::AI::MachineLearning::ExecutionProvider*, ABI::Microsoft::Windows::AI::MachineLearning::IExecutionProvider*>>
{
    static const wchar_t* z_get_rc_name_impl()
    {
        return L"Windows.Foundation.Collections.IIterator`1<Microsoft.Windows.AI.MachineLearning.ExecutionProvider>";
    }
};
// Define a typedef for the parameterized interface specialization's mangled name.
// This allows code which uses the mangled name for the parameterized interface to access the
// correct parameterized interface specialization.
typedef IIterator<ABI::Microsoft::Windows::AI::MachineLearning::ExecutionProvider*> __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_t;
#define __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider ABI::Windows::Foundation::Collections::__FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_t
/* Collections */ } /* Foundation */ } /* Windows */ } /* ABI */ }

#endif // !defined(RO_NO_TEMPLATE_NAME)
#endif /* DEF___FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_USE */

#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

#ifndef DEF___FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_USE
#define DEF___FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_USE
#if !defined(RO_NO_TEMPLATE_NAME)
namespace ABI { namespace Windows { namespace Foundation { namespace Collections {
template <>
struct __declspec(uuid("24c3e327-7502-52ad-8f77-3d2fcebed429"))
IIterable<ABI::Microsoft::Windows::AI::MachineLearning::ExecutionProvider*> : IIterable_impl<ABI::Windows::Foundation::Internal::AggregateType<ABI::Microsoft::Windows::AI::MachineLearning::ExecutionProvider*, ABI::Microsoft::Windows::AI::MachineLearning::IExecutionProvider*>>
{
    static const wchar_t* z_get_rc_name_impl()
    {
        return L"Windows.Foundation.Collections.IIterable`1<Microsoft.Windows.AI.MachineLearning.ExecutionProvider>";
    }
};
// Define a typedef for the parameterized interface specialization's mangled name.
// This allows code which uses the mangled name for the parameterized interface to access the
// correct parameterized interface specialization.
typedef IIterable<ABI::Microsoft::Windows::AI::MachineLearning::ExecutionProvider*> __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_t;
#define __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider ABI::Windows::Foundation::Collections::__FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_t
/* Collections */ } /* Foundation */ } /* Windows */ } /* ABI */ }

#endif // !defined(RO_NO_TEMPLATE_NAME)
#endif /* DEF___FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_USE */

#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

#ifndef DEF___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_USE
#define DEF___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_USE
#if !defined(RO_NO_TEMPLATE_NAME)
namespace ABI { namespace Windows { namespace Foundation { namespace Collections {
template <>
struct __declspec(uuid("93c6b249-9a52-5640-a46c-75d0a52b40f0"))
IVectorView<ABI::Microsoft::Windows::AI::MachineLearning::ExecutionProvider*> : IVectorView_impl<ABI::Windows::Foundation::Internal::AggregateType<ABI::Microsoft::Windows::AI::MachineLearning::ExecutionProvider*, ABI::Microsoft::Windows::AI::MachineLearning::IExecutionProvider*>>
{
    static const wchar_t* z_get_rc_name_impl()
    {
        return L"Windows.Foundation.Collections.IVectorView`1<Microsoft.Windows.AI.MachineLearning.ExecutionProvider>";
    }
};
// Define a typedef for the parameterized interface specialization's mangled name.
// This allows code which uses the mangled name for the parameterized interface to access the
// correct parameterized interface specialization.
typedef IVectorView<ABI::Microsoft::Windows::AI::MachineLearning::ExecutionProvider*> __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_t;
#define __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider ABI::Windows::Foundation::Collections::__FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_t
/* Collections */ } /* Foundation */ } /* Windows */ } /* ABI */ }

#endif // !defined(RO_NO_TEMPLATE_NAME)
#endif /* DEF___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_USE */

#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

#ifndef DEF___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_USE
#define DEF___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_USE
#if !defined(RO_NO_TEMPLATE_NAME)
namespace ABI { namespace Windows { namespace Foundation { namespace Collections {
template <>
struct __declspec(uuid("e8096b87-1eee-5237-874d-b48b5253cadb"))
IVector<ABI::Microsoft::Windows::AI::MachineLearning::ExecutionProvider*> : IVector_impl<ABI::Windows::Foundation::Internal::AggregateType<ABI::Microsoft::Windows::AI::MachineLearning::ExecutionProvider*, ABI::Microsoft::Windows::AI::MachineLearning::IExecutionProvider*>>
{
    static const wchar_t* z_get_rc_name_impl()
    {
        return L"Windows.Foundation.Collections.IVector`1<Microsoft.Windows.AI.MachineLearning.ExecutionProvider>";
    }
};
// Define a typedef for the parameterized interface specialization's mangled name.
// This allows code which uses the mangled name for the parameterized interface to access the
// correct parameterized interface specialization.
typedef IVector<ABI::Microsoft::Windows::AI::MachineLearning::ExecutionProvider*> __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_t;
#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider ABI::Windows::Foundation::Collections::__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_t
/* Collections */ } /* Foundation */ } /* Windows */ } /* ABI */ }

#endif // !defined(RO_NO_TEMPLATE_NAME)
#endif /* DEF___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_USE */

#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

#ifndef DEF___FIAsyncOperationWithProgressCompletedHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_USE
#define DEF___FIAsyncOperationWithProgressCompletedHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_USE
#if !defined(RO_NO_TEMPLATE_NAME)
namespace ABI { namespace Windows { namespace Foundation {
template <>
struct __declspec(uuid("c819ad74-4288-54d3-9f49-32f89ddd870b"))
IAsyncOperationWithProgressCompletedHandler<__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider*, double> : IAsyncOperationWithProgressCompletedHandler_impl<__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider*, double>
{
    static const wchar_t* z_get_rc_name_impl()
    {
        return L"Windows.Foundation.AsyncOperationWithProgressCompletedHandler`2<Windows.Foundation.Collections.IVector`1<Microsoft.Windows.AI.MachineLearning.ExecutionProvider>, Double>";
    }
};
// Define a typedef for the parameterized interface specialization's mangled name.
// This allows code which uses the mangled name for the parameterized interface to access the
// correct parameterized interface specialization.
typedef IAsyncOperationWithProgressCompletedHandler<__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider*, double> __FIAsyncOperationWithProgressCompletedHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_t;
#define __FIAsyncOperationWithProgressCompletedHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double ABI::Windows::Foundation::__FIAsyncOperationWithProgressCompletedHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_t
/* Foundation */ } /* Windows */ } /* ABI */ }

#endif // !defined(RO_NO_TEMPLATE_NAME)
#endif /* DEF___FIAsyncOperationWithProgressCompletedHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_USE */

#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

#ifndef DEF___FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_USE
#define DEF___FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_USE
#if !defined(RO_NO_TEMPLATE_NAME)
namespace ABI { namespace Windows { namespace Foundation {
template <>
struct __declspec(uuid("2400b66a-d1b1-5e7f-8acd-421a23771880"))
IAsyncOperationWithProgress<__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider*, double> : IAsyncOperationWithProgress_impl<__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider*, double>
{
    static const wchar_t* z_get_rc_name_impl()
    {
        return L"Windows.Foundation.IAsyncOperationWithProgress`2<Windows.Foundation.Collections.IVector`1<Microsoft.Windows.AI.MachineLearning.ExecutionProvider>, Double>";
    }
};
// Define a typedef for the parameterized interface specialization's mangled name.
// This allows code which uses the mangled name for the parameterized interface to access the
// correct parameterized interface specialization.
typedef IAsyncOperationWithProgress<__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider*, double> __FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_t;
#define __FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double ABI::Windows::Foundation::__FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_t
/* Foundation */ } /* Windows */ } /* ABI */ }

#endif // !defined(RO_NO_TEMPLATE_NAME)
#endif /* DEF___FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_USE */

#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

#ifndef DEF___FIAsyncOperationProgressHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_USE
#define DEF___FIAsyncOperationProgressHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_USE
#if !defined(RO_NO_TEMPLATE_NAME)
namespace ABI { namespace Windows { namespace Foundation {
template <>
struct __declspec(uuid("582a3320-2121-5d01-94d4-a7422940cbd6"))
IAsyncOperationProgressHandler<__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider*, double> : IAsyncOperationProgressHandler_impl<__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider*, double>
{
    static const wchar_t* z_get_rc_name_impl()
    {
        return L"Windows.Foundation.AsyncOperationProgressHandler`2<Windows.Foundation.Collections.IVector`1<Microsoft.Windows.AI.MachineLearning.ExecutionProvider>, Double>";
    }
};
// Define a typedef for the parameterized interface specialization's mangled name.
// This allows code which uses the mangled name for the parameterized interface to access the
// correct parameterized interface specialization.
typedef IAsyncOperationProgressHandler<__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider*, double> __FIAsyncOperationProgressHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_t;
#define __FIAsyncOperationProgressHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double ABI::Windows::Foundation::__FIAsyncOperationProgressHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_t
/* Foundation */ } /* Windows */ } /* ABI */ }

#endif // !defined(RO_NO_TEMPLATE_NAME)
#endif /* DEF___FIAsyncOperationProgressHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_USE */

#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#ifndef DEF___FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_USE
#define DEF___FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_USE
#if !defined(RO_NO_TEMPLATE_NAME)
namespace ABI { namespace Windows { namespace Foundation { namespace Collections {
template <>
struct __declspec(uuid("9c66662c-828f-5d44-8b7e-6b16425afd36"))
IIterator<ABI::Microsoft::Windows::AI::MachineLearning::ModelCatalogSource*> : IIterator_impl<ABI::Windows::Foundation::Internal::AggregateType<ABI::Microsoft::Windows::AI::MachineLearning::ModelCatalogSource*, ABI::Microsoft::Windows::AI::MachineLearning::IModelCatalogSource*>>
{
    static const wchar_t* z_get_rc_name_impl()
    {
        return L"Windows.Foundation.Collections.IIterator`1<Microsoft.Windows.AI.MachineLearning.ModelCatalogSource>";
    }
};
// Define a typedef for the parameterized interface specialization's mangled name.
// This allows code which uses the mangled name for the parameterized interface to access the
// correct parameterized interface specialization.
typedef IIterator<ABI::Microsoft::Windows::AI::MachineLearning::ModelCatalogSource*> __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_t;
#define __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource ABI::Windows::Foundation::Collections::__FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_t
/* Collections */ } /* Foundation */ } /* Windows */ } /* ABI */ }

#endif // !defined(RO_NO_TEMPLATE_NAME)
#endif /* DEF___FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_USE */

#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#ifndef DEF___FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_USE
#define DEF___FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_USE
#if !defined(RO_NO_TEMPLATE_NAME)
namespace ABI { namespace Windows { namespace Foundation { namespace Collections {
template <>
struct __declspec(uuid("81967082-bf87-54d0-81f1-5f3056c5999a"))
IIterable<ABI::Microsoft::Windows::AI::MachineLearning::ModelCatalogSource*> : IIterable_impl<ABI::Windows::Foundation::Internal::AggregateType<ABI::Microsoft::Windows::AI::MachineLearning::ModelCatalogSource*, ABI::Microsoft::Windows::AI::MachineLearning::IModelCatalogSource*>>
{
    static const wchar_t* z_get_rc_name_impl()
    {
        return L"Windows.Foundation.Collections.IIterable`1<Microsoft.Windows.AI.MachineLearning.ModelCatalogSource>";
    }
};
// Define a typedef for the parameterized interface specialization's mangled name.
// This allows code which uses the mangled name for the parameterized interface to access the
// correct parameterized interface specialization.
typedef IIterable<ABI::Microsoft::Windows::AI::MachineLearning::ModelCatalogSource*> __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_t;
#define __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource ABI::Windows::Foundation::Collections::__FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_t
/* Collections */ } /* Foundation */ } /* Windows */ } /* ABI */ }

#endif // !defined(RO_NO_TEMPLATE_NAME)
#endif /* DEF___FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_USE */

#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000


#ifndef DEF___FIIterator_1_HSTRING_USE
#define DEF___FIIterator_1_HSTRING_USE
#if !defined(RO_NO_TEMPLATE_NAME)
namespace ABI { namespace Windows { namespace Foundation { namespace Collections {
template <>
struct __declspec(uuid("8c304ebb-6615-50a4-8829-879ecd443236"))
IIterator<HSTRING> : IIterator_impl<HSTRING>
{
    static const wchar_t* z_get_rc_name_impl()
    {
        return L"Windows.Foundation.Collections.IIterator`1<String>";
    }
};
// Define a typedef for the parameterized interface specialization's mangled name.
// This allows code which uses the mangled name for the parameterized interface to access the
// correct parameterized interface specialization.
typedef IIterator<HSTRING> __FIIterator_1_HSTRING_t;
#define __FIIterator_1_HSTRING ABI::Windows::Foundation::Collections::__FIIterator_1_HSTRING_t
/* Collections */ } /* Foundation */ } /* Windows */ } /* ABI */ }

#endif // !defined(RO_NO_TEMPLATE_NAME)
#endif /* DEF___FIIterator_1_HSTRING_USE */



#ifndef DEF___FIIterable_1_HSTRING_USE
#define DEF___FIIterable_1_HSTRING_USE
#if !defined(RO_NO_TEMPLATE_NAME)
namespace ABI { namespace Windows { namespace Foundation { namespace Collections {
template <>
struct __declspec(uuid("e2fcc7c1-3bfc-5a0b-b2b0-72e769d1cb7e"))
IIterable<HSTRING> : IIterable_impl<HSTRING>
{
    static const wchar_t* z_get_rc_name_impl()
    {
        return L"Windows.Foundation.Collections.IIterable`1<String>";
    }
};
// Define a typedef for the parameterized interface specialization's mangled name.
// This allows code which uses the mangled name for the parameterized interface to access the
// correct parameterized interface specialization.
typedef IIterable<HSTRING> __FIIterable_1_HSTRING_t;
#define __FIIterable_1_HSTRING ABI::Windows::Foundation::Collections::__FIIterable_1_HSTRING_t
/* Collections */ } /* Foundation */ } /* Windows */ } /* ABI */ }

#endif // !defined(RO_NO_TEMPLATE_NAME)
#endif /* DEF___FIIterable_1_HSTRING_USE */



#ifndef DEF___FIKeyValuePair_2_HSTRING_HSTRING_USE
#define DEF___FIKeyValuePair_2_HSTRING_HSTRING_USE
#if !defined(RO_NO_TEMPLATE_NAME)
namespace ABI { namespace Windows { namespace Foundation { namespace Collections {
template <>
struct __declspec(uuid("60310303-49c5-52e6-abc6-a9b36eccc716"))
IKeyValuePair<HSTRING, HSTRING> : IKeyValuePair_impl<HSTRING, HSTRING>
{
    static const wchar_t* z_get_rc_name_impl()
    {
        return L"Windows.Foundation.Collections.IKeyValuePair`2<String, String>";
    }
};
// Define a typedef for the parameterized interface specialization's mangled name.
// This allows code which uses the mangled name for the parameterized interface to access the
// correct parameterized interface specialization.
typedef IKeyValuePair<HSTRING, HSTRING> __FIKeyValuePair_2_HSTRING_HSTRING_t;
#define __FIKeyValuePair_2_HSTRING_HSTRING ABI::Windows::Foundation::Collections::__FIKeyValuePair_2_HSTRING_HSTRING_t
/* Collections */ } /* Foundation */ } /* Windows */ } /* ABI */ }

#endif // !defined(RO_NO_TEMPLATE_NAME)
#endif /* DEF___FIKeyValuePair_2_HSTRING_HSTRING_USE */



#ifndef DEF___FIIterator_1___FIKeyValuePair_2_HSTRING_HSTRING_USE
#define DEF___FIIterator_1___FIKeyValuePair_2_HSTRING_HSTRING_USE
#if !defined(RO_NO_TEMPLATE_NAME)
namespace ABI { namespace Windows { namespace Foundation { namespace Collections {
template <>
struct __declspec(uuid("05eb86f1-7140-5517-b88d-cbaebe57e6b1"))
IIterator<__FIKeyValuePair_2_HSTRING_HSTRING*> : IIterator_impl<__FIKeyValuePair_2_HSTRING_HSTRING*>
{
    static const wchar_t* z_get_rc_name_impl()
    {
        return L"Windows.Foundation.Collections.IIterator`1<Windows.Foundation.Collections.IKeyValuePair`2<String, String>>";
    }
};
// Define a typedef for the parameterized interface specialization's mangled name.
// This allows code which uses the mangled name for the parameterized interface to access the
// correct parameterized interface specialization.
typedef IIterator<__FIKeyValuePair_2_HSTRING_HSTRING*> __FIIterator_1___FIKeyValuePair_2_HSTRING_HSTRING_t;
#define __FIIterator_1___FIKeyValuePair_2_HSTRING_HSTRING ABI::Windows::Foundation::Collections::__FIIterator_1___FIKeyValuePair_2_HSTRING_HSTRING_t
/* Collections */ } /* Foundation */ } /* Windows */ } /* ABI */ }

#endif // !defined(RO_NO_TEMPLATE_NAME)
#endif /* DEF___FIIterator_1___FIKeyValuePair_2_HSTRING_HSTRING_USE */



#ifndef DEF___FIIterable_1___FIKeyValuePair_2_HSTRING_HSTRING_USE
#define DEF___FIIterable_1___FIKeyValuePair_2_HSTRING_HSTRING_USE
#if !defined(RO_NO_TEMPLATE_NAME)
namespace ABI { namespace Windows { namespace Foundation { namespace Collections {
template <>
struct __declspec(uuid("e9bdaaf0-cbf6-5c72-be90-29cbf3a1319b"))
IIterable<__FIKeyValuePair_2_HSTRING_HSTRING*> : IIterable_impl<__FIKeyValuePair_2_HSTRING_HSTRING*>
{
    static const wchar_t* z_get_rc_name_impl()
    {
        return L"Windows.Foundation.Collections.IIterable`1<Windows.Foundation.Collections.IKeyValuePair`2<String, String>>";
    }
};
// Define a typedef for the parameterized interface specialization's mangled name.
// This allows code which uses the mangled name for the parameterized interface to access the
// correct parameterized interface specialization.
typedef IIterable<__FIKeyValuePair_2_HSTRING_HSTRING*> __FIIterable_1___FIKeyValuePair_2_HSTRING_HSTRING_t;
#define __FIIterable_1___FIKeyValuePair_2_HSTRING_HSTRING ABI::Windows::Foundation::Collections::__FIIterable_1___FIKeyValuePair_2_HSTRING_HSTRING_t
/* Collections */ } /* Foundation */ } /* Windows */ } /* ABI */ }

#endif // !defined(RO_NO_TEMPLATE_NAME)
#endif /* DEF___FIIterable_1___FIKeyValuePair_2_HSTRING_HSTRING_USE */


#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#ifndef DEF___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_USE
#define DEF___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_USE
#if !defined(RO_NO_TEMPLATE_NAME)
namespace ABI { namespace Windows { namespace Foundation { namespace Collections {
template <>
struct __declspec(uuid("86ab62eb-d7d8-55b8-9165-2202d53ac7df"))
IVectorView<ABI::Microsoft::Windows::AI::MachineLearning::ModelCatalogSource*> : IVectorView_impl<ABI::Windows::Foundation::Internal::AggregateType<ABI::Microsoft::Windows::AI::MachineLearning::ModelCatalogSource*, ABI::Microsoft::Windows::AI::MachineLearning::IModelCatalogSource*>>
{
    static const wchar_t* z_get_rc_name_impl()
    {
        return L"Windows.Foundation.Collections.IVectorView`1<Microsoft.Windows.AI.MachineLearning.ModelCatalogSource>";
    }
};
// Define a typedef for the parameterized interface specialization's mangled name.
// This allows code which uses the mangled name for the parameterized interface to access the
// correct parameterized interface specialization.
typedef IVectorView<ABI::Microsoft::Windows::AI::MachineLearning::ModelCatalogSource*> __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_t;
#define __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource ABI::Windows::Foundation::Collections::__FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_t
/* Collections */ } /* Foundation */ } /* Windows */ } /* ABI */ }

#endif // !defined(RO_NO_TEMPLATE_NAME)
#endif /* DEF___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_USE */

#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000


#ifndef DEF___FIVectorView_1_HSTRING_USE
#define DEF___FIVectorView_1_HSTRING_USE
#if !defined(RO_NO_TEMPLATE_NAME)
namespace ABI { namespace Windows { namespace Foundation { namespace Collections {
template <>
struct __declspec(uuid("2f13c006-a03a-5f69-b090-75a43e33423e"))
IVectorView<HSTRING> : IVectorView_impl<HSTRING>
{
    static const wchar_t* z_get_rc_name_impl()
    {
        return L"Windows.Foundation.Collections.IVectorView`1<String>";
    }
};
// Define a typedef for the parameterized interface specialization's mangled name.
// This allows code which uses the mangled name for the parameterized interface to access the
// correct parameterized interface specialization.
typedef IVectorView<HSTRING> __FIVectorView_1_HSTRING_t;
#define __FIVectorView_1_HSTRING ABI::Windows::Foundation::Collections::__FIVectorView_1_HSTRING_t
/* Collections */ } /* Foundation */ } /* Windows */ } /* ABI */ }

#endif // !defined(RO_NO_TEMPLATE_NAME)
#endif /* DEF___FIVectorView_1_HSTRING_USE */


#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#ifndef DEF___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_USE
#define DEF___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_USE
#if !defined(RO_NO_TEMPLATE_NAME)
namespace ABI { namespace Windows { namespace Foundation { namespace Collections {
template <>
struct __declspec(uuid("4ec66e4a-d1cc-5086-a553-f99896b2031d"))
IVector<ABI::Microsoft::Windows::AI::MachineLearning::CatalogModelInfo*> : IVector_impl<ABI::Windows::Foundation::Internal::AggregateType<ABI::Microsoft::Windows::AI::MachineLearning::CatalogModelInfo*, ABI::Microsoft::Windows::AI::MachineLearning::ICatalogModelInfo*>>
{
    static const wchar_t* z_get_rc_name_impl()
    {
        return L"Windows.Foundation.Collections.IVector`1<Microsoft.Windows.AI.MachineLearning.CatalogModelInfo>";
    }
};
// Define a typedef for the parameterized interface specialization's mangled name.
// This allows code which uses the mangled name for the parameterized interface to access the
// correct parameterized interface specialization.
typedef IVector<ABI::Microsoft::Windows::AI::MachineLearning::CatalogModelInfo*> __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_t;
#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo ABI::Windows::Foundation::Collections::__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_t
/* Collections */ } /* Foundation */ } /* Windows */ } /* ABI */ }

#endif // !defined(RO_NO_TEMPLATE_NAME)
#endif /* DEF___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_USE */

#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#ifndef DEF___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_USE
#define DEF___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_USE
#if !defined(RO_NO_TEMPLATE_NAME)
namespace ABI { namespace Windows { namespace Foundation { namespace Collections {
template <>
struct __declspec(uuid("e7f73cfa-cc36-52db-b16f-39df10c63506"))
IVector<ABI::Microsoft::Windows::AI::MachineLearning::ModelCatalogSource*> : IVector_impl<ABI::Windows::Foundation::Internal::AggregateType<ABI::Microsoft::Windows::AI::MachineLearning::ModelCatalogSource*, ABI::Microsoft::Windows::AI::MachineLearning::IModelCatalogSource*>>
{
    static const wchar_t* z_get_rc_name_impl()
    {
        return L"Windows.Foundation.Collections.IVector`1<Microsoft.Windows.AI.MachineLearning.ModelCatalogSource>";
    }
};
// Define a typedef for the parameterized interface specialization's mangled name.
// This allows code which uses the mangled name for the parameterized interface to access the
// correct parameterized interface specialization.
typedef IVector<ABI::Microsoft::Windows::AI::MachineLearning::ModelCatalogSource*> __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_t;
#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource ABI::Windows::Foundation::Collections::__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_t
/* Collections */ } /* Foundation */ } /* Windows */ } /* ABI */ }

#endif // !defined(RO_NO_TEMPLATE_NAME)
#endif /* DEF___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_USE */

#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000


#ifndef DEF___FIVector_1_HSTRING_USE
#define DEF___FIVector_1_HSTRING_USE
#if !defined(RO_NO_TEMPLATE_NAME)
namespace ABI { namespace Windows { namespace Foundation { namespace Collections {
template <>
struct __declspec(uuid("98b9acc1-4b56-532e-ac73-03d5291cca90"))
IVector<HSTRING> : IVector_impl<HSTRING>
{
    static const wchar_t* z_get_rc_name_impl()
    {
        return L"Windows.Foundation.Collections.IVector`1<String>";
    }
};
// Define a typedef for the parameterized interface specialization's mangled name.
// This allows code which uses the mangled name for the parameterized interface to access the
// correct parameterized interface specialization.
typedef IVector<HSTRING> __FIVector_1_HSTRING_t;
#define __FIVector_1_HSTRING ABI::Windows::Foundation::Collections::__FIVector_1_HSTRING_t
/* Collections */ } /* Foundation */ } /* Windows */ } /* ABI */ }

#endif // !defined(RO_NO_TEMPLATE_NAME)
#endif /* DEF___FIVector_1_HSTRING_USE */


namespace ABI {
    namespace Windows {
        namespace ApplicationModel {
            class PackageId;
        } /* ApplicationModel */
    } /* Windows */
} /* ABI */

#ifndef ____x_ABI_CWindows_CApplicationModel_CIPackageId_FWD_DEFINED__
#define ____x_ABI_CWindows_CApplicationModel_CIPackageId_FWD_DEFINED__
namespace ABI {
    namespace Windows {
        namespace ApplicationModel {
            interface IPackageId;
        } /* ApplicationModel */
    } /* Windows */
} /* ABI */
#define __x_ABI_CWindows_CApplicationModel_CIPackageId ABI::Windows::ApplicationModel::IPackageId

#endif // ____x_ABI_CWindows_CApplicationModel_CIPackageId_FWD_DEFINED__

#ifndef ____x_ABI_CWindows_CFoundation_CIClosable_FWD_DEFINED__
#define ____x_ABI_CWindows_CFoundation_CIClosable_FWD_DEFINED__
namespace ABI {
    namespace Windows {
        namespace Foundation {
            interface IClosable;
        } /* Foundation */
    } /* Windows */
} /* ABI */
#define __x_ABI_CWindows_CFoundation_CIClosable ABI::Windows::Foundation::IClosable

#endif // ____x_ABI_CWindows_CFoundation_CIClosable_FWD_DEFINED__

namespace ABI {
    namespace Windows {
        namespace Foundation {
            class Uri;
        } /* Foundation */
    } /* Windows */
} /* ABI */

#ifndef ____x_ABI_CWindows_CFoundation_CIUriRuntimeClass_FWD_DEFINED__
#define ____x_ABI_CWindows_CFoundation_CIUriRuntimeClass_FWD_DEFINED__
namespace ABI {
    namespace Windows {
        namespace Foundation {
            interface IUriRuntimeClass;
        } /* Foundation */
    } /* Windows */
} /* ABI */
#define __x_ABI_CWindows_CFoundation_CIUriRuntimeClass ABI::Windows::Foundation::IUriRuntimeClass

#endif // ____x_ABI_CWindows_CFoundation_CIUriRuntimeClass_FWD_DEFINED__

namespace ABI {
    namespace Microsoft {
        namespace Windows {
            namespace AI {
                namespace MachineLearning {
                    typedef enum CatalogModelInstanceStatus : int CatalogModelInstanceStatus;
                } /* MachineLearning */
            } /* AI */
        } /* Windows */
    } /* Microsoft */
} /* ABI */

namespace ABI {
    namespace Microsoft {
        namespace Windows {
            namespace AI {
                namespace MachineLearning {
                    typedef enum CatalogModelStatus : int CatalogModelStatus;
                } /* MachineLearning */
            } /* AI */
        } /* Windows */
    } /* Microsoft */
} /* ABI */

namespace ABI {
    namespace Microsoft {
        namespace Windows {
            namespace AI {
                namespace MachineLearning {
                    typedef enum ExecutionProviderCertification : int ExecutionProviderCertification;
                } /* MachineLearning */
            } /* AI */
        } /* Windows */
    } /* Microsoft */
} /* ABI */

namespace ABI {
    namespace Microsoft {
        namespace Windows {
            namespace AI {
                namespace MachineLearning {
                    typedef enum ExecutionProviderReadyResultState : int ExecutionProviderReadyResultState;
                } /* MachineLearning */
            } /* AI */
        } /* Windows */
    } /* Microsoft */
} /* ABI */

namespace ABI {
    namespace Microsoft {
        namespace Windows {
            namespace AI {
                namespace MachineLearning {
                    typedef enum ExecutionProviderReadyState : int ExecutionProviderReadyState;
                } /* MachineLearning */
            } /* AI */
        } /* Windows */
    } /* Microsoft */
} /* ABI */

namespace ABI {
    namespace Microsoft {
        namespace Windows {
            namespace AI {
                namespace MachineLearning {
                    class CatalogModelInstance;
                } /* MachineLearning */
            } /* AI */
        } /* Windows */
    } /* Microsoft */
} /* ABI */

namespace ABI {
    namespace Microsoft {
        namespace Windows {
            namespace AI {
                namespace MachineLearning {
                    class ExecutionProviderCatalog;
                } /* MachineLearning */
            } /* AI */
        } /* Windows */
    } /* Microsoft */
} /* ABI */

namespace ABI {
    namespace Microsoft {
        namespace Windows {
            namespace AI {
                namespace MachineLearning {
                    class ModelCatalog;
                } /* MachineLearning */
            } /* AI */
        } /* Windows */
    } /* Microsoft */
} /* ABI */

/*
 *
 * Struct Microsoft.Windows.AI.MachineLearning.CatalogModelInstanceStatus
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 2.0
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
namespace ABI {
    namespace Microsoft {
        namespace Windows {
            namespace AI {
                namespace MachineLearning {
                    enum CatalogModelInstanceStatus : int
                    {
                        CatalogModelInstanceStatus_Available = 0,
                        CatalogModelInstanceStatus_InProgress = 1,
                        CatalogModelInstanceStatus_Unavailable = 2,
                    };
                } /* MachineLearning */
            } /* AI */
        } /* Windows */
    } /* Microsoft */
} /* ABI */
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

/*
 *
 * Struct Microsoft.Windows.AI.MachineLearning.CatalogModelStatus
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 2.0
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
namespace ABI {
    namespace Microsoft {
        namespace Windows {
            namespace AI {
                namespace MachineLearning {
                    enum CatalogModelStatus : int
                    {
                        CatalogModelStatus_Ready = 0,
                        CatalogModelStatus_NotReady = 1,
                    };
                } /* MachineLearning */
            } /* AI */
        } /* Windows */
    } /* Microsoft */
} /* ABI */
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

/*
 *
 * Struct Microsoft.Windows.AI.MachineLearning.ExecutionProviderCertification
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 1.0
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000
namespace ABI {
    namespace Microsoft {
        namespace Windows {
            namespace AI {
                namespace MachineLearning {
                    enum ExecutionProviderCertification : int
                    {
                        ExecutionProviderCertification_Unknown = 0,
                        ExecutionProviderCertification_Certified = 1,
                        ExecutionProviderCertification_Uncertified = 2,
                    };
                } /* MachineLearning */
            } /* AI */
        } /* Windows */
    } /* Microsoft */
} /* ABI */
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

/*
 *
 * Struct Microsoft.Windows.AI.MachineLearning.ExecutionProviderReadyResultState
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 1.0
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000
namespace ABI {
    namespace Microsoft {
        namespace Windows {
            namespace AI {
                namespace MachineLearning {
                    enum ExecutionProviderReadyResultState : int
                    {
                        ExecutionProviderReadyResultState_InProgress = 0,
                        ExecutionProviderReadyResultState_Success = 1,
                        ExecutionProviderReadyResultState_Failure = 2,
                    };
                } /* MachineLearning */
            } /* AI */
        } /* Windows */
    } /* Microsoft */
} /* ABI */
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

/*
 *
 * Struct Microsoft.Windows.AI.MachineLearning.ExecutionProviderReadyState
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 1.0
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000
namespace ABI {
    namespace Microsoft {
        namespace Windows {
            namespace AI {
                namespace MachineLearning {
                    enum ExecutionProviderReadyState : int
                    {
                        ExecutionProviderReadyState_Ready = 0,
                        ExecutionProviderReadyState_NotReady = 1,
                        ExecutionProviderReadyState_NotPresent = 2,
                    };
                } /* MachineLearning */
            } /* AI */
        } /* Windows */
    } /* Microsoft */
} /* ABI */
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

/*
 *
 * Interface Microsoft.Windows.AI.MachineLearning.ICatalogModelInfo
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 2.0
 *
 * Interface is a part of the implementation of type Microsoft.Windows.AI.MachineLearning.CatalogModelInfo
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
#if !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo_INTERFACE_DEFINED__)
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo_INTERFACE_DEFINED__
extern const __declspec(selectany) _Null_terminated_ WCHAR InterfaceName_Microsoft_Windows_AI_MachineLearning_ICatalogModelInfo[] = L"Microsoft.Windows.AI.MachineLearning.ICatalogModelInfo";
namespace ABI {
    namespace Microsoft {
        namespace Windows {
            namespace AI {
                namespace MachineLearning {
                    MIDL_INTERFACE("62057faa-3def-509f-9ed2-aef1e0de21a4")
                    ICatalogModelInfo : public IInspectable
                    {
                    public:
                        virtual HRESULT STDMETHODCALLTYPE get_Id(
                            HSTRING* value
                            ) = 0;
                        virtual HRESULT STDMETHODCALLTYPE get_Name(
                            HSTRING* value
                            ) = 0;
                        virtual HRESULT STDMETHODCALLTYPE get_Publisher(
                            HSTRING* value
                            ) = 0;
                        virtual HRESULT STDMETHODCALLTYPE get_SourceId(
                            HSTRING* value
                            ) = 0;
                        virtual HRESULT STDMETHODCALLTYPE get_Uri(
                            ABI::Windows::Foundation::IUriRuntimeClass** value
                            ) = 0;
                        virtual HRESULT STDMETHODCALLTYPE get_ExecutionProviders(
                            __FIVectorView_1_HSTRING** value
                            ) = 0;
                        virtual HRESULT STDMETHODCALLTYPE get_ModelSizeInBytes(
                            UINT64* value
                            ) = 0;
                        virtual HRESULT STDMETHODCALLTYPE get_Version(
                            HSTRING* value
                            ) = 0;
                        virtual HRESULT STDMETHODCALLTYPE get_License(
                            HSTRING* value
                            ) = 0;
                        virtual HRESULT STDMETHODCALLTYPE get_LicenseUri(
                            HSTRING* value
                            ) = 0;
                        virtual HRESULT STDMETHODCALLTYPE get_LicenseText(
                            HSTRING* value
                            ) = 0;
                        virtual HRESULT STDMETHODCALLTYPE GetStatus(
                            ABI::Microsoft::Windows::AI::MachineLearning::CatalogModelStatus* result
                            ) = 0;
                        virtual HRESULT STDMETHODCALLTYPE GetInstanceAsync(
                            __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double** operation
                            ) = 0;
                        virtual HRESULT STDMETHODCALLTYPE GetInstanceAsync2(
                            __FIIterable_1___FIKeyValuePair_2_HSTRING_HSTRING* additionalHeaders,
                            __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double** operation
                            ) = 0;
                    };

                    MIDL_CONST_ID IID& IID_ICatalogModelInfo = _uuidof(ICatalogModelInfo);
                } /* MachineLearning */
            } /* AI */
        } /* Windows */
    } /* Microsoft */
} /* ABI */

EXTERN_C const IID IID___x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo;
#endif /* !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo_INTERFACE_DEFINED__) */
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

/*
 *
 * Interface Microsoft.Windows.AI.MachineLearning.ICatalogModelInstance
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 2.0
 *
 * Interface is a part of the implementation of type Microsoft.Windows.AI.MachineLearning.CatalogModelInstance
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
#if !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstance_INTERFACE_DEFINED__)
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstance_INTERFACE_DEFINED__
extern const __declspec(selectany) _Null_terminated_ WCHAR InterfaceName_Microsoft_Windows_AI_MachineLearning_ICatalogModelInstance[] = L"Microsoft.Windows.AI.MachineLearning.ICatalogModelInstance";
namespace ABI {
    namespace Microsoft {
        namespace Windows {
            namespace AI {
                namespace MachineLearning {
                    MIDL_INTERFACE("9e920521-5e06-5c30-b0c2-8af313efb756")
                    ICatalogModelInstance : public IInspectable
                    {
                    public:
                        virtual HRESULT STDMETHODCALLTYPE get_ModelPaths(
                            __FIVectorView_1_HSTRING** value
                            ) = 0;
                        virtual HRESULT STDMETHODCALLTYPE get_ModelInfo(
                            ABI::Microsoft::Windows::AI::MachineLearning::ICatalogModelInfo** value
                            ) = 0;
                    };

                    MIDL_CONST_ID IID& IID_ICatalogModelInstance = _uuidof(ICatalogModelInstance);
                } /* MachineLearning */
            } /* AI */
        } /* Windows */
    } /* Microsoft */
} /* ABI */

EXTERN_C const IID IID___x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstance;
#endif /* !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstance_INTERFACE_DEFINED__) */
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

/*
 *
 * Interface Microsoft.Windows.AI.MachineLearning.ICatalogModelInstanceResult
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 2.0
 *
 * Interface is a part of the implementation of type Microsoft.Windows.AI.MachineLearning.CatalogModelInstanceResult
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
#if !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResult_INTERFACE_DEFINED__)
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResult_INTERFACE_DEFINED__
extern const __declspec(selectany) _Null_terminated_ WCHAR InterfaceName_Microsoft_Windows_AI_MachineLearning_ICatalogModelInstanceResult[] = L"Microsoft.Windows.AI.MachineLearning.ICatalogModelInstanceResult";
namespace ABI {
    namespace Microsoft {
        namespace Windows {
            namespace AI {
                namespace MachineLearning {
                    MIDL_INTERFACE("b3701b71-61fa-5185-a2ce-8df6e6a5c66d")
                    ICatalogModelInstanceResult : public IInspectable
                    {
                    public:
                        virtual HRESULT STDMETHODCALLTYPE get_Status(
                            ABI::Microsoft::Windows::AI::MachineLearning::CatalogModelInstanceStatus* value
                            ) = 0;
                        virtual HRESULT STDMETHODCALLTYPE get_ExtendedError(
                            HRESULT* value
                            ) = 0;
                        virtual HRESULT STDMETHODCALLTYPE get_DiagnosticText(
                            HSTRING* value
                            ) = 0;
                        virtual HRESULT STDMETHODCALLTYPE GetInstance(
                            ABI::Microsoft::Windows::AI::MachineLearning::ICatalogModelInstance** result
                            ) = 0;
                    };

                    MIDL_CONST_ID IID& IID_ICatalogModelInstanceResult = _uuidof(ICatalogModelInstanceResult);
                } /* MachineLearning */
            } /* AI */
        } /* Windows */
    } /* Microsoft */
} /* ABI */

EXTERN_C const IID IID___x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResult;
#endif /* !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResult_INTERFACE_DEFINED__) */
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

/*
 *
 * Interface Microsoft.Windows.AI.MachineLearning.IExecutionProvider
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 1.0
 *
 * Interface is a part of the implementation of type Microsoft.Windows.AI.MachineLearning.ExecutionProvider
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000
#if !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider_INTERFACE_DEFINED__)
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider_INTERFACE_DEFINED__
extern const __declspec(selectany) _Null_terminated_ WCHAR InterfaceName_Microsoft_Windows_AI_MachineLearning_IExecutionProvider[] = L"Microsoft.Windows.AI.MachineLearning.IExecutionProvider";
namespace ABI {
    namespace Microsoft {
        namespace Windows {
            namespace AI {
                namespace MachineLearning {
                    MIDL_INTERFACE("98356468-cf23-504f-b29c-9347781925ff")
                    IExecutionProvider : public IInspectable
                    {
                    public:
                        virtual HRESULT STDMETHODCALLTYPE get_Name(
                            HSTRING* value
                            ) = 0;
                        virtual HRESULT STDMETHODCALLTYPE get_LibraryPath(
                            HSTRING* value
                            ) = 0;
                        virtual HRESULT STDMETHODCALLTYPE get_PackageId(
                            ABI::Windows::ApplicationModel::IPackageId** value
                            ) = 0;
                        virtual HRESULT STDMETHODCALLTYPE get_ReadyState(
                            ABI::Microsoft::Windows::AI::MachineLearning::ExecutionProviderReadyState* value
                            ) = 0;
                        virtual HRESULT STDMETHODCALLTYPE get_Certification(
                            ABI::Microsoft::Windows::AI::MachineLearning::ExecutionProviderCertification* value
                            ) = 0;
                        virtual HRESULT STDMETHODCALLTYPE EnsureReadyAsync(
                            __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double** operation
                            ) = 0;
                        virtual HRESULT STDMETHODCALLTYPE TryRegister(
                            boolean* result
                            ) = 0;
                    };

                    MIDL_CONST_ID IID& IID_IExecutionProvider = _uuidof(IExecutionProvider);
                } /* MachineLearning */
            } /* AI */
        } /* Windows */
    } /* Microsoft */
} /* ABI */

EXTERN_C const IID IID___x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider;
#endif /* !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider_INTERFACE_DEFINED__) */
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

/*
 *
 * Interface Microsoft.Windows.AI.MachineLearning.IExecutionProviderCatalog
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 1.0
 *
 * Interface is a part of the implementation of type Microsoft.Windows.AI.MachineLearning.ExecutionProviderCatalog
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000
#if !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalog_INTERFACE_DEFINED__)
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalog_INTERFACE_DEFINED__
extern const __declspec(selectany) _Null_terminated_ WCHAR InterfaceName_Microsoft_Windows_AI_MachineLearning_IExecutionProviderCatalog[] = L"Microsoft.Windows.AI.MachineLearning.IExecutionProviderCatalog";
namespace ABI {
    namespace Microsoft {
        namespace Windows {
            namespace AI {
                namespace MachineLearning {
                    MIDL_INTERFACE("aa9bfe14-2222-5921-8002-4d2a205ea03c")
                    IExecutionProviderCatalog : public IInspectable
                    {
                    public:
                        virtual HRESULT STDMETHODCALLTYPE FindAllProviders(
                            UINT32* resultLength,
                            ABI::Microsoft::Windows::AI::MachineLearning::IExecutionProvider*** result
                            ) = 0;
                        virtual HRESULT STDMETHODCALLTYPE EnsureAndRegisterCertifiedAsync(
                            __FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double** operation
                            ) = 0;
                        virtual HRESULT STDMETHODCALLTYPE RegisterCertifiedAsync(
                            __FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double** operation
                            ) = 0;
                    };

                    MIDL_CONST_ID IID& IID_IExecutionProviderCatalog = _uuidof(IExecutionProviderCatalog);
                } /* MachineLearning */
            } /* AI */
        } /* Windows */
    } /* Microsoft */
} /* ABI */

EXTERN_C const IID IID___x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalog;
#endif /* !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalog_INTERFACE_DEFINED__) */
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

/*
 *
 * Interface Microsoft.Windows.AI.MachineLearning.IExecutionProviderCatalogStatics
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 1.0
 *
 * Interface is a part of the implementation of type Microsoft.Windows.AI.MachineLearning.ExecutionProviderCatalog
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000
#if !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalogStatics_INTERFACE_DEFINED__)
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalogStatics_INTERFACE_DEFINED__
extern const __declspec(selectany) _Null_terminated_ WCHAR InterfaceName_Microsoft_Windows_AI_MachineLearning_IExecutionProviderCatalogStatics[] = L"Microsoft.Windows.AI.MachineLearning.IExecutionProviderCatalogStatics";
namespace ABI {
    namespace Microsoft {
        namespace Windows {
            namespace AI {
                namespace MachineLearning {
                    MIDL_INTERFACE("550def98-2611-5433-afb8-43673b610848")
                    IExecutionProviderCatalogStatics : public IInspectable
                    {
                    public:
                        virtual HRESULT STDMETHODCALLTYPE GetDefault(
                            ABI::Microsoft::Windows::AI::MachineLearning::IExecutionProviderCatalog** result
                            ) = 0;
                    };

                    MIDL_CONST_ID IID& IID_IExecutionProviderCatalogStatics = _uuidof(IExecutionProviderCatalogStatics);
                } /* MachineLearning */
            } /* AI */
        } /* Windows */
    } /* Microsoft */
} /* ABI */

EXTERN_C const IID IID___x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalogStatics;
#endif /* !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalogStatics_INTERFACE_DEFINED__) */
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

/*
 *
 * Interface Microsoft.Windows.AI.MachineLearning.IExecutionProviderReadyResult
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 1.0
 *
 * Interface is a part of the implementation of type Microsoft.Windows.AI.MachineLearning.ExecutionProviderReadyResult
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000
#if !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderReadyResult_INTERFACE_DEFINED__)
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderReadyResult_INTERFACE_DEFINED__
extern const __declspec(selectany) _Null_terminated_ WCHAR InterfaceName_Microsoft_Windows_AI_MachineLearning_IExecutionProviderReadyResult[] = L"Microsoft.Windows.AI.MachineLearning.IExecutionProviderReadyResult";
namespace ABI {
    namespace Microsoft {
        namespace Windows {
            namespace AI {
                namespace MachineLearning {
                    MIDL_INTERFACE("91c1724d-93c7-5284-adbe-ba2bd7be7c79")
                    IExecutionProviderReadyResult : public IInspectable
                    {
                    public:
                        virtual HRESULT STDMETHODCALLTYPE get_Status(
                            ABI::Microsoft::Windows::AI::MachineLearning::ExecutionProviderReadyResultState* value
                            ) = 0;
                        virtual HRESULT STDMETHODCALLTYPE get_ExtendedError(
                            HRESULT* value
                            ) = 0;
                        virtual HRESULT STDMETHODCALLTYPE get_DiagnosticText(
                            HSTRING* value
                            ) = 0;
                    };

                    MIDL_CONST_ID IID& IID_IExecutionProviderReadyResult = _uuidof(IExecutionProviderReadyResult);
                } /* MachineLearning */
            } /* AI */
        } /* Windows */
    } /* Microsoft */
} /* ABI */

EXTERN_C const IID IID___x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderReadyResult;
#endif /* !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderReadyResult_INTERFACE_DEFINED__) */
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

/*
 *
 * Interface Microsoft.Windows.AI.MachineLearning.IModelCatalog
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 2.0
 *
 * Interface is a part of the implementation of type Microsoft.Windows.AI.MachineLearning.ModelCatalog
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
#if !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog_INTERFACE_DEFINED__)
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog_INTERFACE_DEFINED__
extern const __declspec(selectany) _Null_terminated_ WCHAR InterfaceName_Microsoft_Windows_AI_MachineLearning_IModelCatalog[] = L"Microsoft.Windows.AI.MachineLearning.IModelCatalog";
namespace ABI {
    namespace Microsoft {
        namespace Windows {
            namespace AI {
                namespace MachineLearning {
                    MIDL_INTERFACE("8cae018c-49f5-5080-abb8-ef4a1df356da")
                    IModelCatalog : public IInspectable
                    {
                    public:
                        virtual HRESULT STDMETHODCALLTYPE get_Sources(
                            __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource** value
                            ) = 0;
                        virtual HRESULT STDMETHODCALLTYPE get_ExecutionProviders(
                            __FIVector_1_HSTRING** value
                            ) = 0;
                        virtual HRESULT STDMETHODCALLTYPE GetAvailableModel(
                            HSTRING idOrName,
                            ABI::Microsoft::Windows::AI::MachineLearning::ICatalogModelInfo** result
                            ) = 0;
                        virtual HRESULT STDMETHODCALLTYPE GetAvailableModels(
                            __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo** result
                            ) = 0;
                        virtual HRESULT STDMETHODCALLTYPE FindModelAsync(
                            HSTRING idOrName,
                            __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo** operation
                            ) = 0;
                        virtual HRESULT STDMETHODCALLTYPE FindAllModelsAsync(
                            __FIAsyncOperation_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo** operation
                            ) = 0;
                    };

                    MIDL_CONST_ID IID& IID_IModelCatalog = _uuidof(IModelCatalog);
                } /* MachineLearning */
            } /* AI */
        } /* Windows */
    } /* Microsoft */
} /* ABI */

EXTERN_C const IID IID___x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog;
#endif /* !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog_INTERFACE_DEFINED__) */
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

/*
 *
 * Interface Microsoft.Windows.AI.MachineLearning.IModelCatalogFactory
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 2.0
 *
 * Interface is a part of the implementation of type Microsoft.Windows.AI.MachineLearning.ModelCatalog
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
#if !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogFactory_INTERFACE_DEFINED__)
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogFactory_INTERFACE_DEFINED__
extern const __declspec(selectany) _Null_terminated_ WCHAR InterfaceName_Microsoft_Windows_AI_MachineLearning_IModelCatalogFactory[] = L"Microsoft.Windows.AI.MachineLearning.IModelCatalogFactory";
namespace ABI {
    namespace Microsoft {
        namespace Windows {
            namespace AI {
                namespace MachineLearning {
                    MIDL_INTERFACE("2942d7bd-6243-511f-a12c-42cb151b625f")
                    IModelCatalogFactory : public IInspectable
                    {
                    public:
                        virtual HRESULT STDMETHODCALLTYPE CreateInstance(
                            UINT32 sourcesLength,
                            ABI::Microsoft::Windows::AI::MachineLearning::IModelCatalogSource** sources,
                            ABI::Microsoft::Windows::AI::MachineLearning::IModelCatalog** value
                            ) = 0;
                    };

                    MIDL_CONST_ID IID& IID_IModelCatalogFactory = _uuidof(IModelCatalogFactory);
                } /* MachineLearning */
            } /* AI */
        } /* Windows */
    } /* Microsoft */
} /* ABI */

EXTERN_C const IID IID___x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogFactory;
#endif /* !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogFactory_INTERFACE_DEFINED__) */
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

/*
 *
 * Interface Microsoft.Windows.AI.MachineLearning.IModelCatalogSource
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 2.0
 *
 * Interface is a part of the implementation of type Microsoft.Windows.AI.MachineLearning.ModelCatalogSource
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
#if !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource_INTERFACE_DEFINED__)
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource_INTERFACE_DEFINED__
extern const __declspec(selectany) _Null_terminated_ WCHAR InterfaceName_Microsoft_Windows_AI_MachineLearning_IModelCatalogSource[] = L"Microsoft.Windows.AI.MachineLearning.IModelCatalogSource";
namespace ABI {
    namespace Microsoft {
        namespace Windows {
            namespace AI {
                namespace MachineLearning {
                    MIDL_INTERFACE("3117952c-8dc3-54c9-94fa-bf6f38cbfce9")
                    IModelCatalogSource : public IInspectable
                    {
                    public:
                        virtual HRESULT STDMETHODCALLTYPE get_Id(
                            HSTRING* value
                            ) = 0;
                        virtual HRESULT STDMETHODCALLTYPE get_Uri(
                            ABI::Windows::Foundation::IUriRuntimeClass** value
                            ) = 0;
                    };

                    MIDL_CONST_ID IID& IID_IModelCatalogSource = _uuidof(IModelCatalogSource);
                } /* MachineLearning */
            } /* AI */
        } /* Windows */
    } /* Microsoft */
} /* ABI */

EXTERN_C const IID IID___x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource;
#endif /* !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource_INTERFACE_DEFINED__) */
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

/*
 *
 * Interface Microsoft.Windows.AI.MachineLearning.IModelCatalogSourceStatics
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 2.0
 *
 * Interface is a part of the implementation of type Microsoft.Windows.AI.MachineLearning.ModelCatalogSource
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
#if !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSourceStatics_INTERFACE_DEFINED__)
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSourceStatics_INTERFACE_DEFINED__
extern const __declspec(selectany) _Null_terminated_ WCHAR InterfaceName_Microsoft_Windows_AI_MachineLearning_IModelCatalogSourceStatics[] = L"Microsoft.Windows.AI.MachineLearning.IModelCatalogSourceStatics";
namespace ABI {
    namespace Microsoft {
        namespace Windows {
            namespace AI {
                namespace MachineLearning {
                    MIDL_INTERFACE("deba0a9b-6eda-571a-9a05-5a7e2a0531ef")
                    IModelCatalogSourceStatics : public IInspectable
                    {
                    public:
                        virtual HRESULT STDMETHODCALLTYPE CreateFromUriAsync(
                            ABI::Windows::Foundation::IUriRuntimeClass* location,
                            __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource** operation
                            ) = 0;
                        virtual HRESULT STDMETHODCALLTYPE CreateFromUriAsync2(
                            ABI::Windows::Foundation::IUriRuntimeClass* location,
                            __FIIterable_1___FIKeyValuePair_2_HSTRING_HSTRING* additionalHeaders,
                            __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource** operation
                            ) = 0;
                    };

                    MIDL_CONST_ID IID& IID_IModelCatalogSourceStatics = _uuidof(IModelCatalogSourceStatics);
                } /* MachineLearning */
            } /* AI */
        } /* Windows */
    } /* Microsoft */
} /* ABI */

EXTERN_C const IID IID___x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSourceStatics;
#endif /* !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSourceStatics_INTERFACE_DEFINED__) */
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

/*
 *
 * Class Microsoft.Windows.AI.MachineLearning.CatalogModelInfo
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 2.0
 *
 * Class implements the following interfaces:
 *    Microsoft.Windows.AI.MachineLearning.ICatalogModelInfo ** Default Interface **
 *
 * Class Marshaling Behavior:  Agile - Class is agile
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
#ifndef RUNTIMECLASS_Microsoft_Windows_AI_MachineLearning_CatalogModelInfo_DEFINED
#define RUNTIMECLASS_Microsoft_Windows_AI_MachineLearning_CatalogModelInfo_DEFINED
extern const __declspec(selectany) _Null_terminated_ WCHAR RuntimeClass_Microsoft_Windows_AI_MachineLearning_CatalogModelInfo[] = L"Microsoft.Windows.AI.MachineLearning.CatalogModelInfo";
#endif
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

/*
 *
 * Class Microsoft.Windows.AI.MachineLearning.CatalogModelInstance
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 2.0
 *
 * Class implements the following interfaces:
 *    Microsoft.Windows.AI.MachineLearning.ICatalogModelInstance ** Default Interface **
 *    Windows.Foundation.IClosable
 *
 * Class Marshaling Behavior:  Agile - Class is agile
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
#ifndef RUNTIMECLASS_Microsoft_Windows_AI_MachineLearning_CatalogModelInstance_DEFINED
#define RUNTIMECLASS_Microsoft_Windows_AI_MachineLearning_CatalogModelInstance_DEFINED
extern const __declspec(selectany) _Null_terminated_ WCHAR RuntimeClass_Microsoft_Windows_AI_MachineLearning_CatalogModelInstance[] = L"Microsoft.Windows.AI.MachineLearning.CatalogModelInstance";
#endif
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

/*
 *
 * Class Microsoft.Windows.AI.MachineLearning.CatalogModelInstanceResult
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 2.0
 *
 * Class implements the following interfaces:
 *    Microsoft.Windows.AI.MachineLearning.ICatalogModelInstanceResult ** Default Interface **
 *
 * Class Marshaling Behavior:  Agile - Class is agile
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
#ifndef RUNTIMECLASS_Microsoft_Windows_AI_MachineLearning_CatalogModelInstanceResult_DEFINED
#define RUNTIMECLASS_Microsoft_Windows_AI_MachineLearning_CatalogModelInstanceResult_DEFINED
extern const __declspec(selectany) _Null_terminated_ WCHAR RuntimeClass_Microsoft_Windows_AI_MachineLearning_CatalogModelInstanceResult[] = L"Microsoft.Windows.AI.MachineLearning.CatalogModelInstanceResult";
#endif
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

/*
 *
 * Class Microsoft.Windows.AI.MachineLearning.ExecutionProvider
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 1.0
 *
 * Class implements the following interfaces:
 *    Microsoft.Windows.AI.MachineLearning.IExecutionProvider ** Default Interface **
 *
 * Class Marshaling Behavior:  Agile - Class is agile
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000
#ifndef RUNTIMECLASS_Microsoft_Windows_AI_MachineLearning_ExecutionProvider_DEFINED
#define RUNTIMECLASS_Microsoft_Windows_AI_MachineLearning_ExecutionProvider_DEFINED
extern const __declspec(selectany) _Null_terminated_ WCHAR RuntimeClass_Microsoft_Windows_AI_MachineLearning_ExecutionProvider[] = L"Microsoft.Windows.AI.MachineLearning.ExecutionProvider";
#endif
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

/*
 *
 * Class Microsoft.Windows.AI.MachineLearning.ExecutionProviderCatalog
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 1.0
 *
 * RuntimeClass contains static methods.
 *   Static Methods exist on the Microsoft.Windows.AI.MachineLearning.IExecutionProviderCatalogStatics interface starting with version 1.0 of the Microsoft.Windows.AI.MachineLearning.MachineLearningContract API contract
 *
 * Class implements the following interfaces:
 *    Microsoft.Windows.AI.MachineLearning.IExecutionProviderCatalog ** Default Interface **
 *
 * Class Threading Model:  Both Single and Multi Threaded Apartment
 *
 * Class Marshaling Behavior:  Agile - Class is agile
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000
#ifndef RUNTIMECLASS_Microsoft_Windows_AI_MachineLearning_ExecutionProviderCatalog_DEFINED
#define RUNTIMECLASS_Microsoft_Windows_AI_MachineLearning_ExecutionProviderCatalog_DEFINED
extern const __declspec(selectany) _Null_terminated_ WCHAR RuntimeClass_Microsoft_Windows_AI_MachineLearning_ExecutionProviderCatalog[] = L"Microsoft.Windows.AI.MachineLearning.ExecutionProviderCatalog";
#endif
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

/*
 *
 * Class Microsoft.Windows.AI.MachineLearning.ExecutionProviderReadyResult
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 1.0
 *
 * Class implements the following interfaces:
 *    Microsoft.Windows.AI.MachineLearning.IExecutionProviderReadyResult ** Default Interface **
 *
 * Class Marshaling Behavior:  Agile - Class is agile
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000
#ifndef RUNTIMECLASS_Microsoft_Windows_AI_MachineLearning_ExecutionProviderReadyResult_DEFINED
#define RUNTIMECLASS_Microsoft_Windows_AI_MachineLearning_ExecutionProviderReadyResult_DEFINED
extern const __declspec(selectany) _Null_terminated_ WCHAR RuntimeClass_Microsoft_Windows_AI_MachineLearning_ExecutionProviderReadyResult[] = L"Microsoft.Windows.AI.MachineLearning.ExecutionProviderReadyResult";
#endif
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

/*
 *
 * Class Microsoft.Windows.AI.MachineLearning.ModelCatalog
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 2.0
 *
 * RuntimeClass can be activated.
 *   Type can be activated via the Microsoft.Windows.AI.MachineLearning.IModelCatalogFactory interface starting with version 2.0 of the Microsoft.Windows.AI.MachineLearning.MachineLearningContract API contract
 *
 * Class implements the following interfaces:
 *    Microsoft.Windows.AI.MachineLearning.IModelCatalog ** Default Interface **
 *
 * Class Threading Model:  Both Single and Multi Threaded Apartment
 *
 * Class Marshaling Behavior:  Agile - Class is agile
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
#ifndef RUNTIMECLASS_Microsoft_Windows_AI_MachineLearning_ModelCatalog_DEFINED
#define RUNTIMECLASS_Microsoft_Windows_AI_MachineLearning_ModelCatalog_DEFINED
extern const __declspec(selectany) _Null_terminated_ WCHAR RuntimeClass_Microsoft_Windows_AI_MachineLearning_ModelCatalog[] = L"Microsoft.Windows.AI.MachineLearning.ModelCatalog";
#endif
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

/*
 *
 * Class Microsoft.Windows.AI.MachineLearning.ModelCatalogSource
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 2.0
 *
 * RuntimeClass contains static methods.
 *   Static Methods exist on the Microsoft.Windows.AI.MachineLearning.IModelCatalogSourceStatics interface starting with version 2.0 of the Microsoft.Windows.AI.MachineLearning.MachineLearningContract API contract
 *
 * Class implements the following interfaces:
 *    Microsoft.Windows.AI.MachineLearning.IModelCatalogSource ** Default Interface **
 *
 * Class Threading Model:  Both Single and Multi Threaded Apartment
 *
 * Class Marshaling Behavior:  Agile - Class is agile
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
#ifndef RUNTIMECLASS_Microsoft_Windows_AI_MachineLearning_ModelCatalogSource_DEFINED
#define RUNTIMECLASS_Microsoft_Windows_AI_MachineLearning_ModelCatalogSource_DEFINED
extern const __declspec(selectany) _Null_terminated_ WCHAR RuntimeClass_Microsoft_Windows_AI_MachineLearning_ModelCatalogSource[] = L"Microsoft.Windows.AI.MachineLearning.ModelCatalogSource";
#endif
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#else // !defined(__cplusplus)
/* Forward Declarations */
#ifndef ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo_FWD_DEFINED__
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo_FWD_DEFINED__
typedef interface __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo;

#endif // ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo_FWD_DEFINED__

#ifndef ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstance_FWD_DEFINED__
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstance_FWD_DEFINED__
typedef interface __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstance __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstance;

#endif // ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstance_FWD_DEFINED__

#ifndef ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResult_FWD_DEFINED__
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResult_FWD_DEFINED__
typedef interface __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResult __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResult;

#endif // ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResult_FWD_DEFINED__

#ifndef ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider_FWD_DEFINED__
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider_FWD_DEFINED__
typedef interface __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider;

#endif // ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider_FWD_DEFINED__

#ifndef ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalog_FWD_DEFINED__
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalog_FWD_DEFINED__
typedef interface __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalog __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalog;

#endif // ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalog_FWD_DEFINED__

#ifndef ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalogStatics_FWD_DEFINED__
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalogStatics_FWD_DEFINED__
typedef interface __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalogStatics __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalogStatics;

#endif // ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalogStatics_FWD_DEFINED__

#ifndef ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderReadyResult_FWD_DEFINED__
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderReadyResult_FWD_DEFINED__
typedef interface __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderReadyResult __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderReadyResult;

#endif // ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderReadyResult_FWD_DEFINED__

#ifndef ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog_FWD_DEFINED__
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog_FWD_DEFINED__
typedef interface __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog;

#endif // ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog_FWD_DEFINED__

#ifndef ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogFactory_FWD_DEFINED__
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogFactory_FWD_DEFINED__
typedef interface __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogFactory __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogFactory;

#endif // ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogFactory_FWD_DEFINED__

#ifndef ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource_FWD_DEFINED__
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource_FWD_DEFINED__
typedef interface __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource;

#endif // ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource_FWD_DEFINED__

#ifndef ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSourceStatics_FWD_DEFINED__
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSourceStatics_FWD_DEFINED__
typedef interface __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSourceStatics __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSourceStatics;

#endif // ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSourceStatics_FWD_DEFINED__

// Parameterized interface forward declarations (C)

// Collection interface definitions

typedef interface __FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo __FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo;

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
#if !defined(____FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_INTERFACE_DEFINED__)
#define ____FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_INTERFACE_DEFINED__

typedef interface __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo;

//  Declare the parameterized interface IID.
EXTERN_C const IID IID___FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo;

typedef struct __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfoVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This);
    ULONG (STDMETHODCALLTYPE* Release)(__FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This);
    HRESULT (STDMETHODCALLTYPE* GetIids)(__FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        ULONG* iidCount,
        IID** iids);
    HRESULT (STDMETHODCALLTYPE* GetRuntimeClassName)(__FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        HSTRING* className);
    HRESULT (STDMETHODCALLTYPE* GetTrustLevel)(__FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        TrustLevel* trustLevel);
    HRESULT (STDMETHODCALLTYPE* put_Completed)(__FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        __FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* handler);
    HRESULT (STDMETHODCALLTYPE* get_Completed)(__FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        __FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo** result);
    HRESULT (STDMETHODCALLTYPE* GetResults)(__FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo** result);

    END_INTERFACE
} __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfoVtbl;

interface __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo
{
    CONST_VTBL struct __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfoVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_GetIids(This, iidCount, iids) \
    ((This)->lpVtbl->GetIids(This, iidCount, iids))

#define __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_GetRuntimeClassName(This, className) \
    ((This)->lpVtbl->GetRuntimeClassName(This, className))

#define __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_GetTrustLevel(This, trustLevel) \
    ((This)->lpVtbl->GetTrustLevel(This, trustLevel))

#define __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_put_Completed(This, handler) \
    ((This)->lpVtbl->put_Completed(This, handler))

#define __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_get_Completed(This, result) \
    ((This)->lpVtbl->get_Completed(This, result))

#define __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_GetResults(This, result) \
    ((This)->lpVtbl->GetResults(This, result))

#endif /* COBJMACROS */

#endif // ____FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_INTERFACE_DEFINED__
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
#if !defined(____FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_INTERFACE_DEFINED__)
#define ____FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_INTERFACE_DEFINED__

typedef interface __FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo __FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo;

//  Declare the parameterized interface IID.
EXTERN_C const IID IID___FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo;

typedef struct __FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfoVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This);
    ULONG (STDMETHODCALLTYPE* Release)(__FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This);
    HRESULT (STDMETHODCALLTYPE* Invoke)(__FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* asyncInfo,
        AsyncStatus asyncStatus);

    END_INTERFACE
} __FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfoVtbl;

interface __FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo
{
    CONST_VTBL struct __FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfoVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_Invoke(This, asyncInfo, asyncStatus) \
    ((This)->lpVtbl->Invoke(This, asyncInfo, asyncStatus))

#endif /* COBJMACROS */

#endif // ____FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_INTERFACE_DEFINED__
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

typedef interface __FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource __FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource;

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
#if !defined(____FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_INTERFACE_DEFINED__)
#define ____FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_INTERFACE_DEFINED__

typedef interface __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource;

//  Declare the parameterized interface IID.
EXTERN_C const IID IID___FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource;

typedef struct __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSourceVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This);
    ULONG (STDMETHODCALLTYPE* Release)(__FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This);
    HRESULT (STDMETHODCALLTYPE* GetIids)(__FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        ULONG* iidCount,
        IID** iids);
    HRESULT (STDMETHODCALLTYPE* GetRuntimeClassName)(__FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        HSTRING* className);
    HRESULT (STDMETHODCALLTYPE* GetTrustLevel)(__FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        TrustLevel* trustLevel);
    HRESULT (STDMETHODCALLTYPE* put_Completed)(__FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        __FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* handler);
    HRESULT (STDMETHODCALLTYPE* get_Completed)(__FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        __FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource** result);
    HRESULT (STDMETHODCALLTYPE* GetResults)(__FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource** result);

    END_INTERFACE
} __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSourceVtbl;

interface __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource
{
    CONST_VTBL struct __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSourceVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_GetIids(This, iidCount, iids) \
    ((This)->lpVtbl->GetIids(This, iidCount, iids))

#define __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_GetRuntimeClassName(This, className) \
    ((This)->lpVtbl->GetRuntimeClassName(This, className))

#define __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_GetTrustLevel(This, trustLevel) \
    ((This)->lpVtbl->GetTrustLevel(This, trustLevel))

#define __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_put_Completed(This, handler) \
    ((This)->lpVtbl->put_Completed(This, handler))

#define __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_get_Completed(This, result) \
    ((This)->lpVtbl->get_Completed(This, result))

#define __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_GetResults(This, result) \
    ((This)->lpVtbl->GetResults(This, result))

#endif /* COBJMACROS */

#endif // ____FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_INTERFACE_DEFINED__
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
#if !defined(____FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_INTERFACE_DEFINED__)
#define ____FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_INTERFACE_DEFINED__

typedef interface __FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource __FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource;

//  Declare the parameterized interface IID.
EXTERN_C const IID IID___FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource;

typedef struct __FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSourceVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This);
    ULONG (STDMETHODCALLTYPE* Release)(__FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This);
    HRESULT (STDMETHODCALLTYPE* Invoke)(__FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* asyncInfo,
        AsyncStatus asyncStatus);

    END_INTERFACE
} __FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSourceVtbl;

interface __FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource
{
    CONST_VTBL struct __FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSourceVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_Invoke(This, asyncInfo, asyncStatus) \
    ((This)->lpVtbl->Invoke(This, asyncInfo, asyncStatus))

#endif /* COBJMACROS */

#endif // ____FIAsyncOperationCompletedHandler_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_INTERFACE_DEFINED__
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
#if !defined(____FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_INTERFACE_DEFINED__)
#define ____FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_INTERFACE_DEFINED__

typedef interface __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo;

//  Declare the parameterized interface IID.
EXTERN_C const IID IID___FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo;

typedef struct __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfoVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This);
    ULONG (STDMETHODCALLTYPE* Release)(__FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This);
    HRESULT (STDMETHODCALLTYPE* GetIids)(__FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        ULONG* iidCount,
        IID** iids);
    HRESULT (STDMETHODCALLTYPE* GetRuntimeClassName)(__FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        HSTRING* className);
    HRESULT (STDMETHODCALLTYPE* GetTrustLevel)(__FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        TrustLevel* trustLevel);
    HRESULT (STDMETHODCALLTYPE* get_Current)(__FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo** result);
    HRESULT (STDMETHODCALLTYPE* get_HasCurrent)(__FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        boolean* result);
    HRESULT (STDMETHODCALLTYPE* MoveNext)(__FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        boolean* result);
    HRESULT (STDMETHODCALLTYPE* GetMany)(__FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        UINT32 itemsLength,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo** items,
        UINT32* result);

    END_INTERFACE
} __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfoVtbl;

interface __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo
{
    CONST_VTBL struct __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfoVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_GetIids(This, iidCount, iids) \
    ((This)->lpVtbl->GetIids(This, iidCount, iids))

#define __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_GetRuntimeClassName(This, className) \
    ((This)->lpVtbl->GetRuntimeClassName(This, className))

#define __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_GetTrustLevel(This, trustLevel) \
    ((This)->lpVtbl->GetTrustLevel(This, trustLevel))

#define __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_get_Current(This, result) \
    ((This)->lpVtbl->get_Current(This, result))

#define __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_get_HasCurrent(This, result) \
    ((This)->lpVtbl->get_HasCurrent(This, result))

#define __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_MoveNext(This, result) \
    ((This)->lpVtbl->MoveNext(This, result))

#define __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_GetMany(This, itemsLength, items, result) \
    ((This)->lpVtbl->GetMany(This, itemsLength, items, result))

#endif /* COBJMACROS */

#endif // ____FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_INTERFACE_DEFINED__
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
#if !defined(____FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_INTERFACE_DEFINED__)
#define ____FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_INTERFACE_DEFINED__

typedef interface __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo;

//  Declare the parameterized interface IID.
EXTERN_C const IID IID___FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo;

typedef struct __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfoVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This);
    ULONG (STDMETHODCALLTYPE* Release)(__FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This);
    HRESULT (STDMETHODCALLTYPE* GetIids)(__FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        ULONG* iidCount,
        IID** iids);
    HRESULT (STDMETHODCALLTYPE* GetRuntimeClassName)(__FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        HSTRING* className);
    HRESULT (STDMETHODCALLTYPE* GetTrustLevel)(__FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        TrustLevel* trustLevel);
    HRESULT (STDMETHODCALLTYPE* First)(__FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo** result);

    END_INTERFACE
} __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfoVtbl;

interface __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo
{
    CONST_VTBL struct __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfoVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_GetIids(This, iidCount, iids) \
    ((This)->lpVtbl->GetIids(This, iidCount, iids))

#define __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_GetRuntimeClassName(This, className) \
    ((This)->lpVtbl->GetRuntimeClassName(This, className))

#define __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_GetTrustLevel(This, trustLevel) \
    ((This)->lpVtbl->GetTrustLevel(This, trustLevel))

#define __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_First(This, result) \
    ((This)->lpVtbl->First(This, result))

#endif /* COBJMACROS */

#endif // ____FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_INTERFACE_DEFINED__
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
#if !defined(____FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_INTERFACE_DEFINED__)
#define ____FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_INTERFACE_DEFINED__

typedef interface __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo;

//  Declare the parameterized interface IID.
EXTERN_C const IID IID___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo;

typedef struct __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfoVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This);
    ULONG (STDMETHODCALLTYPE* Release)(__FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This);
    HRESULT (STDMETHODCALLTYPE* GetIids)(__FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        ULONG* iidCount,
        IID** iids);
    HRESULT (STDMETHODCALLTYPE* GetRuntimeClassName)(__FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        HSTRING* className);
    HRESULT (STDMETHODCALLTYPE* GetTrustLevel)(__FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        TrustLevel* trustLevel);
    HRESULT (STDMETHODCALLTYPE* GetAt)(__FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        UINT32 index,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo** result);
    HRESULT (STDMETHODCALLTYPE* get_Size)(__FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        UINT32* result);
    HRESULT (STDMETHODCALLTYPE* IndexOf)(__FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo* value,
        UINT32* index,
        boolean* result);
    HRESULT (STDMETHODCALLTYPE* GetMany)(__FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        UINT32 startIndex,
        UINT32 itemsLength,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo** items,
        UINT32* result);

    END_INTERFACE
} __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfoVtbl;

interface __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo
{
    CONST_VTBL struct __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfoVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_GetIids(This, iidCount, iids) \
    ((This)->lpVtbl->GetIids(This, iidCount, iids))

#define __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_GetRuntimeClassName(This, className) \
    ((This)->lpVtbl->GetRuntimeClassName(This, className))

#define __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_GetTrustLevel(This, trustLevel) \
    ((This)->lpVtbl->GetTrustLevel(This, trustLevel))

#define __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_GetAt(This, index, result) \
    ((This)->lpVtbl->GetAt(This, index, result))

#define __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_get_Size(This, result) \
    ((This)->lpVtbl->get_Size(This, result))

#define __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_IndexOf(This, value, index, result) \
    ((This)->lpVtbl->IndexOf(This, value, index, result))

#define __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_GetMany(This, startIndex, itemsLength, items, result) \
    ((This)->lpVtbl->GetMany(This, startIndex, itemsLength, items, result))

#endif /* COBJMACROS */

#endif // ____FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_INTERFACE_DEFINED__
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

typedef interface __FIAsyncOperationCompletedHandler_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo __FIAsyncOperationCompletedHandler_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo;

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
#if !defined(____FIAsyncOperation_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_INTERFACE_DEFINED__)
#define ____FIAsyncOperation_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_INTERFACE_DEFINED__

typedef interface __FIAsyncOperation_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo __FIAsyncOperation_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo;

//  Declare the parameterized interface IID.
EXTERN_C const IID IID___FIAsyncOperation_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo;

typedef struct __FIAsyncOperation_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfoVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__FIAsyncOperation_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__FIAsyncOperation_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This);
    ULONG (STDMETHODCALLTYPE* Release)(__FIAsyncOperation_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This);
    HRESULT (STDMETHODCALLTYPE* GetIids)(__FIAsyncOperation_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        ULONG* iidCount,
        IID** iids);
    HRESULT (STDMETHODCALLTYPE* GetRuntimeClassName)(__FIAsyncOperation_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        HSTRING* className);
    HRESULT (STDMETHODCALLTYPE* GetTrustLevel)(__FIAsyncOperation_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        TrustLevel* trustLevel);
    HRESULT (STDMETHODCALLTYPE* put_Completed)(__FIAsyncOperation_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        __FIAsyncOperationCompletedHandler_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* handler);
    HRESULT (STDMETHODCALLTYPE* get_Completed)(__FIAsyncOperation_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        __FIAsyncOperationCompletedHandler_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo** result);
    HRESULT (STDMETHODCALLTYPE* GetResults)(__FIAsyncOperation_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo** result);

    END_INTERFACE
} __FIAsyncOperation_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfoVtbl;

interface __FIAsyncOperation_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo
{
    CONST_VTBL struct __FIAsyncOperation_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfoVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __FIAsyncOperation_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __FIAsyncOperation_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __FIAsyncOperation_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __FIAsyncOperation_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_GetIids(This, iidCount, iids) \
    ((This)->lpVtbl->GetIids(This, iidCount, iids))

#define __FIAsyncOperation_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_GetRuntimeClassName(This, className) \
    ((This)->lpVtbl->GetRuntimeClassName(This, className))

#define __FIAsyncOperation_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_GetTrustLevel(This, trustLevel) \
    ((This)->lpVtbl->GetTrustLevel(This, trustLevel))

#define __FIAsyncOperation_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_put_Completed(This, handler) \
    ((This)->lpVtbl->put_Completed(This, handler))

#define __FIAsyncOperation_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_get_Completed(This, result) \
    ((This)->lpVtbl->get_Completed(This, result))

#define __FIAsyncOperation_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_GetResults(This, result) \
    ((This)->lpVtbl->GetResults(This, result))

#endif /* COBJMACROS */

#endif // ____FIAsyncOperation_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_INTERFACE_DEFINED__
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
#if !defined(____FIAsyncOperationCompletedHandler_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_INTERFACE_DEFINED__)
#define ____FIAsyncOperationCompletedHandler_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_INTERFACE_DEFINED__

typedef interface __FIAsyncOperationCompletedHandler_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo __FIAsyncOperationCompletedHandler_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo;

//  Declare the parameterized interface IID.
EXTERN_C const IID IID___FIAsyncOperationCompletedHandler_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo;

typedef struct __FIAsyncOperationCompletedHandler_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfoVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__FIAsyncOperationCompletedHandler_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__FIAsyncOperationCompletedHandler_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This);
    ULONG (STDMETHODCALLTYPE* Release)(__FIAsyncOperationCompletedHandler_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This);
    HRESULT (STDMETHODCALLTYPE* Invoke)(__FIAsyncOperationCompletedHandler_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        __FIAsyncOperation_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* asyncInfo,
        AsyncStatus asyncStatus);

    END_INTERFACE
} __FIAsyncOperationCompletedHandler_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfoVtbl;

interface __FIAsyncOperationCompletedHandler_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo
{
    CONST_VTBL struct __FIAsyncOperationCompletedHandler_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfoVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __FIAsyncOperationCompletedHandler_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __FIAsyncOperationCompletedHandler_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __FIAsyncOperationCompletedHandler_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __FIAsyncOperationCompletedHandler_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_Invoke(This, asyncInfo, asyncStatus) \
    ((This)->lpVtbl->Invoke(This, asyncInfo, asyncStatus))

#endif /* COBJMACROS */

#endif // ____FIAsyncOperationCompletedHandler_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_INTERFACE_DEFINED__
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

typedef interface __FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double __FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double;

typedef interface __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double;

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
#if !defined(____FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_INTERFACE_DEFINED__)
#define ____FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_INTERFACE_DEFINED__

typedef interface __FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double __FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double;

//  Declare the parameterized interface IID.
EXTERN_C const IID IID___FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double;

typedef struct __FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_doubleVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double* This);
    ULONG (STDMETHODCALLTYPE* Release)(__FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double* This);
    HRESULT (STDMETHODCALLTYPE* Invoke)(__FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double* This,
        __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double* asyncInfo,
        AsyncStatus asyncStatus);

    END_INTERFACE
} __FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_doubleVtbl;

interface __FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double
{
    CONST_VTBL struct __FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_doubleVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_Invoke(This, asyncInfo, asyncStatus) \
    ((This)->lpVtbl->Invoke(This, asyncInfo, asyncStatus))

#endif /* COBJMACROS */

#endif // ____FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_INTERFACE_DEFINED__
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
#if !defined(____FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_INTERFACE_DEFINED__)
#define ____FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_INTERFACE_DEFINED__

typedef interface __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double;

//  Declare the parameterized interface IID.
EXTERN_C const IID IID___FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double;

typedef struct __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_doubleVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double* This);
    ULONG (STDMETHODCALLTYPE* Release)(__FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double* This);
    HRESULT (STDMETHODCALLTYPE* GetIids)(__FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double* This,
        ULONG* iidCount,
        IID** iids);
    HRESULT (STDMETHODCALLTYPE* GetRuntimeClassName)(__FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double* This,
        HSTRING* className);
    HRESULT (STDMETHODCALLTYPE* GetTrustLevel)(__FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double* This,
        TrustLevel* trustLevel);
    HRESULT (STDMETHODCALLTYPE* put_Progress)(__FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double* This,
        __FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double* handler);
    HRESULT (STDMETHODCALLTYPE* get_Progress)(__FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double* This,
        __FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double** result);
    HRESULT (STDMETHODCALLTYPE* put_Completed)(__FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double* This,
        __FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double* handler);
    HRESULT (STDMETHODCALLTYPE* get_Completed)(__FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double* This,
        __FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double** result);
    HRESULT (STDMETHODCALLTYPE* GetResults)(__FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double* This,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResult** result);

    END_INTERFACE
} __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_doubleVtbl;

interface __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double
{
    CONST_VTBL struct __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_doubleVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_GetIids(This, iidCount, iids) \
    ((This)->lpVtbl->GetIids(This, iidCount, iids))

#define __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_GetRuntimeClassName(This, className) \
    ((This)->lpVtbl->GetRuntimeClassName(This, className))

#define __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_GetTrustLevel(This, trustLevel) \
    ((This)->lpVtbl->GetTrustLevel(This, trustLevel))

#define __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_put_Progress(This, handler) \
    ((This)->lpVtbl->put_Progress(This, handler))

#define __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_get_Progress(This, result) \
    ((This)->lpVtbl->get_Progress(This, result))

#define __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_put_Completed(This, handler) \
    ((This)->lpVtbl->put_Completed(This, handler))

#define __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_get_Completed(This, result) \
    ((This)->lpVtbl->get_Completed(This, result))

#define __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_GetResults(This, result) \
    ((This)->lpVtbl->GetResults(This, result))

#endif /* COBJMACROS */

#endif // ____FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_INTERFACE_DEFINED__
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
#if !defined(____FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_INTERFACE_DEFINED__)
#define ____FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_INTERFACE_DEFINED__

typedef interface __FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double __FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double;

//  Declare the parameterized interface IID.
EXTERN_C const IID IID___FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double;

typedef struct __FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_doubleVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double* This);
    ULONG (STDMETHODCALLTYPE* Release)(__FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double* This);
    HRESULT (STDMETHODCALLTYPE* Invoke)(__FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double* This,
        __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double* asyncInfo,
        DOUBLE progressInfo);

    END_INTERFACE
} __FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_doubleVtbl;

interface __FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double
{
    CONST_VTBL struct __FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_doubleVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_Invoke(This, asyncInfo, progressInfo) \
    ((This)->lpVtbl->Invoke(This, asyncInfo, progressInfo))

#endif /* COBJMACROS */

#endif // ____FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double_INTERFACE_DEFINED__
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

typedef interface __FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double __FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double;

typedef interface __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double;

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000
#if !defined(____FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_INTERFACE_DEFINED__)
#define ____FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_INTERFACE_DEFINED__

typedef interface __FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double __FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double;

//  Declare the parameterized interface IID.
EXTERN_C const IID IID___FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double;

typedef struct __FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_doubleVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double* This);
    ULONG (STDMETHODCALLTYPE* Release)(__FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double* This);
    HRESULT (STDMETHODCALLTYPE* Invoke)(__FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double* This,
        __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double* asyncInfo,
        AsyncStatus asyncStatus);

    END_INTERFACE
} __FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_doubleVtbl;

interface __FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double
{
    CONST_VTBL struct __FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_doubleVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_Invoke(This, asyncInfo, asyncStatus) \
    ((This)->lpVtbl->Invoke(This, asyncInfo, asyncStatus))

#endif /* COBJMACROS */

#endif // ____FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_INTERFACE_DEFINED__
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000
#if !defined(____FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_INTERFACE_DEFINED__)
#define ____FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_INTERFACE_DEFINED__

typedef interface __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double;

//  Declare the parameterized interface IID.
EXTERN_C const IID IID___FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double;

typedef struct __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_doubleVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double* This);
    ULONG (STDMETHODCALLTYPE* Release)(__FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double* This);
    HRESULT (STDMETHODCALLTYPE* GetIids)(__FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double* This,
        ULONG* iidCount,
        IID** iids);
    HRESULT (STDMETHODCALLTYPE* GetRuntimeClassName)(__FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double* This,
        HSTRING* className);
    HRESULT (STDMETHODCALLTYPE* GetTrustLevel)(__FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double* This,
        TrustLevel* trustLevel);
    HRESULT (STDMETHODCALLTYPE* put_Progress)(__FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double* This,
        __FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double* handler);
    HRESULT (STDMETHODCALLTYPE* get_Progress)(__FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double* This,
        __FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double** result);
    HRESULT (STDMETHODCALLTYPE* put_Completed)(__FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double* This,
        __FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double* handler);
    HRESULT (STDMETHODCALLTYPE* get_Completed)(__FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double* This,
        __FIAsyncOperationWithProgressCompletedHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double** result);
    HRESULT (STDMETHODCALLTYPE* GetResults)(__FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double* This,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderReadyResult** result);

    END_INTERFACE
} __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_doubleVtbl;

interface __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double
{
    CONST_VTBL struct __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_doubleVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_GetIids(This, iidCount, iids) \
    ((This)->lpVtbl->GetIids(This, iidCount, iids))

#define __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_GetRuntimeClassName(This, className) \
    ((This)->lpVtbl->GetRuntimeClassName(This, className))

#define __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_GetTrustLevel(This, trustLevel) \
    ((This)->lpVtbl->GetTrustLevel(This, trustLevel))

#define __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_put_Progress(This, handler) \
    ((This)->lpVtbl->put_Progress(This, handler))

#define __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_get_Progress(This, result) \
    ((This)->lpVtbl->get_Progress(This, result))

#define __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_put_Completed(This, handler) \
    ((This)->lpVtbl->put_Completed(This, handler))

#define __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_get_Completed(This, result) \
    ((This)->lpVtbl->get_Completed(This, result))

#define __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_GetResults(This, result) \
    ((This)->lpVtbl->GetResults(This, result))

#endif /* COBJMACROS */

#endif // ____FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_INTERFACE_DEFINED__
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000
#if !defined(____FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_INTERFACE_DEFINED__)
#define ____FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_INTERFACE_DEFINED__

typedef interface __FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double __FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double;

//  Declare the parameterized interface IID.
EXTERN_C const IID IID___FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double;

typedef struct __FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_doubleVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double* This);
    ULONG (STDMETHODCALLTYPE* Release)(__FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double* This);
    HRESULT (STDMETHODCALLTYPE* Invoke)(__FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double* This,
        __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double* asyncInfo,
        DOUBLE progressInfo);

    END_INTERFACE
} __FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_doubleVtbl;

interface __FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double
{
    CONST_VTBL struct __FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_doubleVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_Invoke(This, asyncInfo, progressInfo) \
    ((This)->lpVtbl->Invoke(This, asyncInfo, progressInfo))

#endif /* COBJMACROS */

#endif // ____FIAsyncOperationProgressHandler_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double_INTERFACE_DEFINED__
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000
#if !defined(____FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_INTERFACE_DEFINED__)
#define ____FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_INTERFACE_DEFINED__

typedef interface __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider;

//  Declare the parameterized interface IID.
EXTERN_C const IID IID___FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider;

typedef struct __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This);
    ULONG (STDMETHODCALLTYPE* Release)(__FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This);
    HRESULT (STDMETHODCALLTYPE* GetIids)(__FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This,
        ULONG* iidCount,
        IID** iids);
    HRESULT (STDMETHODCALLTYPE* GetRuntimeClassName)(__FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This,
        HSTRING* className);
    HRESULT (STDMETHODCALLTYPE* GetTrustLevel)(__FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This,
        TrustLevel* trustLevel);
    HRESULT (STDMETHODCALLTYPE* get_Current)(__FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider** result);
    HRESULT (STDMETHODCALLTYPE* get_HasCurrent)(__FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This,
        boolean* result);
    HRESULT (STDMETHODCALLTYPE* MoveNext)(__FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This,
        boolean* result);
    HRESULT (STDMETHODCALLTYPE* GetMany)(__FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This,
        UINT32 itemsLength,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider** items,
        UINT32* result);

    END_INTERFACE
} __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderVtbl;

interface __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider
{
    CONST_VTBL struct __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_GetIids(This, iidCount, iids) \
    ((This)->lpVtbl->GetIids(This, iidCount, iids))

#define __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_GetRuntimeClassName(This, className) \
    ((This)->lpVtbl->GetRuntimeClassName(This, className))

#define __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_GetTrustLevel(This, trustLevel) \
    ((This)->lpVtbl->GetTrustLevel(This, trustLevel))

#define __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_get_Current(This, result) \
    ((This)->lpVtbl->get_Current(This, result))

#define __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_get_HasCurrent(This, result) \
    ((This)->lpVtbl->get_HasCurrent(This, result))

#define __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_MoveNext(This, result) \
    ((This)->lpVtbl->MoveNext(This, result))

#define __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_GetMany(This, itemsLength, items, result) \
    ((This)->lpVtbl->GetMany(This, itemsLength, items, result))

#endif /* COBJMACROS */

#endif // ____FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_INTERFACE_DEFINED__
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000
#if !defined(____FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_INTERFACE_DEFINED__)
#define ____FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_INTERFACE_DEFINED__

typedef interface __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider;

//  Declare the parameterized interface IID.
EXTERN_C const IID IID___FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider;

typedef struct __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This);
    ULONG (STDMETHODCALLTYPE* Release)(__FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This);
    HRESULT (STDMETHODCALLTYPE* GetIids)(__FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This,
        ULONG* iidCount,
        IID** iids);
    HRESULT (STDMETHODCALLTYPE* GetRuntimeClassName)(__FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This,
        HSTRING* className);
    HRESULT (STDMETHODCALLTYPE* GetTrustLevel)(__FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This,
        TrustLevel* trustLevel);
    HRESULT (STDMETHODCALLTYPE* First)(__FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This,
        __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider** result);

    END_INTERFACE
} __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderVtbl;

interface __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider
{
    CONST_VTBL struct __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_GetIids(This, iidCount, iids) \
    ((This)->lpVtbl->GetIids(This, iidCount, iids))

#define __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_GetRuntimeClassName(This, className) \
    ((This)->lpVtbl->GetRuntimeClassName(This, className))

#define __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_GetTrustLevel(This, trustLevel) \
    ((This)->lpVtbl->GetTrustLevel(This, trustLevel))

#define __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_First(This, result) \
    ((This)->lpVtbl->First(This, result))

#endif /* COBJMACROS */

#endif // ____FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_INTERFACE_DEFINED__
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000
#if !defined(____FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_INTERFACE_DEFINED__)
#define ____FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_INTERFACE_DEFINED__

typedef interface __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider;

//  Declare the parameterized interface IID.
EXTERN_C const IID IID___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider;

typedef struct __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This);
    ULONG (STDMETHODCALLTYPE* Release)(__FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This);
    HRESULT (STDMETHODCALLTYPE* GetIids)(__FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This,
        ULONG* iidCount,
        IID** iids);
    HRESULT (STDMETHODCALLTYPE* GetRuntimeClassName)(__FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This,
        HSTRING* className);
    HRESULT (STDMETHODCALLTYPE* GetTrustLevel)(__FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This,
        TrustLevel* trustLevel);
    HRESULT (STDMETHODCALLTYPE* GetAt)(__FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This,
        UINT32 index,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider** result);
    HRESULT (STDMETHODCALLTYPE* get_Size)(__FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This,
        UINT32* result);
    HRESULT (STDMETHODCALLTYPE* IndexOf)(__FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider* value,
        UINT32* index,
        boolean* result);
    HRESULT (STDMETHODCALLTYPE* GetMany)(__FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This,
        UINT32 startIndex,
        UINT32 itemsLength,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider** items,
        UINT32* result);

    END_INTERFACE
} __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderVtbl;

interface __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider
{
    CONST_VTBL struct __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_GetIids(This, iidCount, iids) \
    ((This)->lpVtbl->GetIids(This, iidCount, iids))

#define __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_GetRuntimeClassName(This, className) \
    ((This)->lpVtbl->GetRuntimeClassName(This, className))

#define __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_GetTrustLevel(This, trustLevel) \
    ((This)->lpVtbl->GetTrustLevel(This, trustLevel))

#define __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_GetAt(This, index, result) \
    ((This)->lpVtbl->GetAt(This, index, result))

#define __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_get_Size(This, result) \
    ((This)->lpVtbl->get_Size(This, result))

#define __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_IndexOf(This, value, index, result) \
    ((This)->lpVtbl->IndexOf(This, value, index, result))

#define __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_GetMany(This, startIndex, itemsLength, items, result) \
    ((This)->lpVtbl->GetMany(This, startIndex, itemsLength, items, result))

#endif /* COBJMACROS */

#endif // ____FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_INTERFACE_DEFINED__
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000
#if !defined(____FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_INTERFACE_DEFINED__)
#define ____FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_INTERFACE_DEFINED__

typedef interface __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider;

//  Declare the parameterized interface IID.
EXTERN_C const IID IID___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider;

typedef struct __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This);
    ULONG (STDMETHODCALLTYPE* Release)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This);
    HRESULT (STDMETHODCALLTYPE* GetIids)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This,
        ULONG* iidCount,
        IID** iids);
    HRESULT (STDMETHODCALLTYPE* GetRuntimeClassName)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This,
        HSTRING* className);
    HRESULT (STDMETHODCALLTYPE* GetTrustLevel)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This,
        TrustLevel* trustLevel);
    HRESULT (STDMETHODCALLTYPE* GetAt)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This,
        UINT32 index,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider** result);
    HRESULT (STDMETHODCALLTYPE* get_Size)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This,
        UINT32* result);
    HRESULT (STDMETHODCALLTYPE* GetView)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This,
        __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider** result);
    HRESULT (STDMETHODCALLTYPE* IndexOf)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider* value,
        UINT32* index,
        boolean* result);
    HRESULT (STDMETHODCALLTYPE* SetAt)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This,
        UINT32 index,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider* value);
    HRESULT (STDMETHODCALLTYPE* InsertAt)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This,
        UINT32 index,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider* value);
    HRESULT (STDMETHODCALLTYPE* RemoveAt)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This,
        UINT32 index);
    HRESULT (STDMETHODCALLTYPE* Append)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider* value);
    HRESULT (STDMETHODCALLTYPE* RemoveAtEnd)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This);
    HRESULT (STDMETHODCALLTYPE* Clear)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This);
    HRESULT (STDMETHODCALLTYPE* GetMany)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This,
        UINT32 startIndex,
        UINT32 itemsLength,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider** items,
        UINT32* result);
    HRESULT (STDMETHODCALLTYPE* ReplaceAll)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider* This,
        UINT32 itemsLength,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider** items);

    END_INTERFACE
} __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderVtbl;

interface __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider
{
    CONST_VTBL struct __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_GetIids(This, iidCount, iids) \
    ((This)->lpVtbl->GetIids(This, iidCount, iids))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_GetRuntimeClassName(This, className) \
    ((This)->lpVtbl->GetRuntimeClassName(This, className))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_GetTrustLevel(This, trustLevel) \
    ((This)->lpVtbl->GetTrustLevel(This, trustLevel))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_GetAt(This, index, result) \
    ((This)->lpVtbl->GetAt(This, index, result))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_get_Size(This, result) \
    ((This)->lpVtbl->get_Size(This, result))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_GetView(This, result) \
    ((This)->lpVtbl->GetView(This, result))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_IndexOf(This, value, index, result) \
    ((This)->lpVtbl->IndexOf(This, value, index, result))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_SetAt(This, index, value) \
    ((This)->lpVtbl->SetAt(This, index, value))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_InsertAt(This, index, value) \
    ((This)->lpVtbl->InsertAt(This, index, value))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_RemoveAt(This, index) \
    ((This)->lpVtbl->RemoveAt(This, index))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_Append(This, value) \
    ((This)->lpVtbl->Append(This, value))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_RemoveAtEnd(This) \
    ((This)->lpVtbl->RemoveAtEnd(This))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_Clear(This) \
    ((This)->lpVtbl->Clear(This))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_GetMany(This, startIndex, itemsLength, items, result) \
    ((This)->lpVtbl->GetMany(This, startIndex, itemsLength, items, result))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_ReplaceAll(This, itemsLength, items) \
    ((This)->lpVtbl->ReplaceAll(This, itemsLength, items))

#endif /* COBJMACROS */

#endif // ____FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_INTERFACE_DEFINED__
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

typedef interface __FIAsyncOperationProgressHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double __FIAsyncOperationProgressHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double;

typedef interface __FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double __FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double;

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000
#if !defined(____FIAsyncOperationWithProgressCompletedHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_INTERFACE_DEFINED__)
#define ____FIAsyncOperationWithProgressCompletedHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_INTERFACE_DEFINED__

typedef interface __FIAsyncOperationWithProgressCompletedHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double __FIAsyncOperationWithProgressCompletedHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double;

//  Declare the parameterized interface IID.
EXTERN_C const IID IID___FIAsyncOperationWithProgressCompletedHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double;

typedef struct __FIAsyncOperationWithProgressCompletedHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_doubleVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__FIAsyncOperationWithProgressCompletedHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__FIAsyncOperationWithProgressCompletedHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double* This);
    ULONG (STDMETHODCALLTYPE* Release)(__FIAsyncOperationWithProgressCompletedHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double* This);
    HRESULT (STDMETHODCALLTYPE* Invoke)(__FIAsyncOperationWithProgressCompletedHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double* This,
        __FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double* asyncInfo,
        AsyncStatus asyncStatus);

    END_INTERFACE
} __FIAsyncOperationWithProgressCompletedHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_doubleVtbl;

interface __FIAsyncOperationWithProgressCompletedHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double
{
    CONST_VTBL struct __FIAsyncOperationWithProgressCompletedHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_doubleVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __FIAsyncOperationWithProgressCompletedHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __FIAsyncOperationWithProgressCompletedHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __FIAsyncOperationWithProgressCompletedHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __FIAsyncOperationWithProgressCompletedHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_Invoke(This, asyncInfo, asyncStatus) \
    ((This)->lpVtbl->Invoke(This, asyncInfo, asyncStatus))

#endif /* COBJMACROS */

#endif // ____FIAsyncOperationWithProgressCompletedHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_INTERFACE_DEFINED__
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000
#if !defined(____FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_INTERFACE_DEFINED__)
#define ____FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_INTERFACE_DEFINED__

typedef interface __FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double __FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double;

//  Declare the parameterized interface IID.
EXTERN_C const IID IID___FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double;

typedef struct __FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_doubleVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double* This);
    ULONG (STDMETHODCALLTYPE* Release)(__FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double* This);
    HRESULT (STDMETHODCALLTYPE* GetIids)(__FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double* This,
        ULONG* iidCount,
        IID** iids);
    HRESULT (STDMETHODCALLTYPE* GetRuntimeClassName)(__FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double* This,
        HSTRING* className);
    HRESULT (STDMETHODCALLTYPE* GetTrustLevel)(__FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double* This,
        TrustLevel* trustLevel);
    HRESULT (STDMETHODCALLTYPE* put_Progress)(__FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double* This,
        __FIAsyncOperationProgressHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double* handler);
    HRESULT (STDMETHODCALLTYPE* get_Progress)(__FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double* This,
        __FIAsyncOperationProgressHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double** result);
    HRESULT (STDMETHODCALLTYPE* put_Completed)(__FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double* This,
        __FIAsyncOperationWithProgressCompletedHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double* handler);
    HRESULT (STDMETHODCALLTYPE* get_Completed)(__FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double* This,
        __FIAsyncOperationWithProgressCompletedHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double** result);
    HRESULT (STDMETHODCALLTYPE* GetResults)(__FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double* This,
        __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider** result);

    END_INTERFACE
} __FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_doubleVtbl;

interface __FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double
{
    CONST_VTBL struct __FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_doubleVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_GetIids(This, iidCount, iids) \
    ((This)->lpVtbl->GetIids(This, iidCount, iids))

#define __FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_GetRuntimeClassName(This, className) \
    ((This)->lpVtbl->GetRuntimeClassName(This, className))

#define __FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_GetTrustLevel(This, trustLevel) \
    ((This)->lpVtbl->GetTrustLevel(This, trustLevel))

#define __FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_put_Progress(This, handler) \
    ((This)->lpVtbl->put_Progress(This, handler))

#define __FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_get_Progress(This, result) \
    ((This)->lpVtbl->get_Progress(This, result))

#define __FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_put_Completed(This, handler) \
    ((This)->lpVtbl->put_Completed(This, handler))

#define __FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_get_Completed(This, result) \
    ((This)->lpVtbl->get_Completed(This, result))

#define __FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_GetResults(This, result) \
    ((This)->lpVtbl->GetResults(This, result))

#endif /* COBJMACROS */

#endif // ____FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_INTERFACE_DEFINED__
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000
#if !defined(____FIAsyncOperationProgressHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_INTERFACE_DEFINED__)
#define ____FIAsyncOperationProgressHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_INTERFACE_DEFINED__

typedef interface __FIAsyncOperationProgressHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double __FIAsyncOperationProgressHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double;

//  Declare the parameterized interface IID.
EXTERN_C const IID IID___FIAsyncOperationProgressHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double;

typedef struct __FIAsyncOperationProgressHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_doubleVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__FIAsyncOperationProgressHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__FIAsyncOperationProgressHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double* This);
    ULONG (STDMETHODCALLTYPE* Release)(__FIAsyncOperationProgressHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double* This);
    HRESULT (STDMETHODCALLTYPE* Invoke)(__FIAsyncOperationProgressHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double* This,
        __FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double* asyncInfo,
        DOUBLE progressInfo);

    END_INTERFACE
} __FIAsyncOperationProgressHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_doubleVtbl;

interface __FIAsyncOperationProgressHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double
{
    CONST_VTBL struct __FIAsyncOperationProgressHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_doubleVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __FIAsyncOperationProgressHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __FIAsyncOperationProgressHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __FIAsyncOperationProgressHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __FIAsyncOperationProgressHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_Invoke(This, asyncInfo, progressInfo) \
    ((This)->lpVtbl->Invoke(This, asyncInfo, progressInfo))

#endif /* COBJMACROS */

#endif // ____FIAsyncOperationProgressHandler_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double_INTERFACE_DEFINED__
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
#if !defined(____FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_INTERFACE_DEFINED__)
#define ____FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_INTERFACE_DEFINED__

typedef interface __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource;

//  Declare the parameterized interface IID.
EXTERN_C const IID IID___FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource;

typedef struct __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSourceVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This);
    ULONG (STDMETHODCALLTYPE* Release)(__FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This);
    HRESULT (STDMETHODCALLTYPE* GetIids)(__FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        ULONG* iidCount,
        IID** iids);
    HRESULT (STDMETHODCALLTYPE* GetRuntimeClassName)(__FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        HSTRING* className);
    HRESULT (STDMETHODCALLTYPE* GetTrustLevel)(__FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        TrustLevel* trustLevel);
    HRESULT (STDMETHODCALLTYPE* get_Current)(__FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource** result);
    HRESULT (STDMETHODCALLTYPE* get_HasCurrent)(__FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        boolean* result);
    HRESULT (STDMETHODCALLTYPE* MoveNext)(__FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        boolean* result);
    HRESULT (STDMETHODCALLTYPE* GetMany)(__FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        UINT32 itemsLength,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource** items,
        UINT32* result);

    END_INTERFACE
} __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSourceVtbl;

interface __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource
{
    CONST_VTBL struct __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSourceVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_GetIids(This, iidCount, iids) \
    ((This)->lpVtbl->GetIids(This, iidCount, iids))

#define __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_GetRuntimeClassName(This, className) \
    ((This)->lpVtbl->GetRuntimeClassName(This, className))

#define __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_GetTrustLevel(This, trustLevel) \
    ((This)->lpVtbl->GetTrustLevel(This, trustLevel))

#define __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_get_Current(This, result) \
    ((This)->lpVtbl->get_Current(This, result))

#define __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_get_HasCurrent(This, result) \
    ((This)->lpVtbl->get_HasCurrent(This, result))

#define __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_MoveNext(This, result) \
    ((This)->lpVtbl->MoveNext(This, result))

#define __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_GetMany(This, itemsLength, items, result) \
    ((This)->lpVtbl->GetMany(This, itemsLength, items, result))

#endif /* COBJMACROS */

#endif // ____FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_INTERFACE_DEFINED__
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
#if !defined(____FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_INTERFACE_DEFINED__)
#define ____FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_INTERFACE_DEFINED__

typedef interface __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource;

//  Declare the parameterized interface IID.
EXTERN_C const IID IID___FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource;

typedef struct __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSourceVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This);
    ULONG (STDMETHODCALLTYPE* Release)(__FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This);
    HRESULT (STDMETHODCALLTYPE* GetIids)(__FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        ULONG* iidCount,
        IID** iids);
    HRESULT (STDMETHODCALLTYPE* GetRuntimeClassName)(__FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        HSTRING* className);
    HRESULT (STDMETHODCALLTYPE* GetTrustLevel)(__FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        TrustLevel* trustLevel);
    HRESULT (STDMETHODCALLTYPE* First)(__FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        __FIIterator_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource** result);

    END_INTERFACE
} __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSourceVtbl;

interface __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource
{
    CONST_VTBL struct __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSourceVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_GetIids(This, iidCount, iids) \
    ((This)->lpVtbl->GetIids(This, iidCount, iids))

#define __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_GetRuntimeClassName(This, className) \
    ((This)->lpVtbl->GetRuntimeClassName(This, className))

#define __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_GetTrustLevel(This, trustLevel) \
    ((This)->lpVtbl->GetTrustLevel(This, trustLevel))

#define __FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_First(This, result) \
    ((This)->lpVtbl->First(This, result))

#endif /* COBJMACROS */

#endif // ____FIIterable_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_INTERFACE_DEFINED__
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#if !defined(____FIIterator_1_HSTRING_INTERFACE_DEFINED__)
#define ____FIIterator_1_HSTRING_INTERFACE_DEFINED__

typedef interface __FIIterator_1_HSTRING __FIIterator_1_HSTRING;

//  Declare the parameterized interface IID.
EXTERN_C const IID IID___FIIterator_1_HSTRING;

typedef struct __FIIterator_1_HSTRINGVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__FIIterator_1_HSTRING* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__FIIterator_1_HSTRING* This);
    ULONG (STDMETHODCALLTYPE* Release)(__FIIterator_1_HSTRING* This);
    HRESULT (STDMETHODCALLTYPE* GetIids)(__FIIterator_1_HSTRING* This,
        ULONG* iidCount,
        IID** iids);
    HRESULT (STDMETHODCALLTYPE* GetRuntimeClassName)(__FIIterator_1_HSTRING* This,
        HSTRING* className);
    HRESULT (STDMETHODCALLTYPE* GetTrustLevel)(__FIIterator_1_HSTRING* This,
        TrustLevel* trustLevel);
    HRESULT (STDMETHODCALLTYPE* get_Current)(__FIIterator_1_HSTRING* This,
        HSTRING* result);
    HRESULT (STDMETHODCALLTYPE* get_HasCurrent)(__FIIterator_1_HSTRING* This,
        boolean* result);
    HRESULT (STDMETHODCALLTYPE* MoveNext)(__FIIterator_1_HSTRING* This,
        boolean* result);
    HRESULT (STDMETHODCALLTYPE* GetMany)(__FIIterator_1_HSTRING* This,
        UINT32 itemsLength,
        HSTRING* items,
        UINT32* result);

    END_INTERFACE
} __FIIterator_1_HSTRINGVtbl;

interface __FIIterator_1_HSTRING
{
    CONST_VTBL struct __FIIterator_1_HSTRINGVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __FIIterator_1_HSTRING_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __FIIterator_1_HSTRING_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __FIIterator_1_HSTRING_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __FIIterator_1_HSTRING_GetIids(This, iidCount, iids) \
    ((This)->lpVtbl->GetIids(This, iidCount, iids))

#define __FIIterator_1_HSTRING_GetRuntimeClassName(This, className) \
    ((This)->lpVtbl->GetRuntimeClassName(This, className))

#define __FIIterator_1_HSTRING_GetTrustLevel(This, trustLevel) \
    ((This)->lpVtbl->GetTrustLevel(This, trustLevel))

#define __FIIterator_1_HSTRING_get_Current(This, result) \
    ((This)->lpVtbl->get_Current(This, result))

#define __FIIterator_1_HSTRING_get_HasCurrent(This, result) \
    ((This)->lpVtbl->get_HasCurrent(This, result))

#define __FIIterator_1_HSTRING_MoveNext(This, result) \
    ((This)->lpVtbl->MoveNext(This, result))

#define __FIIterator_1_HSTRING_GetMany(This, itemsLength, items, result) \
    ((This)->lpVtbl->GetMany(This, itemsLength, items, result))

#endif /* COBJMACROS */

#endif // ____FIIterator_1_HSTRING_INTERFACE_DEFINED__

#if !defined(____FIIterable_1_HSTRING_INTERFACE_DEFINED__)
#define ____FIIterable_1_HSTRING_INTERFACE_DEFINED__

typedef interface __FIIterable_1_HSTRING __FIIterable_1_HSTRING;

//  Declare the parameterized interface IID.
EXTERN_C const IID IID___FIIterable_1_HSTRING;

typedef struct __FIIterable_1_HSTRINGVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__FIIterable_1_HSTRING* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__FIIterable_1_HSTRING* This);
    ULONG (STDMETHODCALLTYPE* Release)(__FIIterable_1_HSTRING* This);
    HRESULT (STDMETHODCALLTYPE* GetIids)(__FIIterable_1_HSTRING* This,
        ULONG* iidCount,
        IID** iids);
    HRESULT (STDMETHODCALLTYPE* GetRuntimeClassName)(__FIIterable_1_HSTRING* This,
        HSTRING* className);
    HRESULT (STDMETHODCALLTYPE* GetTrustLevel)(__FIIterable_1_HSTRING* This,
        TrustLevel* trustLevel);
    HRESULT (STDMETHODCALLTYPE* First)(__FIIterable_1_HSTRING* This,
        __FIIterator_1_HSTRING** result);

    END_INTERFACE
} __FIIterable_1_HSTRINGVtbl;

interface __FIIterable_1_HSTRING
{
    CONST_VTBL struct __FIIterable_1_HSTRINGVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __FIIterable_1_HSTRING_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __FIIterable_1_HSTRING_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __FIIterable_1_HSTRING_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __FIIterable_1_HSTRING_GetIids(This, iidCount, iids) \
    ((This)->lpVtbl->GetIids(This, iidCount, iids))

#define __FIIterable_1_HSTRING_GetRuntimeClassName(This, className) \
    ((This)->lpVtbl->GetRuntimeClassName(This, className))

#define __FIIterable_1_HSTRING_GetTrustLevel(This, trustLevel) \
    ((This)->lpVtbl->GetTrustLevel(This, trustLevel))

#define __FIIterable_1_HSTRING_First(This, result) \
    ((This)->lpVtbl->First(This, result))

#endif /* COBJMACROS */

#endif // ____FIIterable_1_HSTRING_INTERFACE_DEFINED__

#if !defined(____FIKeyValuePair_2_HSTRING_HSTRING_INTERFACE_DEFINED__)
#define ____FIKeyValuePair_2_HSTRING_HSTRING_INTERFACE_DEFINED__

typedef interface __FIKeyValuePair_2_HSTRING_HSTRING __FIKeyValuePair_2_HSTRING_HSTRING;

//  Declare the parameterized interface IID.
EXTERN_C const IID IID___FIKeyValuePair_2_HSTRING_HSTRING;

typedef struct __FIKeyValuePair_2_HSTRING_HSTRINGVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__FIKeyValuePair_2_HSTRING_HSTRING* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__FIKeyValuePair_2_HSTRING_HSTRING* This);
    ULONG (STDMETHODCALLTYPE* Release)(__FIKeyValuePair_2_HSTRING_HSTRING* This);
    HRESULT (STDMETHODCALLTYPE* GetIids)(__FIKeyValuePair_2_HSTRING_HSTRING* This,
        ULONG* iidCount,
        IID** iids);
    HRESULT (STDMETHODCALLTYPE* GetRuntimeClassName)(__FIKeyValuePair_2_HSTRING_HSTRING* This,
        HSTRING* className);
    HRESULT (STDMETHODCALLTYPE* GetTrustLevel)(__FIKeyValuePair_2_HSTRING_HSTRING* This,
        TrustLevel* trustLevel);
    HRESULT (STDMETHODCALLTYPE* get_Key)(__FIKeyValuePair_2_HSTRING_HSTRING* This,
        HSTRING* result);
    HRESULT (STDMETHODCALLTYPE* get_Value)(__FIKeyValuePair_2_HSTRING_HSTRING* This,
        HSTRING* result);

    END_INTERFACE
} __FIKeyValuePair_2_HSTRING_HSTRINGVtbl;

interface __FIKeyValuePair_2_HSTRING_HSTRING
{
    CONST_VTBL struct __FIKeyValuePair_2_HSTRING_HSTRINGVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __FIKeyValuePair_2_HSTRING_HSTRING_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __FIKeyValuePair_2_HSTRING_HSTRING_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __FIKeyValuePair_2_HSTRING_HSTRING_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __FIKeyValuePair_2_HSTRING_HSTRING_GetIids(This, iidCount, iids) \
    ((This)->lpVtbl->GetIids(This, iidCount, iids))

#define __FIKeyValuePair_2_HSTRING_HSTRING_GetRuntimeClassName(This, className) \
    ((This)->lpVtbl->GetRuntimeClassName(This, className))

#define __FIKeyValuePair_2_HSTRING_HSTRING_GetTrustLevel(This, trustLevel) \
    ((This)->lpVtbl->GetTrustLevel(This, trustLevel))

#define __FIKeyValuePair_2_HSTRING_HSTRING_get_Key(This, result) \
    ((This)->lpVtbl->get_Key(This, result))

#define __FIKeyValuePair_2_HSTRING_HSTRING_get_Value(This, result) \
    ((This)->lpVtbl->get_Value(This, result))

#endif /* COBJMACROS */

#endif // ____FIKeyValuePair_2_HSTRING_HSTRING_INTERFACE_DEFINED__

#if !defined(____FIIterator_1___FIKeyValuePair_2_HSTRING_HSTRING_INTERFACE_DEFINED__)
#define ____FIIterator_1___FIKeyValuePair_2_HSTRING_HSTRING_INTERFACE_DEFINED__

typedef interface __FIIterator_1___FIKeyValuePair_2_HSTRING_HSTRING __FIIterator_1___FIKeyValuePair_2_HSTRING_HSTRING;

//  Declare the parameterized interface IID.
EXTERN_C const IID IID___FIIterator_1___FIKeyValuePair_2_HSTRING_HSTRING;

typedef struct __FIIterator_1___FIKeyValuePair_2_HSTRING_HSTRINGVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__FIIterator_1___FIKeyValuePair_2_HSTRING_HSTRING* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__FIIterator_1___FIKeyValuePair_2_HSTRING_HSTRING* This);
    ULONG (STDMETHODCALLTYPE* Release)(__FIIterator_1___FIKeyValuePair_2_HSTRING_HSTRING* This);
    HRESULT (STDMETHODCALLTYPE* GetIids)(__FIIterator_1___FIKeyValuePair_2_HSTRING_HSTRING* This,
        ULONG* iidCount,
        IID** iids);
    HRESULT (STDMETHODCALLTYPE* GetRuntimeClassName)(__FIIterator_1___FIKeyValuePair_2_HSTRING_HSTRING* This,
        HSTRING* className);
    HRESULT (STDMETHODCALLTYPE* GetTrustLevel)(__FIIterator_1___FIKeyValuePair_2_HSTRING_HSTRING* This,
        TrustLevel* trustLevel);
    HRESULT (STDMETHODCALLTYPE* get_Current)(__FIIterator_1___FIKeyValuePair_2_HSTRING_HSTRING* This,
        __FIKeyValuePair_2_HSTRING_HSTRING** result);
    HRESULT (STDMETHODCALLTYPE* get_HasCurrent)(__FIIterator_1___FIKeyValuePair_2_HSTRING_HSTRING* This,
        boolean* result);
    HRESULT (STDMETHODCALLTYPE* MoveNext)(__FIIterator_1___FIKeyValuePair_2_HSTRING_HSTRING* This,
        boolean* result);
    HRESULT (STDMETHODCALLTYPE* GetMany)(__FIIterator_1___FIKeyValuePair_2_HSTRING_HSTRING* This,
        UINT32 itemsLength,
        __FIKeyValuePair_2_HSTRING_HSTRING** items,
        UINT32* result);

    END_INTERFACE
} __FIIterator_1___FIKeyValuePair_2_HSTRING_HSTRINGVtbl;

interface __FIIterator_1___FIKeyValuePair_2_HSTRING_HSTRING
{
    CONST_VTBL struct __FIIterator_1___FIKeyValuePair_2_HSTRING_HSTRINGVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __FIIterator_1___FIKeyValuePair_2_HSTRING_HSTRING_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __FIIterator_1___FIKeyValuePair_2_HSTRING_HSTRING_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __FIIterator_1___FIKeyValuePair_2_HSTRING_HSTRING_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __FIIterator_1___FIKeyValuePair_2_HSTRING_HSTRING_GetIids(This, iidCount, iids) \
    ((This)->lpVtbl->GetIids(This, iidCount, iids))

#define __FIIterator_1___FIKeyValuePair_2_HSTRING_HSTRING_GetRuntimeClassName(This, className) \
    ((This)->lpVtbl->GetRuntimeClassName(This, className))

#define __FIIterator_1___FIKeyValuePair_2_HSTRING_HSTRING_GetTrustLevel(This, trustLevel) \
    ((This)->lpVtbl->GetTrustLevel(This, trustLevel))

#define __FIIterator_1___FIKeyValuePair_2_HSTRING_HSTRING_get_Current(This, result) \
    ((This)->lpVtbl->get_Current(This, result))

#define __FIIterator_1___FIKeyValuePair_2_HSTRING_HSTRING_get_HasCurrent(This, result) \
    ((This)->lpVtbl->get_HasCurrent(This, result))

#define __FIIterator_1___FIKeyValuePair_2_HSTRING_HSTRING_MoveNext(This, result) \
    ((This)->lpVtbl->MoveNext(This, result))

#define __FIIterator_1___FIKeyValuePair_2_HSTRING_HSTRING_GetMany(This, itemsLength, items, result) \
    ((This)->lpVtbl->GetMany(This, itemsLength, items, result))

#endif /* COBJMACROS */

#endif // ____FIIterator_1___FIKeyValuePair_2_HSTRING_HSTRING_INTERFACE_DEFINED__

#if !defined(____FIIterable_1___FIKeyValuePair_2_HSTRING_HSTRING_INTERFACE_DEFINED__)
#define ____FIIterable_1___FIKeyValuePair_2_HSTRING_HSTRING_INTERFACE_DEFINED__

typedef interface __FIIterable_1___FIKeyValuePair_2_HSTRING_HSTRING __FIIterable_1___FIKeyValuePair_2_HSTRING_HSTRING;

//  Declare the parameterized interface IID.
EXTERN_C const IID IID___FIIterable_1___FIKeyValuePair_2_HSTRING_HSTRING;

typedef struct __FIIterable_1___FIKeyValuePair_2_HSTRING_HSTRINGVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__FIIterable_1___FIKeyValuePair_2_HSTRING_HSTRING* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__FIIterable_1___FIKeyValuePair_2_HSTRING_HSTRING* This);
    ULONG (STDMETHODCALLTYPE* Release)(__FIIterable_1___FIKeyValuePair_2_HSTRING_HSTRING* This);
    HRESULT (STDMETHODCALLTYPE* GetIids)(__FIIterable_1___FIKeyValuePair_2_HSTRING_HSTRING* This,
        ULONG* iidCount,
        IID** iids);
    HRESULT (STDMETHODCALLTYPE* GetRuntimeClassName)(__FIIterable_1___FIKeyValuePair_2_HSTRING_HSTRING* This,
        HSTRING* className);
    HRESULT (STDMETHODCALLTYPE* GetTrustLevel)(__FIIterable_1___FIKeyValuePair_2_HSTRING_HSTRING* This,
        TrustLevel* trustLevel);
    HRESULT (STDMETHODCALLTYPE* First)(__FIIterable_1___FIKeyValuePair_2_HSTRING_HSTRING* This,
        __FIIterator_1___FIKeyValuePair_2_HSTRING_HSTRING** result);

    END_INTERFACE
} __FIIterable_1___FIKeyValuePair_2_HSTRING_HSTRINGVtbl;

interface __FIIterable_1___FIKeyValuePair_2_HSTRING_HSTRING
{
    CONST_VTBL struct __FIIterable_1___FIKeyValuePair_2_HSTRING_HSTRINGVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __FIIterable_1___FIKeyValuePair_2_HSTRING_HSTRING_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __FIIterable_1___FIKeyValuePair_2_HSTRING_HSTRING_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __FIIterable_1___FIKeyValuePair_2_HSTRING_HSTRING_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __FIIterable_1___FIKeyValuePair_2_HSTRING_HSTRING_GetIids(This, iidCount, iids) \
    ((This)->lpVtbl->GetIids(This, iidCount, iids))

#define __FIIterable_1___FIKeyValuePair_2_HSTRING_HSTRING_GetRuntimeClassName(This, className) \
    ((This)->lpVtbl->GetRuntimeClassName(This, className))

#define __FIIterable_1___FIKeyValuePair_2_HSTRING_HSTRING_GetTrustLevel(This, trustLevel) \
    ((This)->lpVtbl->GetTrustLevel(This, trustLevel))

#define __FIIterable_1___FIKeyValuePair_2_HSTRING_HSTRING_First(This, result) \
    ((This)->lpVtbl->First(This, result))

#endif /* COBJMACROS */

#endif // ____FIIterable_1___FIKeyValuePair_2_HSTRING_HSTRING_INTERFACE_DEFINED__

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
#if !defined(____FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_INTERFACE_DEFINED__)
#define ____FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_INTERFACE_DEFINED__

typedef interface __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource;

//  Declare the parameterized interface IID.
EXTERN_C const IID IID___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource;

typedef struct __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSourceVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This);
    ULONG (STDMETHODCALLTYPE* Release)(__FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This);
    HRESULT (STDMETHODCALLTYPE* GetIids)(__FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        ULONG* iidCount,
        IID** iids);
    HRESULT (STDMETHODCALLTYPE* GetRuntimeClassName)(__FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        HSTRING* className);
    HRESULT (STDMETHODCALLTYPE* GetTrustLevel)(__FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        TrustLevel* trustLevel);
    HRESULT (STDMETHODCALLTYPE* GetAt)(__FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        UINT32 index,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource** result);
    HRESULT (STDMETHODCALLTYPE* get_Size)(__FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        UINT32* result);
    HRESULT (STDMETHODCALLTYPE* IndexOf)(__FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource* value,
        UINT32* index,
        boolean* result);
    HRESULT (STDMETHODCALLTYPE* GetMany)(__FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        UINT32 startIndex,
        UINT32 itemsLength,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource** items,
        UINT32* result);

    END_INTERFACE
} __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSourceVtbl;

interface __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource
{
    CONST_VTBL struct __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSourceVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_GetIids(This, iidCount, iids) \
    ((This)->lpVtbl->GetIids(This, iidCount, iids))

#define __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_GetRuntimeClassName(This, className) \
    ((This)->lpVtbl->GetRuntimeClassName(This, className))

#define __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_GetTrustLevel(This, trustLevel) \
    ((This)->lpVtbl->GetTrustLevel(This, trustLevel))

#define __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_GetAt(This, index, result) \
    ((This)->lpVtbl->GetAt(This, index, result))

#define __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_get_Size(This, result) \
    ((This)->lpVtbl->get_Size(This, result))

#define __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_IndexOf(This, value, index, result) \
    ((This)->lpVtbl->IndexOf(This, value, index, result))

#define __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_GetMany(This, startIndex, itemsLength, items, result) \
    ((This)->lpVtbl->GetMany(This, startIndex, itemsLength, items, result))

#endif /* COBJMACROS */

#endif // ____FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_INTERFACE_DEFINED__
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#if !defined(____FIVectorView_1_HSTRING_INTERFACE_DEFINED__)
#define ____FIVectorView_1_HSTRING_INTERFACE_DEFINED__

typedef interface __FIVectorView_1_HSTRING __FIVectorView_1_HSTRING;

//  Declare the parameterized interface IID.
EXTERN_C const IID IID___FIVectorView_1_HSTRING;

typedef struct __FIVectorView_1_HSTRINGVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__FIVectorView_1_HSTRING* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__FIVectorView_1_HSTRING* This);
    ULONG (STDMETHODCALLTYPE* Release)(__FIVectorView_1_HSTRING* This);
    HRESULT (STDMETHODCALLTYPE* GetIids)(__FIVectorView_1_HSTRING* This,
        ULONG* iidCount,
        IID** iids);
    HRESULT (STDMETHODCALLTYPE* GetRuntimeClassName)(__FIVectorView_1_HSTRING* This,
        HSTRING* className);
    HRESULT (STDMETHODCALLTYPE* GetTrustLevel)(__FIVectorView_1_HSTRING* This,
        TrustLevel* trustLevel);
    HRESULT (STDMETHODCALLTYPE* GetAt)(__FIVectorView_1_HSTRING* This,
        UINT32 index,
        HSTRING* result);
    HRESULT (STDMETHODCALLTYPE* get_Size)(__FIVectorView_1_HSTRING* This,
        UINT32* result);
    HRESULT (STDMETHODCALLTYPE* IndexOf)(__FIVectorView_1_HSTRING* This,
        HSTRING value,
        UINT32* index,
        boolean* result);
    HRESULT (STDMETHODCALLTYPE* GetMany)(__FIVectorView_1_HSTRING* This,
        UINT32 startIndex,
        UINT32 itemsLength,
        HSTRING* items,
        UINT32* result);

    END_INTERFACE
} __FIVectorView_1_HSTRINGVtbl;

interface __FIVectorView_1_HSTRING
{
    CONST_VTBL struct __FIVectorView_1_HSTRINGVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __FIVectorView_1_HSTRING_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __FIVectorView_1_HSTRING_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __FIVectorView_1_HSTRING_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __FIVectorView_1_HSTRING_GetIids(This, iidCount, iids) \
    ((This)->lpVtbl->GetIids(This, iidCount, iids))

#define __FIVectorView_1_HSTRING_GetRuntimeClassName(This, className) \
    ((This)->lpVtbl->GetRuntimeClassName(This, className))

#define __FIVectorView_1_HSTRING_GetTrustLevel(This, trustLevel) \
    ((This)->lpVtbl->GetTrustLevel(This, trustLevel))

#define __FIVectorView_1_HSTRING_GetAt(This, index, result) \
    ((This)->lpVtbl->GetAt(This, index, result))

#define __FIVectorView_1_HSTRING_get_Size(This, result) \
    ((This)->lpVtbl->get_Size(This, result))

#define __FIVectorView_1_HSTRING_IndexOf(This, value, index, result) \
    ((This)->lpVtbl->IndexOf(This, value, index, result))

#define __FIVectorView_1_HSTRING_GetMany(This, startIndex, itemsLength, items, result) \
    ((This)->lpVtbl->GetMany(This, startIndex, itemsLength, items, result))

#endif /* COBJMACROS */

#endif // ____FIVectorView_1_HSTRING_INTERFACE_DEFINED__

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
#if !defined(____FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_INTERFACE_DEFINED__)
#define ____FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_INTERFACE_DEFINED__

typedef interface __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo;

//  Declare the parameterized interface IID.
EXTERN_C const IID IID___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo;

typedef struct __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfoVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This);
    ULONG (STDMETHODCALLTYPE* Release)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This);
    HRESULT (STDMETHODCALLTYPE* GetIids)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        ULONG* iidCount,
        IID** iids);
    HRESULT (STDMETHODCALLTYPE* GetRuntimeClassName)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        HSTRING* className);
    HRESULT (STDMETHODCALLTYPE* GetTrustLevel)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        TrustLevel* trustLevel);
    HRESULT (STDMETHODCALLTYPE* GetAt)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        UINT32 index,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo** result);
    HRESULT (STDMETHODCALLTYPE* get_Size)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        UINT32* result);
    HRESULT (STDMETHODCALLTYPE* GetView)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo** result);
    HRESULT (STDMETHODCALLTYPE* IndexOf)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo* value,
        UINT32* index,
        boolean* result);
    HRESULT (STDMETHODCALLTYPE* SetAt)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        UINT32 index,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo* value);
    HRESULT (STDMETHODCALLTYPE* InsertAt)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        UINT32 index,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo* value);
    HRESULT (STDMETHODCALLTYPE* RemoveAt)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        UINT32 index);
    HRESULT (STDMETHODCALLTYPE* Append)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo* value);
    HRESULT (STDMETHODCALLTYPE* RemoveAtEnd)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This);
    HRESULT (STDMETHODCALLTYPE* Clear)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This);
    HRESULT (STDMETHODCALLTYPE* GetMany)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        UINT32 startIndex,
        UINT32 itemsLength,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo** items,
        UINT32* result);
    HRESULT (STDMETHODCALLTYPE* ReplaceAll)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo* This,
        UINT32 itemsLength,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo** items);

    END_INTERFACE
} __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfoVtbl;

interface __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo
{
    CONST_VTBL struct __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfoVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_GetIids(This, iidCount, iids) \
    ((This)->lpVtbl->GetIids(This, iidCount, iids))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_GetRuntimeClassName(This, className) \
    ((This)->lpVtbl->GetRuntimeClassName(This, className))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_GetTrustLevel(This, trustLevel) \
    ((This)->lpVtbl->GetTrustLevel(This, trustLevel))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_GetAt(This, index, result) \
    ((This)->lpVtbl->GetAt(This, index, result))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_get_Size(This, result) \
    ((This)->lpVtbl->get_Size(This, result))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_GetView(This, result) \
    ((This)->lpVtbl->GetView(This, result))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_IndexOf(This, value, index, result) \
    ((This)->lpVtbl->IndexOf(This, value, index, result))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_SetAt(This, index, value) \
    ((This)->lpVtbl->SetAt(This, index, value))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_InsertAt(This, index, value) \
    ((This)->lpVtbl->InsertAt(This, index, value))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_RemoveAt(This, index) \
    ((This)->lpVtbl->RemoveAt(This, index))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_Append(This, value) \
    ((This)->lpVtbl->Append(This, value))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_RemoveAtEnd(This) \
    ((This)->lpVtbl->RemoveAtEnd(This))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_Clear(This) \
    ((This)->lpVtbl->Clear(This))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_GetMany(This, startIndex, itemsLength, items, result) \
    ((This)->lpVtbl->GetMany(This, startIndex, itemsLength, items, result))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_ReplaceAll(This, itemsLength, items) \
    ((This)->lpVtbl->ReplaceAll(This, itemsLength, items))

#endif /* COBJMACROS */

#endif // ____FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo_INTERFACE_DEFINED__
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
#if !defined(____FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_INTERFACE_DEFINED__)
#define ____FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_INTERFACE_DEFINED__

typedef interface __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource;

//  Declare the parameterized interface IID.
EXTERN_C const IID IID___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource;

typedef struct __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSourceVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This);
    ULONG (STDMETHODCALLTYPE* Release)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This);
    HRESULT (STDMETHODCALLTYPE* GetIids)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        ULONG* iidCount,
        IID** iids);
    HRESULT (STDMETHODCALLTYPE* GetRuntimeClassName)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        HSTRING* className);
    HRESULT (STDMETHODCALLTYPE* GetTrustLevel)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        TrustLevel* trustLevel);
    HRESULT (STDMETHODCALLTYPE* GetAt)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        UINT32 index,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource** result);
    HRESULT (STDMETHODCALLTYPE* get_Size)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        UINT32* result);
    HRESULT (STDMETHODCALLTYPE* GetView)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        __FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource** result);
    HRESULT (STDMETHODCALLTYPE* IndexOf)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource* value,
        UINT32* index,
        boolean* result);
    HRESULT (STDMETHODCALLTYPE* SetAt)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        UINT32 index,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource* value);
    HRESULT (STDMETHODCALLTYPE* InsertAt)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        UINT32 index,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource* value);
    HRESULT (STDMETHODCALLTYPE* RemoveAt)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        UINT32 index);
    HRESULT (STDMETHODCALLTYPE* Append)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource* value);
    HRESULT (STDMETHODCALLTYPE* RemoveAtEnd)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This);
    HRESULT (STDMETHODCALLTYPE* Clear)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This);
    HRESULT (STDMETHODCALLTYPE* GetMany)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        UINT32 startIndex,
        UINT32 itemsLength,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource** items,
        UINT32* result);
    HRESULT (STDMETHODCALLTYPE* ReplaceAll)(__FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource* This,
        UINT32 itemsLength,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource** items);

    END_INTERFACE
} __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSourceVtbl;

interface __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource
{
    CONST_VTBL struct __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSourceVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_GetIids(This, iidCount, iids) \
    ((This)->lpVtbl->GetIids(This, iidCount, iids))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_GetRuntimeClassName(This, className) \
    ((This)->lpVtbl->GetRuntimeClassName(This, className))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_GetTrustLevel(This, trustLevel) \
    ((This)->lpVtbl->GetTrustLevel(This, trustLevel))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_GetAt(This, index, result) \
    ((This)->lpVtbl->GetAt(This, index, result))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_get_Size(This, result) \
    ((This)->lpVtbl->get_Size(This, result))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_GetView(This, result) \
    ((This)->lpVtbl->GetView(This, result))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_IndexOf(This, value, index, result) \
    ((This)->lpVtbl->IndexOf(This, value, index, result))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_SetAt(This, index, value) \
    ((This)->lpVtbl->SetAt(This, index, value))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_InsertAt(This, index, value) \
    ((This)->lpVtbl->InsertAt(This, index, value))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_RemoveAt(This, index) \
    ((This)->lpVtbl->RemoveAt(This, index))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_Append(This, value) \
    ((This)->lpVtbl->Append(This, value))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_RemoveAtEnd(This) \
    ((This)->lpVtbl->RemoveAtEnd(This))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_Clear(This) \
    ((This)->lpVtbl->Clear(This))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_GetMany(This, startIndex, itemsLength, items, result) \
    ((This)->lpVtbl->GetMany(This, startIndex, itemsLength, items, result))

#define __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_ReplaceAll(This, itemsLength, items) \
    ((This)->lpVtbl->ReplaceAll(This, itemsLength, items))

#endif /* COBJMACROS */

#endif // ____FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource_INTERFACE_DEFINED__
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#if !defined(____FIVector_1_HSTRING_INTERFACE_DEFINED__)
#define ____FIVector_1_HSTRING_INTERFACE_DEFINED__

typedef interface __FIVector_1_HSTRING __FIVector_1_HSTRING;

//  Declare the parameterized interface IID.
EXTERN_C const IID IID___FIVector_1_HSTRING;

typedef struct __FIVector_1_HSTRINGVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__FIVector_1_HSTRING* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__FIVector_1_HSTRING* This);
    ULONG (STDMETHODCALLTYPE* Release)(__FIVector_1_HSTRING* This);
    HRESULT (STDMETHODCALLTYPE* GetIids)(__FIVector_1_HSTRING* This,
        ULONG* iidCount,
        IID** iids);
    HRESULT (STDMETHODCALLTYPE* GetRuntimeClassName)(__FIVector_1_HSTRING* This,
        HSTRING* className);
    HRESULT (STDMETHODCALLTYPE* GetTrustLevel)(__FIVector_1_HSTRING* This,
        TrustLevel* trustLevel);
    HRESULT (STDMETHODCALLTYPE* GetAt)(__FIVector_1_HSTRING* This,
        UINT32 index,
        HSTRING* result);
    HRESULT (STDMETHODCALLTYPE* get_Size)(__FIVector_1_HSTRING* This,
        UINT32* result);
    HRESULT (STDMETHODCALLTYPE* GetView)(__FIVector_1_HSTRING* This,
        __FIVectorView_1_HSTRING** result);
    HRESULT (STDMETHODCALLTYPE* IndexOf)(__FIVector_1_HSTRING* This,
        HSTRING value,
        UINT32* index,
        boolean* result);
    HRESULT (STDMETHODCALLTYPE* SetAt)(__FIVector_1_HSTRING* This,
        UINT32 index,
        HSTRING value);
    HRESULT (STDMETHODCALLTYPE* InsertAt)(__FIVector_1_HSTRING* This,
        UINT32 index,
        HSTRING value);
    HRESULT (STDMETHODCALLTYPE* RemoveAt)(__FIVector_1_HSTRING* This,
        UINT32 index);
    HRESULT (STDMETHODCALLTYPE* Append)(__FIVector_1_HSTRING* This,
        HSTRING value);
    HRESULT (STDMETHODCALLTYPE* RemoveAtEnd)(__FIVector_1_HSTRING* This);
    HRESULT (STDMETHODCALLTYPE* Clear)(__FIVector_1_HSTRING* This);
    HRESULT (STDMETHODCALLTYPE* GetMany)(__FIVector_1_HSTRING* This,
        UINT32 startIndex,
        UINT32 itemsLength,
        HSTRING* items,
        UINT32* result);
    HRESULT (STDMETHODCALLTYPE* ReplaceAll)(__FIVector_1_HSTRING* This,
        UINT32 itemsLength,
        HSTRING* items);

    END_INTERFACE
} __FIVector_1_HSTRINGVtbl;

interface __FIVector_1_HSTRING
{
    CONST_VTBL struct __FIVector_1_HSTRINGVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __FIVector_1_HSTRING_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __FIVector_1_HSTRING_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __FIVector_1_HSTRING_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __FIVector_1_HSTRING_GetIids(This, iidCount, iids) \
    ((This)->lpVtbl->GetIids(This, iidCount, iids))

#define __FIVector_1_HSTRING_GetRuntimeClassName(This, className) \
    ((This)->lpVtbl->GetRuntimeClassName(This, className))

#define __FIVector_1_HSTRING_GetTrustLevel(This, trustLevel) \
    ((This)->lpVtbl->GetTrustLevel(This, trustLevel))

#define __FIVector_1_HSTRING_GetAt(This, index, result) \
    ((This)->lpVtbl->GetAt(This, index, result))

#define __FIVector_1_HSTRING_get_Size(This, result) \
    ((This)->lpVtbl->get_Size(This, result))

#define __FIVector_1_HSTRING_GetView(This, result) \
    ((This)->lpVtbl->GetView(This, result))

#define __FIVector_1_HSTRING_IndexOf(This, value, index, result) \
    ((This)->lpVtbl->IndexOf(This, value, index, result))

#define __FIVector_1_HSTRING_SetAt(This, index, value) \
    ((This)->lpVtbl->SetAt(This, index, value))

#define __FIVector_1_HSTRING_InsertAt(This, index, value) \
    ((This)->lpVtbl->InsertAt(This, index, value))

#define __FIVector_1_HSTRING_RemoveAt(This, index) \
    ((This)->lpVtbl->RemoveAt(This, index))

#define __FIVector_1_HSTRING_Append(This, value) \
    ((This)->lpVtbl->Append(This, value))

#define __FIVector_1_HSTRING_RemoveAtEnd(This) \
    ((This)->lpVtbl->RemoveAtEnd(This))

#define __FIVector_1_HSTRING_Clear(This) \
    ((This)->lpVtbl->Clear(This))

#define __FIVector_1_HSTRING_GetMany(This, startIndex, itemsLength, items, result) \
    ((This)->lpVtbl->GetMany(This, startIndex, itemsLength, items, result))

#define __FIVector_1_HSTRING_ReplaceAll(This, itemsLength, items) \
    ((This)->lpVtbl->ReplaceAll(This, itemsLength, items))

#endif /* COBJMACROS */

#endif // ____FIVector_1_HSTRING_INTERFACE_DEFINED__

#ifndef ____x_ABI_CWindows_CApplicationModel_CIPackageId_FWD_DEFINED__
#define ____x_ABI_CWindows_CApplicationModel_CIPackageId_FWD_DEFINED__
typedef interface __x_ABI_CWindows_CApplicationModel_CIPackageId __x_ABI_CWindows_CApplicationModel_CIPackageId;

#endif // ____x_ABI_CWindows_CApplicationModel_CIPackageId_FWD_DEFINED__

#ifndef ____x_ABI_CWindows_CFoundation_CIClosable_FWD_DEFINED__
#define ____x_ABI_CWindows_CFoundation_CIClosable_FWD_DEFINED__
typedef interface __x_ABI_CWindows_CFoundation_CIClosable __x_ABI_CWindows_CFoundation_CIClosable;

#endif // ____x_ABI_CWindows_CFoundation_CIClosable_FWD_DEFINED__

#ifndef ____x_ABI_CWindows_CFoundation_CIUriRuntimeClass_FWD_DEFINED__
#define ____x_ABI_CWindows_CFoundation_CIUriRuntimeClass_FWD_DEFINED__
typedef interface __x_ABI_CWindows_CFoundation_CIUriRuntimeClass __x_ABI_CWindows_CFoundation_CIUriRuntimeClass;

#endif // ____x_ABI_CWindows_CFoundation_CIUriRuntimeClass_FWD_DEFINED__

typedef enum __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CCatalogModelInstanceStatus __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CCatalogModelInstanceStatus;

typedef enum __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CCatalogModelStatus __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CCatalogModelStatus;

typedef enum __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CExecutionProviderCertification __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CExecutionProviderCertification;

typedef enum __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CExecutionProviderReadyResultState __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CExecutionProviderReadyResultState;

typedef enum __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CExecutionProviderReadyState __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CExecutionProviderReadyState;

/*
 *
 * Struct Microsoft.Windows.AI.MachineLearning.CatalogModelInstanceStatus
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 2.0
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
enum __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CCatalogModelInstanceStatus
{
    CatalogModelInstanceStatus_Available = 0,
    CatalogModelInstanceStatus_InProgress = 1,
    CatalogModelInstanceStatus_Unavailable = 2,
};
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

/*
 *
 * Struct Microsoft.Windows.AI.MachineLearning.CatalogModelStatus
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 2.0
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
enum __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CCatalogModelStatus
{
    CatalogModelStatus_Ready = 0,
    CatalogModelStatus_NotReady = 1,
};
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

/*
 *
 * Struct Microsoft.Windows.AI.MachineLearning.ExecutionProviderCertification
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 1.0
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000
enum __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CExecutionProviderCertification
{
    ExecutionProviderCertification_Unknown = 0,
    ExecutionProviderCertification_Certified = 1,
    ExecutionProviderCertification_Uncertified = 2,
};
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

/*
 *
 * Struct Microsoft.Windows.AI.MachineLearning.ExecutionProviderReadyResultState
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 1.0
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000
enum __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CExecutionProviderReadyResultState
{
    ExecutionProviderReadyResultState_InProgress = 0,
    ExecutionProviderReadyResultState_Success = 1,
    ExecutionProviderReadyResultState_Failure = 2,
};
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

/*
 *
 * Struct Microsoft.Windows.AI.MachineLearning.ExecutionProviderReadyState
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 1.0
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000
enum __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CExecutionProviderReadyState
{
    ExecutionProviderReadyState_Ready = 0,
    ExecutionProviderReadyState_NotReady = 1,
    ExecutionProviderReadyState_NotPresent = 2,
};
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

/*
 *
 * Interface Microsoft.Windows.AI.MachineLearning.ICatalogModelInfo
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 2.0
 *
 * Interface is a part of the implementation of type Microsoft.Windows.AI.MachineLearning.CatalogModelInfo
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
#if !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo_INTERFACE_DEFINED__)
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo_INTERFACE_DEFINED__
extern const __declspec(selectany) _Null_terminated_ WCHAR InterfaceName_Microsoft_Windows_AI_MachineLearning_ICatalogModelInfo[] = L"Microsoft.Windows.AI.MachineLearning.ICatalogModelInfo";
typedef struct __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfoVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo* This);
    ULONG (STDMETHODCALLTYPE* Release)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo* This);
    HRESULT (STDMETHODCALLTYPE* GetIids)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo* This,
        ULONG* iidCount,
        IID** iids);
    HRESULT (STDMETHODCALLTYPE* GetRuntimeClassName)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo* This,
        HSTRING* className);
    HRESULT (STDMETHODCALLTYPE* GetTrustLevel)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo* This,
        TrustLevel* trustLevel);
    HRESULT (STDMETHODCALLTYPE* get_Id)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo* This,
        HSTRING* value);
    HRESULT (STDMETHODCALLTYPE* get_Name)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo* This,
        HSTRING* value);
    HRESULT (STDMETHODCALLTYPE* get_Publisher)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo* This,
        HSTRING* value);
    HRESULT (STDMETHODCALLTYPE* get_SourceId)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo* This,
        HSTRING* value);
    HRESULT (STDMETHODCALLTYPE* get_Uri)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo* This,
        __x_ABI_CWindows_CFoundation_CIUriRuntimeClass** value);
    HRESULT (STDMETHODCALLTYPE* get_ExecutionProviders)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo* This,
        __FIVectorView_1_HSTRING** value);
    HRESULT (STDMETHODCALLTYPE* get_ModelSizeInBytes)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo* This,
        UINT64* value);
    HRESULT (STDMETHODCALLTYPE* get_Version)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo* This,
        HSTRING* value);
    HRESULT (STDMETHODCALLTYPE* get_License)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo* This,
        HSTRING* value);
    HRESULT (STDMETHODCALLTYPE* get_LicenseUri)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo* This,
        HSTRING* value);
    HRESULT (STDMETHODCALLTYPE* get_LicenseText)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo* This,
        HSTRING* value);
    HRESULT (STDMETHODCALLTYPE* GetStatus)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo* This,
        enum __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CCatalogModelStatus* result);
    HRESULT (STDMETHODCALLTYPE* GetInstanceAsync)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo* This,
        __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double** operation);
    HRESULT (STDMETHODCALLTYPE* GetInstanceAsync2)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo* This,
        __FIIterable_1___FIKeyValuePair_2_HSTRING_HSTRING* additionalHeaders,
        __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInstanceResult_double** operation);

    END_INTERFACE
} __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfoVtbl;

interface __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo
{
    CONST_VTBL struct __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfoVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo_GetIids(This, iidCount, iids) \
    ((This)->lpVtbl->GetIids(This, iidCount, iids))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo_GetRuntimeClassName(This, className) \
    ((This)->lpVtbl->GetRuntimeClassName(This, className))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo_GetTrustLevel(This, trustLevel) \
    ((This)->lpVtbl->GetTrustLevel(This, trustLevel))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo_get_Id(This, value) \
    ((This)->lpVtbl->get_Id(This, value))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo_get_Name(This, value) \
    ((This)->lpVtbl->get_Name(This, value))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo_get_Publisher(This, value) \
    ((This)->lpVtbl->get_Publisher(This, value))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo_get_SourceId(This, value) \
    ((This)->lpVtbl->get_SourceId(This, value))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo_get_Uri(This, value) \
    ((This)->lpVtbl->get_Uri(This, value))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo_get_ExecutionProviders(This, value) \
    ((This)->lpVtbl->get_ExecutionProviders(This, value))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo_get_ModelSizeInBytes(This, value) \
    ((This)->lpVtbl->get_ModelSizeInBytes(This, value))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo_get_Version(This, value) \
    ((This)->lpVtbl->get_Version(This, value))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo_get_License(This, value) \
    ((This)->lpVtbl->get_License(This, value))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo_get_LicenseUri(This, value) \
    ((This)->lpVtbl->get_LicenseUri(This, value))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo_get_LicenseText(This, value) \
    ((This)->lpVtbl->get_LicenseText(This, value))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo_GetStatus(This, result) \
    ((This)->lpVtbl->GetStatus(This, result))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo_GetInstanceAsync(This, operation) \
    ((This)->lpVtbl->GetInstanceAsync(This, operation))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo_GetInstanceAsync2(This, additionalHeaders, operation) \
    ((This)->lpVtbl->GetInstanceAsync2(This, additionalHeaders, operation))

#endif /* COBJMACROS */

EXTERN_C const IID IID___x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo;
#endif /* !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo_INTERFACE_DEFINED__) */
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

/*
 *
 * Interface Microsoft.Windows.AI.MachineLearning.ICatalogModelInstance
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 2.0
 *
 * Interface is a part of the implementation of type Microsoft.Windows.AI.MachineLearning.CatalogModelInstance
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
#if !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstance_INTERFACE_DEFINED__)
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstance_INTERFACE_DEFINED__
extern const __declspec(selectany) _Null_terminated_ WCHAR InterfaceName_Microsoft_Windows_AI_MachineLearning_ICatalogModelInstance[] = L"Microsoft.Windows.AI.MachineLearning.ICatalogModelInstance";
typedef struct __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstance* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstance* This);
    ULONG (STDMETHODCALLTYPE* Release)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstance* This);
    HRESULT (STDMETHODCALLTYPE* GetIids)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstance* This,
        ULONG* iidCount,
        IID** iids);
    HRESULT (STDMETHODCALLTYPE* GetRuntimeClassName)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstance* This,
        HSTRING* className);
    HRESULT (STDMETHODCALLTYPE* GetTrustLevel)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstance* This,
        TrustLevel* trustLevel);
    HRESULT (STDMETHODCALLTYPE* get_ModelPaths)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstance* This,
        __FIVectorView_1_HSTRING** value);
    HRESULT (STDMETHODCALLTYPE* get_ModelInfo)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstance* This,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo** value);

    END_INTERFACE
} __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceVtbl;

interface __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstance
{
    CONST_VTBL struct __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstance_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstance_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstance_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstance_GetIids(This, iidCount, iids) \
    ((This)->lpVtbl->GetIids(This, iidCount, iids))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstance_GetRuntimeClassName(This, className) \
    ((This)->lpVtbl->GetRuntimeClassName(This, className))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstance_GetTrustLevel(This, trustLevel) \
    ((This)->lpVtbl->GetTrustLevel(This, trustLevel))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstance_get_ModelPaths(This, value) \
    ((This)->lpVtbl->get_ModelPaths(This, value))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstance_get_ModelInfo(This, value) \
    ((This)->lpVtbl->get_ModelInfo(This, value))

#endif /* COBJMACROS */

EXTERN_C const IID IID___x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstance;
#endif /* !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstance_INTERFACE_DEFINED__) */
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

/*
 *
 * Interface Microsoft.Windows.AI.MachineLearning.ICatalogModelInstanceResult
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 2.0
 *
 * Interface is a part of the implementation of type Microsoft.Windows.AI.MachineLearning.CatalogModelInstanceResult
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
#if !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResult_INTERFACE_DEFINED__)
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResult_INTERFACE_DEFINED__
extern const __declspec(selectany) _Null_terminated_ WCHAR InterfaceName_Microsoft_Windows_AI_MachineLearning_ICatalogModelInstanceResult[] = L"Microsoft.Windows.AI.MachineLearning.ICatalogModelInstanceResult";
typedef struct __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResultVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResult* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResult* This);
    ULONG (STDMETHODCALLTYPE* Release)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResult* This);
    HRESULT (STDMETHODCALLTYPE* GetIids)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResult* This,
        ULONG* iidCount,
        IID** iids);
    HRESULT (STDMETHODCALLTYPE* GetRuntimeClassName)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResult* This,
        HSTRING* className);
    HRESULT (STDMETHODCALLTYPE* GetTrustLevel)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResult* This,
        TrustLevel* trustLevel);
    HRESULT (STDMETHODCALLTYPE* get_Status)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResult* This,
        enum __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CCatalogModelInstanceStatus* value);
    HRESULT (STDMETHODCALLTYPE* get_ExtendedError)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResult* This,
        HRESULT* value);
    HRESULT (STDMETHODCALLTYPE* get_DiagnosticText)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResult* This,
        HSTRING* value);
    HRESULT (STDMETHODCALLTYPE* GetInstance)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResult* This,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstance** result);

    END_INTERFACE
} __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResultVtbl;

interface __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResult
{
    CONST_VTBL struct __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResultVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResult_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResult_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResult_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResult_GetIids(This, iidCount, iids) \
    ((This)->lpVtbl->GetIids(This, iidCount, iids))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResult_GetRuntimeClassName(This, className) \
    ((This)->lpVtbl->GetRuntimeClassName(This, className))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResult_GetTrustLevel(This, trustLevel) \
    ((This)->lpVtbl->GetTrustLevel(This, trustLevel))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResult_get_Status(This, value) \
    ((This)->lpVtbl->get_Status(This, value))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResult_get_ExtendedError(This, value) \
    ((This)->lpVtbl->get_ExtendedError(This, value))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResult_get_DiagnosticText(This, value) \
    ((This)->lpVtbl->get_DiagnosticText(This, value))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResult_GetInstance(This, result) \
    ((This)->lpVtbl->GetInstance(This, result))

#endif /* COBJMACROS */

EXTERN_C const IID IID___x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResult;
#endif /* !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInstanceResult_INTERFACE_DEFINED__) */
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

/*
 *
 * Interface Microsoft.Windows.AI.MachineLearning.IExecutionProvider
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 1.0
 *
 * Interface is a part of the implementation of type Microsoft.Windows.AI.MachineLearning.ExecutionProvider
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000
#if !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider_INTERFACE_DEFINED__)
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider_INTERFACE_DEFINED__
extern const __declspec(selectany) _Null_terminated_ WCHAR InterfaceName_Microsoft_Windows_AI_MachineLearning_IExecutionProvider[] = L"Microsoft.Windows.AI.MachineLearning.IExecutionProvider";
typedef struct __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider* This);
    ULONG (STDMETHODCALLTYPE* Release)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider* This);
    HRESULT (STDMETHODCALLTYPE* GetIids)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider* This,
        ULONG* iidCount,
        IID** iids);
    HRESULT (STDMETHODCALLTYPE* GetRuntimeClassName)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider* This,
        HSTRING* className);
    HRESULT (STDMETHODCALLTYPE* GetTrustLevel)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider* This,
        TrustLevel* trustLevel);
    HRESULT (STDMETHODCALLTYPE* get_Name)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider* This,
        HSTRING* value);
    HRESULT (STDMETHODCALLTYPE* get_LibraryPath)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider* This,
        HSTRING* value);
    HRESULT (STDMETHODCALLTYPE* get_PackageId)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider* This,
        __x_ABI_CWindows_CApplicationModel_CIPackageId** value);
    HRESULT (STDMETHODCALLTYPE* get_ReadyState)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider* This,
        enum __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CExecutionProviderReadyState* value);
    HRESULT (STDMETHODCALLTYPE* get_Certification)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider* This,
        enum __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CExecutionProviderCertification* value);
    HRESULT (STDMETHODCALLTYPE* EnsureReadyAsync)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider* This,
        __FIAsyncOperationWithProgress_2_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProviderReadyResult_double** operation);
    HRESULT (STDMETHODCALLTYPE* TryRegister)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider* This,
        boolean* result);

    END_INTERFACE
} __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderVtbl;

interface __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider
{
    CONST_VTBL struct __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider_GetIids(This, iidCount, iids) \
    ((This)->lpVtbl->GetIids(This, iidCount, iids))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider_GetRuntimeClassName(This, className) \
    ((This)->lpVtbl->GetRuntimeClassName(This, className))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider_GetTrustLevel(This, trustLevel) \
    ((This)->lpVtbl->GetTrustLevel(This, trustLevel))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider_get_Name(This, value) \
    ((This)->lpVtbl->get_Name(This, value))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider_get_LibraryPath(This, value) \
    ((This)->lpVtbl->get_LibraryPath(This, value))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider_get_PackageId(This, value) \
    ((This)->lpVtbl->get_PackageId(This, value))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider_get_ReadyState(This, value) \
    ((This)->lpVtbl->get_ReadyState(This, value))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider_get_Certification(This, value) \
    ((This)->lpVtbl->get_Certification(This, value))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider_EnsureReadyAsync(This, operation) \
    ((This)->lpVtbl->EnsureReadyAsync(This, operation))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider_TryRegister(This, result) \
    ((This)->lpVtbl->TryRegister(This, result))

#endif /* COBJMACROS */

EXTERN_C const IID IID___x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider;
#endif /* !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider_INTERFACE_DEFINED__) */
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

/*
 *
 * Interface Microsoft.Windows.AI.MachineLearning.IExecutionProviderCatalog
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 1.0
 *
 * Interface is a part of the implementation of type Microsoft.Windows.AI.MachineLearning.ExecutionProviderCatalog
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000
#if !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalog_INTERFACE_DEFINED__)
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalog_INTERFACE_DEFINED__
extern const __declspec(selectany) _Null_terminated_ WCHAR InterfaceName_Microsoft_Windows_AI_MachineLearning_IExecutionProviderCatalog[] = L"Microsoft.Windows.AI.MachineLearning.IExecutionProviderCatalog";
typedef struct __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalogVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalog* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalog* This);
    ULONG (STDMETHODCALLTYPE* Release)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalog* This);
    HRESULT (STDMETHODCALLTYPE* GetIids)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalog* This,
        ULONG* iidCount,
        IID** iids);
    HRESULT (STDMETHODCALLTYPE* GetRuntimeClassName)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalog* This,
        HSTRING* className);
    HRESULT (STDMETHODCALLTYPE* GetTrustLevel)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalog* This,
        TrustLevel* trustLevel);
    HRESULT (STDMETHODCALLTYPE* FindAllProviders)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalog* This,
        UINT32* resultLength,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProvider*** result);
    HRESULT (STDMETHODCALLTYPE* EnsureAndRegisterCertifiedAsync)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalog* This,
        __FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double** operation);
    HRESULT (STDMETHODCALLTYPE* RegisterCertifiedAsync)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalog* This,
        __FIAsyncOperationWithProgress_2___FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CExecutionProvider_double** operation);

    END_INTERFACE
} __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalogVtbl;

interface __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalog
{
    CONST_VTBL struct __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalogVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalog_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalog_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalog_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalog_GetIids(This, iidCount, iids) \
    ((This)->lpVtbl->GetIids(This, iidCount, iids))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalog_GetRuntimeClassName(This, className) \
    ((This)->lpVtbl->GetRuntimeClassName(This, className))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalog_GetTrustLevel(This, trustLevel) \
    ((This)->lpVtbl->GetTrustLevel(This, trustLevel))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalog_FindAllProviders(This, resultLength, result) \
    ((This)->lpVtbl->FindAllProviders(This, resultLength, result))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalog_EnsureAndRegisterCertifiedAsync(This, operation) \
    ((This)->lpVtbl->EnsureAndRegisterCertifiedAsync(This, operation))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalog_RegisterCertifiedAsync(This, operation) \
    ((This)->lpVtbl->RegisterCertifiedAsync(This, operation))

#endif /* COBJMACROS */

EXTERN_C const IID IID___x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalog;
#endif /* !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalog_INTERFACE_DEFINED__) */
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

/*
 *
 * Interface Microsoft.Windows.AI.MachineLearning.IExecutionProviderCatalogStatics
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 1.0
 *
 * Interface is a part of the implementation of type Microsoft.Windows.AI.MachineLearning.ExecutionProviderCatalog
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000
#if !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalogStatics_INTERFACE_DEFINED__)
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalogStatics_INTERFACE_DEFINED__
extern const __declspec(selectany) _Null_terminated_ WCHAR InterfaceName_Microsoft_Windows_AI_MachineLearning_IExecutionProviderCatalogStatics[] = L"Microsoft.Windows.AI.MachineLearning.IExecutionProviderCatalogStatics";
typedef struct __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalogStaticsVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalogStatics* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalogStatics* This);
    ULONG (STDMETHODCALLTYPE* Release)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalogStatics* This);
    HRESULT (STDMETHODCALLTYPE* GetIids)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalogStatics* This,
        ULONG* iidCount,
        IID** iids);
    HRESULT (STDMETHODCALLTYPE* GetRuntimeClassName)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalogStatics* This,
        HSTRING* className);
    HRESULT (STDMETHODCALLTYPE* GetTrustLevel)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalogStatics* This,
        TrustLevel* trustLevel);
    HRESULT (STDMETHODCALLTYPE* GetDefault)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalogStatics* This,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalog** result);

    END_INTERFACE
} __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalogStaticsVtbl;

interface __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalogStatics
{
    CONST_VTBL struct __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalogStaticsVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalogStatics_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalogStatics_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalogStatics_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalogStatics_GetIids(This, iidCount, iids) \
    ((This)->lpVtbl->GetIids(This, iidCount, iids))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalogStatics_GetRuntimeClassName(This, className) \
    ((This)->lpVtbl->GetRuntimeClassName(This, className))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalogStatics_GetTrustLevel(This, trustLevel) \
    ((This)->lpVtbl->GetTrustLevel(This, trustLevel))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalogStatics_GetDefault(This, result) \
    ((This)->lpVtbl->GetDefault(This, result))

#endif /* COBJMACROS */

EXTERN_C const IID IID___x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalogStatics;
#endif /* !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderCatalogStatics_INTERFACE_DEFINED__) */
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

/*
 *
 * Interface Microsoft.Windows.AI.MachineLearning.IExecutionProviderReadyResult
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 1.0
 *
 * Interface is a part of the implementation of type Microsoft.Windows.AI.MachineLearning.ExecutionProviderReadyResult
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000
#if !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderReadyResult_INTERFACE_DEFINED__)
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderReadyResult_INTERFACE_DEFINED__
extern const __declspec(selectany) _Null_terminated_ WCHAR InterfaceName_Microsoft_Windows_AI_MachineLearning_IExecutionProviderReadyResult[] = L"Microsoft.Windows.AI.MachineLearning.IExecutionProviderReadyResult";
typedef struct __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderReadyResultVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderReadyResult* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderReadyResult* This);
    ULONG (STDMETHODCALLTYPE* Release)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderReadyResult* This);
    HRESULT (STDMETHODCALLTYPE* GetIids)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderReadyResult* This,
        ULONG* iidCount,
        IID** iids);
    HRESULT (STDMETHODCALLTYPE* GetRuntimeClassName)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderReadyResult* This,
        HSTRING* className);
    HRESULT (STDMETHODCALLTYPE* GetTrustLevel)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderReadyResult* This,
        TrustLevel* trustLevel);
    HRESULT (STDMETHODCALLTYPE* get_Status)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderReadyResult* This,
        enum __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CExecutionProviderReadyResultState* value);
    HRESULT (STDMETHODCALLTYPE* get_ExtendedError)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderReadyResult* This,
        HRESULT* value);
    HRESULT (STDMETHODCALLTYPE* get_DiagnosticText)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderReadyResult* This,
        HSTRING* value);

    END_INTERFACE
} __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderReadyResultVtbl;

interface __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderReadyResult
{
    CONST_VTBL struct __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderReadyResultVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderReadyResult_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderReadyResult_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderReadyResult_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderReadyResult_GetIids(This, iidCount, iids) \
    ((This)->lpVtbl->GetIids(This, iidCount, iids))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderReadyResult_GetRuntimeClassName(This, className) \
    ((This)->lpVtbl->GetRuntimeClassName(This, className))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderReadyResult_GetTrustLevel(This, trustLevel) \
    ((This)->lpVtbl->GetTrustLevel(This, trustLevel))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderReadyResult_get_Status(This, value) \
    ((This)->lpVtbl->get_Status(This, value))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderReadyResult_get_ExtendedError(This, value) \
    ((This)->lpVtbl->get_ExtendedError(This, value))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderReadyResult_get_DiagnosticText(This, value) \
    ((This)->lpVtbl->get_DiagnosticText(This, value))

#endif /* COBJMACROS */

EXTERN_C const IID IID___x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderReadyResult;
#endif /* !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIExecutionProviderReadyResult_INTERFACE_DEFINED__) */
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

/*
 *
 * Interface Microsoft.Windows.AI.MachineLearning.IModelCatalog
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 2.0
 *
 * Interface is a part of the implementation of type Microsoft.Windows.AI.MachineLearning.ModelCatalog
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
#if !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog_INTERFACE_DEFINED__)
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog_INTERFACE_DEFINED__
extern const __declspec(selectany) _Null_terminated_ WCHAR InterfaceName_Microsoft_Windows_AI_MachineLearning_IModelCatalog[] = L"Microsoft.Windows.AI.MachineLearning.IModelCatalog";
typedef struct __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog* This);
    ULONG (STDMETHODCALLTYPE* Release)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog* This);
    HRESULT (STDMETHODCALLTYPE* GetIids)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog* This,
        ULONG* iidCount,
        IID** iids);
    HRESULT (STDMETHODCALLTYPE* GetRuntimeClassName)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog* This,
        HSTRING* className);
    HRESULT (STDMETHODCALLTYPE* GetTrustLevel)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog* This,
        TrustLevel* trustLevel);
    HRESULT (STDMETHODCALLTYPE* get_Sources)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog* This,
        __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource** value);
    HRESULT (STDMETHODCALLTYPE* get_ExecutionProviders)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog* This,
        __FIVector_1_HSTRING** value);
    HRESULT (STDMETHODCALLTYPE* GetAvailableModel)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog* This,
        HSTRING idOrName,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CICatalogModelInfo** result);
    HRESULT (STDMETHODCALLTYPE* GetAvailableModels)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog* This,
        __FIVector_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo** result);
    HRESULT (STDMETHODCALLTYPE* FindModelAsync)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog* This,
        HSTRING idOrName,
        __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo** operation);
    HRESULT (STDMETHODCALLTYPE* FindAllModelsAsync)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog* This,
        __FIAsyncOperation_1___FIVectorView_1_Microsoft__CWindows__CAI__CMachineLearning__CCatalogModelInfo** operation);

    END_INTERFACE
} __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogVtbl;

interface __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog
{
    CONST_VTBL struct __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog_GetIids(This, iidCount, iids) \
    ((This)->lpVtbl->GetIids(This, iidCount, iids))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog_GetRuntimeClassName(This, className) \
    ((This)->lpVtbl->GetRuntimeClassName(This, className))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog_GetTrustLevel(This, trustLevel) \
    ((This)->lpVtbl->GetTrustLevel(This, trustLevel))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog_get_Sources(This, value) \
    ((This)->lpVtbl->get_Sources(This, value))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog_get_ExecutionProviders(This, value) \
    ((This)->lpVtbl->get_ExecutionProviders(This, value))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog_GetAvailableModel(This, idOrName, result) \
    ((This)->lpVtbl->GetAvailableModel(This, idOrName, result))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog_GetAvailableModels(This, result) \
    ((This)->lpVtbl->GetAvailableModels(This, result))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog_FindModelAsync(This, idOrName, operation) \
    ((This)->lpVtbl->FindModelAsync(This, idOrName, operation))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog_FindAllModelsAsync(This, operation) \
    ((This)->lpVtbl->FindAllModelsAsync(This, operation))

#endif /* COBJMACROS */

EXTERN_C const IID IID___x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog;
#endif /* !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog_INTERFACE_DEFINED__) */
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

/*
 *
 * Interface Microsoft.Windows.AI.MachineLearning.IModelCatalogFactory
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 2.0
 *
 * Interface is a part of the implementation of type Microsoft.Windows.AI.MachineLearning.ModelCatalog
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
#if !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogFactory_INTERFACE_DEFINED__)
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogFactory_INTERFACE_DEFINED__
extern const __declspec(selectany) _Null_terminated_ WCHAR InterfaceName_Microsoft_Windows_AI_MachineLearning_IModelCatalogFactory[] = L"Microsoft.Windows.AI.MachineLearning.IModelCatalogFactory";
typedef struct __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogFactoryVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogFactory* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogFactory* This);
    ULONG (STDMETHODCALLTYPE* Release)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogFactory* This);
    HRESULT (STDMETHODCALLTYPE* GetIids)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogFactory* This,
        ULONG* iidCount,
        IID** iids);
    HRESULT (STDMETHODCALLTYPE* GetRuntimeClassName)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogFactory* This,
        HSTRING* className);
    HRESULT (STDMETHODCALLTYPE* GetTrustLevel)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogFactory* This,
        TrustLevel* trustLevel);
    HRESULT (STDMETHODCALLTYPE* CreateInstance)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogFactory* This,
        UINT32 sourcesLength,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource** sources,
        __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalog** value);

    END_INTERFACE
} __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogFactoryVtbl;

interface __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogFactory
{
    CONST_VTBL struct __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogFactoryVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogFactory_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogFactory_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogFactory_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogFactory_GetIids(This, iidCount, iids) \
    ((This)->lpVtbl->GetIids(This, iidCount, iids))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogFactory_GetRuntimeClassName(This, className) \
    ((This)->lpVtbl->GetRuntimeClassName(This, className))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogFactory_GetTrustLevel(This, trustLevel) \
    ((This)->lpVtbl->GetTrustLevel(This, trustLevel))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogFactory_CreateInstance(This, sourcesLength, sources, value) \
    ((This)->lpVtbl->CreateInstance(This, sourcesLength, sources, value))

#endif /* COBJMACROS */

EXTERN_C const IID IID___x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogFactory;
#endif /* !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogFactory_INTERFACE_DEFINED__) */
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

/*
 *
 * Interface Microsoft.Windows.AI.MachineLearning.IModelCatalogSource
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 2.0
 *
 * Interface is a part of the implementation of type Microsoft.Windows.AI.MachineLearning.ModelCatalogSource
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
#if !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource_INTERFACE_DEFINED__)
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource_INTERFACE_DEFINED__
extern const __declspec(selectany) _Null_terminated_ WCHAR InterfaceName_Microsoft_Windows_AI_MachineLearning_IModelCatalogSource[] = L"Microsoft.Windows.AI.MachineLearning.IModelCatalogSource";
typedef struct __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSourceVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource* This);
    ULONG (STDMETHODCALLTYPE* Release)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource* This);
    HRESULT (STDMETHODCALLTYPE* GetIids)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource* This,
        ULONG* iidCount,
        IID** iids);
    HRESULT (STDMETHODCALLTYPE* GetRuntimeClassName)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource* This,
        HSTRING* className);
    HRESULT (STDMETHODCALLTYPE* GetTrustLevel)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource* This,
        TrustLevel* trustLevel);
    HRESULT (STDMETHODCALLTYPE* get_Id)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource* This,
        HSTRING* value);
    HRESULT (STDMETHODCALLTYPE* get_Uri)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource* This,
        __x_ABI_CWindows_CFoundation_CIUriRuntimeClass** value);

    END_INTERFACE
} __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSourceVtbl;

interface __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource
{
    CONST_VTBL struct __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSourceVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource_GetIids(This, iidCount, iids) \
    ((This)->lpVtbl->GetIids(This, iidCount, iids))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource_GetRuntimeClassName(This, className) \
    ((This)->lpVtbl->GetRuntimeClassName(This, className))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource_GetTrustLevel(This, trustLevel) \
    ((This)->lpVtbl->GetTrustLevel(This, trustLevel))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource_get_Id(This, value) \
    ((This)->lpVtbl->get_Id(This, value))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource_get_Uri(This, value) \
    ((This)->lpVtbl->get_Uri(This, value))

#endif /* COBJMACROS */

EXTERN_C const IID IID___x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource;
#endif /* !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSource_INTERFACE_DEFINED__) */
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

/*
 *
 * Interface Microsoft.Windows.AI.MachineLearning.IModelCatalogSourceStatics
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 2.0
 *
 * Interface is a part of the implementation of type Microsoft.Windows.AI.MachineLearning.ModelCatalogSource
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
#if !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSourceStatics_INTERFACE_DEFINED__)
#define ____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSourceStatics_INTERFACE_DEFINED__
extern const __declspec(selectany) _Null_terminated_ WCHAR InterfaceName_Microsoft_Windows_AI_MachineLearning_IModelCatalogSourceStatics[] = L"Microsoft.Windows.AI.MachineLearning.IModelCatalogSourceStatics";
typedef struct __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSourceStaticsVtbl
{
    BEGIN_INTERFACE

    HRESULT (STDMETHODCALLTYPE* QueryInterface)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSourceStatics* This,
        REFIID riid,
        void** ppvObject);
    ULONG (STDMETHODCALLTYPE* AddRef)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSourceStatics* This);
    ULONG (STDMETHODCALLTYPE* Release)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSourceStatics* This);
    HRESULT (STDMETHODCALLTYPE* GetIids)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSourceStatics* This,
        ULONG* iidCount,
        IID** iids);
    HRESULT (STDMETHODCALLTYPE* GetRuntimeClassName)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSourceStatics* This,
        HSTRING* className);
    HRESULT (STDMETHODCALLTYPE* GetTrustLevel)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSourceStatics* This,
        TrustLevel* trustLevel);
    HRESULT (STDMETHODCALLTYPE* CreateFromUriAsync)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSourceStatics* This,
        __x_ABI_CWindows_CFoundation_CIUriRuntimeClass* location,
        __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource** operation);
    HRESULT (STDMETHODCALLTYPE* CreateFromUriAsync2)(__x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSourceStatics* This,
        __x_ABI_CWindows_CFoundation_CIUriRuntimeClass* location,
        __FIIterable_1___FIKeyValuePair_2_HSTRING_HSTRING* additionalHeaders,
        __FIAsyncOperation_1_Microsoft__CWindows__CAI__CMachineLearning__CModelCatalogSource** operation);

    END_INTERFACE
} __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSourceStaticsVtbl;

interface __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSourceStatics
{
    CONST_VTBL struct __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSourceStaticsVtbl* lpVtbl;
};

#ifdef COBJMACROS

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSourceStatics_QueryInterface(This, riid, ppvObject) \
    ((This)->lpVtbl->QueryInterface(This, riid, ppvObject))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSourceStatics_AddRef(This) \
    ((This)->lpVtbl->AddRef(This))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSourceStatics_Release(This) \
    ((This)->lpVtbl->Release(This))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSourceStatics_GetIids(This, iidCount, iids) \
    ((This)->lpVtbl->GetIids(This, iidCount, iids))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSourceStatics_GetRuntimeClassName(This, className) \
    ((This)->lpVtbl->GetRuntimeClassName(This, className))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSourceStatics_GetTrustLevel(This, trustLevel) \
    ((This)->lpVtbl->GetTrustLevel(This, trustLevel))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSourceStatics_CreateFromUriAsync(This, location, operation) \
    ((This)->lpVtbl->CreateFromUriAsync(This, location, operation))

#define __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSourceStatics_CreateFromUriAsync2(This, location, additionalHeaders, operation) \
    ((This)->lpVtbl->CreateFromUriAsync2(This, location, additionalHeaders, operation))

#endif /* COBJMACROS */

EXTERN_C const IID IID___x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSourceStatics;
#endif /* !defined(____x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CIModelCatalogSourceStatics_INTERFACE_DEFINED__) */
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

/*
 *
 * Class Microsoft.Windows.AI.MachineLearning.CatalogModelInfo
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 2.0
 *
 * Class implements the following interfaces:
 *    Microsoft.Windows.AI.MachineLearning.ICatalogModelInfo ** Default Interface **
 *
 * Class Marshaling Behavior:  Agile - Class is agile
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
#ifndef RUNTIMECLASS_Microsoft_Windows_AI_MachineLearning_CatalogModelInfo_DEFINED
#define RUNTIMECLASS_Microsoft_Windows_AI_MachineLearning_CatalogModelInfo_DEFINED
extern const __declspec(selectany) _Null_terminated_ WCHAR RuntimeClass_Microsoft_Windows_AI_MachineLearning_CatalogModelInfo[] = L"Microsoft.Windows.AI.MachineLearning.CatalogModelInfo";
#endif
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

/*
 *
 * Class Microsoft.Windows.AI.MachineLearning.CatalogModelInstance
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 2.0
 *
 * Class implements the following interfaces:
 *    Microsoft.Windows.AI.MachineLearning.ICatalogModelInstance ** Default Interface **
 *    Windows.Foundation.IClosable
 *
 * Class Marshaling Behavior:  Agile - Class is agile
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
#ifndef RUNTIMECLASS_Microsoft_Windows_AI_MachineLearning_CatalogModelInstance_DEFINED
#define RUNTIMECLASS_Microsoft_Windows_AI_MachineLearning_CatalogModelInstance_DEFINED
extern const __declspec(selectany) _Null_terminated_ WCHAR RuntimeClass_Microsoft_Windows_AI_MachineLearning_CatalogModelInstance[] = L"Microsoft.Windows.AI.MachineLearning.CatalogModelInstance";
#endif
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

/*
 *
 * Class Microsoft.Windows.AI.MachineLearning.CatalogModelInstanceResult
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 2.0
 *
 * Class implements the following interfaces:
 *    Microsoft.Windows.AI.MachineLearning.ICatalogModelInstanceResult ** Default Interface **
 *
 * Class Marshaling Behavior:  Agile - Class is agile
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
#ifndef RUNTIMECLASS_Microsoft_Windows_AI_MachineLearning_CatalogModelInstanceResult_DEFINED
#define RUNTIMECLASS_Microsoft_Windows_AI_MachineLearning_CatalogModelInstanceResult_DEFINED
extern const __declspec(selectany) _Null_terminated_ WCHAR RuntimeClass_Microsoft_Windows_AI_MachineLearning_CatalogModelInstanceResult[] = L"Microsoft.Windows.AI.MachineLearning.CatalogModelInstanceResult";
#endif
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

/*
 *
 * Class Microsoft.Windows.AI.MachineLearning.ExecutionProvider
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 1.0
 *
 * Class implements the following interfaces:
 *    Microsoft.Windows.AI.MachineLearning.IExecutionProvider ** Default Interface **
 *
 * Class Marshaling Behavior:  Agile - Class is agile
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000
#ifndef RUNTIMECLASS_Microsoft_Windows_AI_MachineLearning_ExecutionProvider_DEFINED
#define RUNTIMECLASS_Microsoft_Windows_AI_MachineLearning_ExecutionProvider_DEFINED
extern const __declspec(selectany) _Null_terminated_ WCHAR RuntimeClass_Microsoft_Windows_AI_MachineLearning_ExecutionProvider[] = L"Microsoft.Windows.AI.MachineLearning.ExecutionProvider";
#endif
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

/*
 *
 * Class Microsoft.Windows.AI.MachineLearning.ExecutionProviderCatalog
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 1.0
 *
 * RuntimeClass contains static methods.
 *   Static Methods exist on the Microsoft.Windows.AI.MachineLearning.IExecutionProviderCatalogStatics interface starting with version 1.0 of the Microsoft.Windows.AI.MachineLearning.MachineLearningContract API contract
 *
 * Class implements the following interfaces:
 *    Microsoft.Windows.AI.MachineLearning.IExecutionProviderCatalog ** Default Interface **
 *
 * Class Threading Model:  Both Single and Multi Threaded Apartment
 *
 * Class Marshaling Behavior:  Agile - Class is agile
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000
#ifndef RUNTIMECLASS_Microsoft_Windows_AI_MachineLearning_ExecutionProviderCatalog_DEFINED
#define RUNTIMECLASS_Microsoft_Windows_AI_MachineLearning_ExecutionProviderCatalog_DEFINED
extern const __declspec(selectany) _Null_terminated_ WCHAR RuntimeClass_Microsoft_Windows_AI_MachineLearning_ExecutionProviderCatalog[] = L"Microsoft.Windows.AI.MachineLearning.ExecutionProviderCatalog";
#endif
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

/*
 *
 * Class Microsoft.Windows.AI.MachineLearning.ExecutionProviderReadyResult
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 1.0
 *
 * Class implements the following interfaces:
 *    Microsoft.Windows.AI.MachineLearning.IExecutionProviderReadyResult ** Default Interface **
 *
 * Class Marshaling Behavior:  Agile - Class is agile
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000
#ifndef RUNTIMECLASS_Microsoft_Windows_AI_MachineLearning_ExecutionProviderReadyResult_DEFINED
#define RUNTIMECLASS_Microsoft_Windows_AI_MachineLearning_ExecutionProviderReadyResult_DEFINED
extern const __declspec(selectany) _Null_terminated_ WCHAR RuntimeClass_Microsoft_Windows_AI_MachineLearning_ExecutionProviderReadyResult[] = L"Microsoft.Windows.AI.MachineLearning.ExecutionProviderReadyResult";
#endif
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x10000

/*
 *
 * Class Microsoft.Windows.AI.MachineLearning.ModelCatalog
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 2.0
 *
 * RuntimeClass can be activated.
 *   Type can be activated via the Microsoft.Windows.AI.MachineLearning.IModelCatalogFactory interface starting with version 2.0 of the Microsoft.Windows.AI.MachineLearning.MachineLearningContract API contract
 *
 * Class implements the following interfaces:
 *    Microsoft.Windows.AI.MachineLearning.IModelCatalog ** Default Interface **
 *
 * Class Threading Model:  Both Single and Multi Threaded Apartment
 *
 * Class Marshaling Behavior:  Agile - Class is agile
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
#ifndef RUNTIMECLASS_Microsoft_Windows_AI_MachineLearning_ModelCatalog_DEFINED
#define RUNTIMECLASS_Microsoft_Windows_AI_MachineLearning_ModelCatalog_DEFINED
extern const __declspec(selectany) _Null_terminated_ WCHAR RuntimeClass_Microsoft_Windows_AI_MachineLearning_ModelCatalog[] = L"Microsoft.Windows.AI.MachineLearning.ModelCatalog";
#endif
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

/*
 *
 * Class Microsoft.Windows.AI.MachineLearning.ModelCatalogSource
 *
 * Introduced to Microsoft.Windows.AI.MachineLearning.MachineLearningContract in version 2.0
 *
 * RuntimeClass contains static methods.
 *   Static Methods exist on the Microsoft.Windows.AI.MachineLearning.IModelCatalogSourceStatics interface starting with version 2.0 of the Microsoft.Windows.AI.MachineLearning.MachineLearningContract API contract
 *
 * Class implements the following interfaces:
 *    Microsoft.Windows.AI.MachineLearning.IModelCatalogSource ** Default Interface **
 *
 * Class Threading Model:  Both Single and Multi Threaded Apartment
 *
 * Class Marshaling Behavior:  Agile - Class is agile
 *
 */
#if MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000
#ifndef RUNTIMECLASS_Microsoft_Windows_AI_MachineLearning_ModelCatalogSource_DEFINED
#define RUNTIMECLASS_Microsoft_Windows_AI_MachineLearning_ModelCatalogSource_DEFINED
extern const __declspec(selectany) _Null_terminated_ WCHAR RuntimeClass_Microsoft_Windows_AI_MachineLearning_ModelCatalogSource[] = L"Microsoft.Windows.AI.MachineLearning.ModelCatalogSource";
#endif
#endif // MICROSOFT_WINDOWS_AI_MACHINELEARNING_MACHINELEARNINGCONTRACT_VERSION >= 0x20000

#endif // defined(__cplusplus)
#pragma pop_macro("MIDL_CONST_ID")
#endif // __microsoft2Ewindows2Eai2Emachinelearning_p_h__

#endif // __microsoft2Ewindows2Eai2Emachinelearning_h__
