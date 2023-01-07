/*
This file is part of NYX.

Copyright (c) 2021 Sergej Schumilo
Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
#ifndef KAFL_USER_H
#define KAFL_USER_H

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#ifndef __MINGW64__
#include <sys/mman.h>
#endif

#ifdef __MINGW64__
#ifndef uint64_t
#define uint64_t UINT64
#endif
#ifndef int32_t
#define int32_t INT32
#endif
#ifndef uint8_t
#define uint8_t UINT8
#endif
#else 
#include <stdint.h>
#endif

#define HYPERCALL_KAFL_RAX_ID				0x01f
#define HYPERCALL_KAFL_ACQUIRE				0
#define HYPERCALL_KAFL_GET_PAYLOAD			1

/* deprecated */
#define HYPERCALL_KAFL_GET_PROGRAM			2
/* deprecated */
#define HYPERCALL_KAFL_GET_ARGV				3

#define HYPERCALL_KAFL_RELEASE				4
#define HYPERCALL_KAFL_SUBMIT_CR3			5
#define HYPERCALL_KAFL_SUBMIT_PANIC			6

/* deprecated */
#define HYPERCALL_KAFL_SUBMIT_KASAN			7

#define HYPERCALL_KAFL_PANIC				8

/* deprecated */
#define HYPERCALL_KAFL_KASAN				9
#define HYPERCALL_KAFL_LOCK					10

/* deprecated */
#define HYPERCALL_KAFL_INFO					11

#define HYPERCALL_KAFL_NEXT_PAYLOAD			12
#define HYPERCALL_KAFL_PRINTF				13

/* deprecated */
#define HYPERCALL_KAFL_PRINTK_ADDR			14
/* deprecated */
#define HYPERCALL_KAFL_PRINTK				15

/* user space only hypercalls */
#define HYPERCALL_KAFL_USER_RANGE_ADVISE	16
#define HYPERCALL_KAFL_USER_SUBMIT_MODE		17
#define HYPERCALL_KAFL_USER_FAST_ACQUIRE	18
/* 19 is already used for exit reason KVM_EXIT_KAFL_TOPA_MAIN_FULL */
#define HYPERCALL_KAFL_USER_ABORT			20
#define HYPERCALL_KAFL_RANGE_SUBMIT		29
#define HYPERCALL_KAFL_REQ_STREAM_DATA		30
#define HYPERCALL_KAFL_PANIC_EXTENDED		32

#define HYPERCALL_KAFL_CREATE_TMP_SNAPSHOT 33
#define HYPERCALL_KAFL_DEBUG_TMP_SNAPSHOT 34 /* hypercall for debugging / development purposes */

#define HYPERCALL_KAFL_GET_HOST_CONFIG 35
#define HYPERCALL_KAFL_SET_AGENT_CONFIG 36

#define HYPERCALL_KAFL_DUMP_FILE 37

#define HYPERCALL_KAFL_REQ_STREAM_DATA_BULK 38
#define HYPERCALL_KAFL_PERSIST_PAGE_PAST_SNAPSHOT 39

/* hypertrash only hypercalls */
#define HYPERTRASH_HYPERCALL_MASK			0xAA000000

#define HYPERCALL_KAFL_NESTED_PREPARE		(0 | HYPERTRASH_HYPERCALL_MASK)
#define HYPERCALL_KAFL_NESTED_CONFIG		(1 | HYPERTRASH_HYPERCALL_MASK)
#define HYPERCALL_KAFL_NESTED_ACQUIRE		(2 | HYPERTRASH_HYPERCALL_MASK)
#define HYPERCALL_KAFL_NESTED_RELEASE		(3 | HYPERTRASH_HYPERCALL_MASK)
#define HYPERCALL_KAFL_NESTED_HPRINTF		(4 | HYPERTRASH_HYPERCALL_MASK)gre

#define HPRINTF_MAX_SIZE					0x1000					/* up to 4KB hprintf strings */

/* specific defines to enable support for NYX hypercalls on unmodified KVM builds */
/* PIO port number used by VMWare backdoor */
#define VMWARE_PORT   0x5658
/* slightly changed RAX_ID to avoid vmware backdoor collisions */
#define HYPERCALL_KAFL_RAX_ID_VMWARE				0x8080801f

