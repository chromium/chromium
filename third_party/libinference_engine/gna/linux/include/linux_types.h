//*****************************************************************************
// Copyright (C) 2018 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions
// and limitations under the License.
//
//
// SPDX-License-Identifier: Apache-2.0
//*****************************************************************************

#ifndef LINUX_TYPES_H_
#define LINUX_TYPES_H_

/* Integer types */

/* Exact-width integer types */


#define POINTER_64

typedef signed char             INT8, *PINT8;
typedef signed short            INT16, *PINT16;
typedef signed int              INT32, *PINT32;
typedef long long int           INT64, *PINT64;
typedef unsigned char           UINT8, *PUINT8;
typedef unsigned short          UINT16, *PUINT16;
typedef unsigned int            UINT32, *PUINT32;
typedef unsigned long long int  UINT64, *PUINT64;

typedef unsigned long long 	LARGE_INTEGER;
typedef unsigned long 		DWORD;
typedef long 			HRESULT;



#define PtrToUint( p ) ((unsigned int) (unsigned int)(p) )

#define UINT_MAX                0xffffffff    /* maximum unsigned int value */

#ifndef FALSE
#define FALSE                   0
#endif

#ifndef TRUE
#define TRUE                    1
#endif




#endif /* LINUX_TYPES_H_ */
