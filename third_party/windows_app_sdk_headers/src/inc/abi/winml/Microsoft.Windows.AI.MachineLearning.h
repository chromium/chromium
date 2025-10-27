// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License.


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

// Parameterized interface forward declarations (C++)

// Collection interface definitions
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
                    class ExecutionProviderCatalog;
                } /* MachineLearning */
            } /* AI */
        } /* Windows */
    } /* Microsoft */
} /* ABI */

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

#else // !defined(__cplusplus)
/* Forward Declarations */
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

// Parameterized interface forward declarations (C)

// Collection interface definitions

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

#ifndef ____x_ABI_CWindows_CApplicationModel_CIPackageId_FWD_DEFINED__
#define ____x_ABI_CWindows_CApplicationModel_CIPackageId_FWD_DEFINED__
typedef interface __x_ABI_CWindows_CApplicationModel_CIPackageId __x_ABI_CWindows_CApplicationModel_CIPackageId;

#endif // ____x_ABI_CWindows_CApplicationModel_CIPackageId_FWD_DEFINED__

typedef enum __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CExecutionProviderCertification __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CExecutionProviderCertification;

typedef enum __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CExecutionProviderReadyResultState __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CExecutionProviderReadyResultState;

typedef enum __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CExecutionProviderReadyState __x_ABI_CMicrosoft_CWindows_CAI_CMachineLearning_CExecutionProviderReadyState;

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

#endif // defined(__cplusplus)
#pragma pop_macro("MIDL_CONST_ID")
#endif // __microsoft2Ewindows2Eai2Emachinelearning_p_h__

#endif // __microsoft2Ewindows2Eai2Emachinelearning_h__