typedef struct{
	int32_t size;
	uint8_t data[];
} kAFL_payload;

typedef struct{
	uint64_t ip[4];
	uint64_t size[4];
	uint8_t enabled[4];
} kAFL_ranges; 

#define KAFL_MODE_64	0
#define KAFL_MODE_32	1
#define KAFL_MODE_16	2

#if defined(__i386__)
#define KAFL_HYPERCALL_NO_PT(_ebx, _ecx) ({ \
	uint32_t _eax = HYPERCALL_KAFL_RAX_ID_VMWARE; \
	do{ \
	asm volatile( \
		"outl %%eax, %%dx;" \
	: "+a" (_eax) \
	: "b" (_ebx), "c" (_ecx), "d" (VMWARE_PORT) \
	: "cc", "memory" \
	); \
	} while(0); \
	_eax; \
})

#define KAFL_HYPERCALL_PT(_ebx, _ecx) ({ \
	uint32_t _eax = HYPERCALL_KAFL_RAX_ID; \
	do{ \
	asm volatile( \
		"vmcall;" \
	: "+a" (_eax) \
	: "b" (_ebx), "c" (_ecx) \
	: "cc", "memory" \
	); \
	} while(0); \
	_eax; \
})

#else

#define KAFL_HYPERCALL_NO_PT(_rbx, _rcx) ({ \
	uint64_t _rax = HYPERCALL_KAFL_RAX_ID_VMWARE; \
	do{ \
	asm volatile( \
		"outl %%eax, %%dx;" \
	: "+a" (_rax) \
	: "b" (_rbx), "c" (_rcx), "d" (VMWARE_PORT) \
	: "cc", "memory" \
	); \
	} while(0); \
	_rax; \
})

#define KAFL_HYPERCALL_PT(_rbx, _rcx) ({ \
	uint64_t _rax = HYPERCALL_KAFL_RAX_ID; \
	do{ \
	asm volatile( \
		"vmcall;" \
	: "+a" (_rax) \
	: "b" (_rbx), "c" (_rcx) \
	: "cc", "memory" \
	); \
	} while(0); \
	_rax; \
})
#endif


#if defined(__i386__)
#ifdef NO_PT_NYX

#define KAFL_HYPERCALL(__rbx, __rcx) \
	KAFL_HYPERCALL_NO_PT(_rbx, _rcx); \
}while(0)

static inline uint32_t kAFL_hypercall(uint32_t rbx, uint32_t rcx){
	return KAFL_HYPERCALL_NO_PT(rbx, rcx);
}
#else
#define KAFL_HYPERCALL(__rbx, __rcx) \
	KAFL_HYPERCALL_PT(_rbx, _rcx); \
}while(0)

static inline uint32_t kAFL_hypercall(uint32_t rbx, uint32_t rcx){
# ifndef __NOKAFL
	return KAFL_HYPERCALL_PT(rbx, rcx);
# endif
	return 0;
}
#endif
#elif defined(__x86_64__)
#ifdef NO_PT_NYX

#define KAFL_HYPERCALL(__rbx, __rcx) \
	KAFL_HYPERCALL_NO_PT(_rbx, _rcx); \
}while(0)

static inline uint64_t kAFL_hypercall(uint64_t rbx, uint64_t rcx){
	return KAFL_HYPERCALL_NO_PT(rbx, rcx);
}
#else
#define KAFL_HYPERCALL(__rbx, __rcx) \
	KAFL_HYPERCALL_PT(_rbx, _rcx); \
}while(0)

static inline uint64_t kAFL_hypercall(uint64_t rbx, uint64_t rcx){
# ifndef __NOKAFL
	return KAFL_HYPERCALL_PT(rbx, rcx);
# endif
	return 0;
}
#endif
#endif

//extern uint8_t* hprintf_buffer; 

static inline uint8_t alloc_hprintf_buffer(uint8_t** hprintf_buffer){
	if(!*hprintf_buffer){
#ifdef __MINGW64__
		*hprintf_buffer = (uint8_t*)VirtualAlloc(0, HPRINTF_MAX_SIZE, MEM_COMMIT, PAGE_READWRITE);
#else 
		*hprintf_buffer = (uint8_t*)mmap((void*)NULL, HPRINTF_MAX_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#endif
		if(!*hprintf_buffer){
			return 0;
		}
	}
	return 1; 
}

#ifdef __NOKAFL
int (*hprintf)(const char * format, ...) = printf;
#else
static void hprintf(const char * format, ...)  __attribute__ ((unused));

static void hprintf(const char * format, ...){
	static uint8_t* hprintf_buffer = NULL; 

	va_list args;
	va_start(args, format);
	if(alloc_hprintf_buffer(&hprintf_buffer)){
		vsnprintf((char*)hprintf_buffer, HPRINTF_MAX_SIZE, format, args);
		kAFL_hypercall(HYPERCALL_KAFL_PRINTF, (uintptr_t)hprintf_buffer);
	}
	//vprintf(format, args);
	va_end(args);
}
#endif

static void habort(char* msg){
	kAFL_hypercall(HYPERCALL_KAFL_USER_ABORT, (uintptr_t)msg);
}

#define NYX_HOST_MAGIC  0x4878794e
#define NYX_AGENT_MAGIC 0x4178794e

#define NYX_HOST_VERSION 2 
#define NYX_AGENT_VERSION 1

typedef struct host_config_s{
  uint32_t host_magic;
  uint32_t host_version;
  uint32_t bitmap_size;
  uint32_t ijon_bitmap_size;
  uint32_t payload_buffer_size;
  uint32_t worker_id;
  /* more to come */
} __attribute__((packed)) host_config_t;

typedef struct agent_config_s{
	uint32_t agent_magic;
	uint32_t agent_version;
	uint8_t agent_timeout_detection;
	uint8_t agent_tracing;
	uint8_t agent_ijon_tracing;
	uint8_t agent_non_reload_mode;
	uint64_t trace_buffer_vaddr;
	uint64_t ijon_trace_buffer_vaddr;
	uint32_t coverage_bitmap_size;
	uint32_t input_buffer_size;		// TODO: remove this later

  	uint8_t dump_payloads; /* set by hypervisor */
  /* more to come */
} __attribute__((packed)) agent_config_t;

typedef struct kafl_dump_file_s{
  uint64_t file_name_str_ptr;
  uint64_t data_ptr;
  uint64_t bytes;
  uint8_t append;
} __attribute__((packed)) kafl_dump_file_t;


enum nyx_cpu_type{
	unkown = 0, 
	nyx_cpu_v1,	/* Nyx CPU used by KVM-PT */
	nyx_cpu_v2  /* Nyx CPU used by vanilla KVM + VMWare backdoor */
};

#define cpuid(in,a,b,c,d)\
  asm("cpuid": "=a" (a), "=b" (b), "=c" (c), "=d" (d) : "a" (in));
	 
static int is_nyx_vcpu(void){
  unsigned long eax,ebx,ecx,edx;
  char str[8];
  cpuid(0x80000004,eax,ebx,ecx,edx);	

  for(int j=0;j<4;j++){
    str[j] = eax >> (8*j);
    str[j+4] = ebx >> (8*j);
  }

  return !memcmp(&str, "NYX vCPU", 8);
}

static int get_nyx_cpu_type(void){
	unsigned long eax,ebx,ecx,edx;
  char str[9];
  cpuid(0x80000004,eax,ebx,ecx,edx);	

  for(int j=0;j<4;j++){
    str[j] = eax >> (8*j);
    str[j+4] = ebx >> (8*j);
  }

	if(memcmp(&str, "NYX vCPU", 8) != 0){
		return unkown;
	}

  for(int j=0;j<4;j++){
    str[j] = ecx >> (8*j);
    str[j+4] = edx >> (8*j);
  }

	if(memcmp(&str, " (NO-PT)", 8) != 0){
		return nyx_cpu_v1;
	}

	return nyx_cpu_v2;

	str[8] = 0;
	printf("ECX: %s\n", str);
}

typedef struct req_data_bulk_s{
	char file_name[256];
	uint64_t num_addresses;
	uint64_t addresses[479];
} req_data_bulk_t;

#endif
