/*
 * windows backend for libusbx 1.0
 * Copyright © 2009-2012 Pete Batard <pete@akeo.ie>
 * With contributions from Michael Plante, Orin Eman et al.
 * Parts of this code adapted from libusb-win32-v1 by Stephan Meyer
 * HID Reports IOCTLs inspired from HIDAPI by Alan Ott, Signal 11 Software
 * Hash table functions adapted from glibc, by Ulrich Drepper et al.
 * Major code testing contribution by Xiaofan Chen
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#define INITGUID
#include <config.h>
#include <windows.h>
#include <cfgmgr32.h>
#include <setupapi.h>
#include <usbioctl.h>
#include <winusb.h>
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <process.h>
#include <stdio.h>
#include <inttypes.h>
#include <objbase.h>
#include <winioctl.h>

#include "libusbi.h"
#include "poll_windows.h"
#include "windows_usb.h"

// The 2 macros below are used in conjunction with safe loops.
#define LOOP_CHECK(fcall) { r=fcall; if (r != LIBUSB_SUCCESS) continue; }
#define LOOP_BREAK(err) { r=err; continue; }

// Helper prototypes
static int windows_get_active_config_descriptor(struct libusb_device *dev, unsigned char *buffer, size_t len, int *host_endian);
static int windows_clock_gettime(int clk_id, struct timespec *tp);
unsigned __stdcall windows_clock_gettime_threaded(void* param);
// Common calls
static int common_configure_endpoints(int sub_api, struct libusb_device_handle *dev_handle, int iface);

// WinUSB-like API prototypes
static int winusbx_init(int sub_api, struct libusb_context *ctx);
static int winusbx_exit(int sub_api);
static int winusbx_open(int sub_api, struct libusb_device_handle *dev_handle);
static void winusbx_close(int sub_api, struct libusb_device_handle *dev_handle);
static int winusbx_configure_endpoints(int sub_api, struct libusb_device_handle *dev_handle, int iface);
static int winusbx_claim_interface(int sub_api, struct libusb_device_handle *dev_handle, int iface);
static int winusbx_release_interface(int sub_api, struct libusb_device_handle *dev_handle, int iface);
static int winusbx_submit_control_transfer(int sub_api, struct usbi_transfer *itransfer);
static int winusbx_set_interface_altsetting(int sub_api, struct libusb_device_handle *dev_handle, int iface, int altsetting);
static int winusbx_submit_bulk_transfer(int sub_api, struct usbi_transfer *itransfer);
static int winusbx_clear_halt(int sub_api, struct libusb_device_handle *dev_handle, unsigned char endpoint);
static int winusbx_abort_transfers(int sub_api, struct usbi_transfer *itransfer);
static int winusbx_abort_control(int sub_api, struct usbi_transfer *itransfer);
static int winusbx_reset_device(int sub_api, struct libusb_device_handle *dev_handle);
static int winusbx_copy_transfer_data(int sub_api, struct usbi_transfer *itransfer, uint32_t io_size);
// Composite API prototypes
static int composite_init(int sub_api, struct libusb_context *ctx);
static int composite_exit(int sub_api);
static int composite_open(int sub_api, struct libusb_device_handle *dev_handle);
static void composite_close(int sub_api, struct libusb_device_handle *dev_handle);
static int composite_claim_interface(int sub_api, struct libusb_device_handle *dev_handle, int iface);
static int composite_set_interface_altsetting(int sub_api, struct libusb_device_handle *dev_handle, int iface, int altsetting);
static int composite_release_interface(int sub_api, struct libusb_device_handle *dev_handle, int iface);
static int composite_submit_control_transfer(int sub_api, struct usbi_transfer *itransfer);
static int composite_submit_bulk_transfer(int sub_api, struct usbi_transfer *itransfer);
static int composite_submit_iso_transfer(int sub_api, struct usbi_transfer *itransfer);
static int composite_clear_halt(int sub_api, struct libusb_device_handle *dev_handle, unsigned char endpoint);
static int composite_abort_transfers(int sub_api, struct usbi_transfer *itransfer);
static int composite_abort_control(int sub_api, struct usbi_transfer *itransfer);
static int composite_reset_device(int sub_api, struct libusb_device_handle *dev_handle);
static int composite_copy_transfer_data(int sub_api, struct usbi_transfer *itransfer, uint32_t io_size);


// Global variables
uint64_t hires_frequency, hires_ticks_to_ps;
const uint64_t epoch_time = UINT64_C(116444736000000000);	// 1970.01.01 00:00:000 in MS Filetime
enum windows_version windows_version = WINDOWS_UNSUPPORTED;
// Concurrency
static int concurrent_usage = -1;
usbi_mutex_t autoclaim_lock;
// Timer thread
// NB: index 0 is for monotonic and 1 is for the thread exit event
HANDLE timer_thread = NULL;
HANDLE timer_mutex = NULL;
struct timespec timer_tp;
volatile LONG request_count[2] = {0, 1};	// last one must be > 0
HANDLE timer_request[2] = { NULL, NULL };
HANDLE timer_response = NULL;
// API globals
const char* sub_api_name[SUB_API_MAX] = WINUSBX_DRV_NAMES;

static inline BOOLEAN guid_eq(const GUID *guid1, const GUID *guid2) {
	if ((guid1 != NULL) && (guid2 != NULL)) {
		return (memcmp(guid1, guid2, sizeof(GUID)) == 0);
	}
	return false;
}

#if defined(ENABLE_LOGGING)
static char* guid_to_string(const GUID* guid)
{
	static char guid_string[MAX_GUID_STRING_LENGTH];

	if (guid == NULL) return NULL;
	sprintf(guid_string, "{%08X-%04X-%04X-%02X%02X-%02X%02X%02X%02X%02X%02X}",
		(unsigned int)guid->Data1, guid->Data2, guid->Data3,
		guid->Data4[0], guid->Data4[1], guid->Data4[2], guid->Data4[3],
		guid->Data4[4], guid->Data4[5], guid->Data4[6], guid->Data4[7]);
	return guid_string;
}
#endif

/*
 * Converts a windows error to human readable string
 * uses retval as errorcode, or, if 0, use GetLastError()
 */
#if defined(ENABLE_LOGGING)
static char *windows_error_str(uint32_t retval)
{
static char err_string[ERR_BUFFER_SIZE];

	DWORD size;
	ssize_t i;
	uint32_t error_code, format_error;

	error_code = retval?retval:GetLastError();

	safe_sprintf(err_string, ERR_BUFFER_SIZE, "[%u] ", error_code);

	size = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, NULL, error_code,
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), &err_string[safe_strlen(err_string)],
		ERR_BUFFER_SIZE - (DWORD)safe_strlen(err_string), NULL);
	if (size == 0) {
		format_error = GetLastError();
		if (format_error)
			safe_sprintf(err_string, ERR_BUFFER_SIZE,
				"Windows error code %u (FormatMessage error code %u)", error_code, format_error);
		else
			safe_sprintf(err_string, ERR_BUFFER_SIZE, "Unknown error code %u", error_code);
	} else {
		// Remove CR/LF terminators
		for (i=safe_strlen(err_string)-1; (i>=0) && ((err_string[i]==0x0A) || (err_string[i]==0x0D)); i--) {
			err_string[i] = 0;
		}
	}
	return err_string;
}
#endif

/*
 * Sanitize Microsoft's paths: convert to uppercase, add prefix and fix backslashes.
 * Return an allocated sanitized string or NULL on error.
 */
static char* sanitize_path(const char* path)
{
	const char root_prefix[] = "\\\\.\\";
	size_t j, size, root_size;
	char* ret_path = NULL;
	size_t add_root = 0;

	if (path == NULL)
		return NULL;

	size = safe_strlen(path)+1;
	root_size = sizeof(root_prefix)-1;

	// Microsoft indiscriminatly uses '\\?\', '\\.\', '##?#" or "##.#" for root prefixes.
	if (!((size > 3) && (((path[0] == '\\') && (path[1] == '\\') && (path[3] == '\\')) ||
		((path[0] == '#') && (path[1] == '#') && (path[3] == '#'))))) {
		add_root = root_size;
		size += add_root;
	}

	if ((ret_path = (char*) calloc(size, 1)) == NULL)
		return NULL;

	safe_strcpy(&ret_path[add_root], size-add_root, path);

	// Ensure consistancy with root prefix
	for (j=0; j<root_size; j++)
		ret_path[j] = root_prefix[j];

	// Same goes for '\' and '#' after the root prefix. Ensure '#' is used
	for(j=root_size; j<size; j++) {
		ret_path[j] = (char)toupper((int)ret_path[j]);	// Fix case too
		if (ret_path[j] == '\\')
			ret_path[j] = '#';
	}

	return ret_path;
}

/*
 * enumerate interfaces for the whole USB class
 *
 * Parameters:
 * dev_info: a pointer to a dev_info list
 * dev_info_data: a pointer to an SP_DEVINFO_DATA to be filled (or NULL if not needed)
 * usb_class: the generic USB class for which to retrieve interface details
 * index: zero based index of the interface in the device info list
 *
 * Note: it is the responsibility of the caller to free the DEVICE_INTERFACE_DETAIL_DATA
 * structure returned and call this function repeatedly using the same guid (with an
 * incremented index starting at zero) until all interfaces have been returned.
 */
static bool get_devinfo_data(struct libusb_context *ctx,
	HDEVINFO *dev_info, SP_DEVINFO_DATA *dev_info_data, const char* usb_class, unsigned _index)
{
	if (_index <= 0) {
		*dev_info = SetupDiGetClassDevsA(NULL, usb_class, NULL, DIGCF_PRESENT|DIGCF_ALLCLASSES);
		if (*dev_info == INVALID_HANDLE_VALUE) {
			return false;
		}
	}

	dev_info_data->cbSize = sizeof(SP_DEVINFO_DATA);
	if (!SetupDiEnumDeviceInfo(*dev_info, _index, dev_info_data)) {
		if (GetLastError() != ERROR_NO_MORE_ITEMS) {
			usbi_err(ctx, "Could not obtain device info data for index %u: %s",
				_index, windows_error_str(0));
		}
		SetupDiDestroyDeviceInfoList(*dev_info);
		*dev_info = INVALID_HANDLE_VALUE;
		return false;
	}
	return true;
}

/*
 * enumerate interfaces for a specific GUID
 *
 * Parameters:
 * dev_info: a pointer to a dev_info list
 * dev_info_data: a pointer to an SP_DEVINFO_DATA to be filled (or NULL if not needed)
 * guid: the GUID for which to retrieve interface details
 * index: zero based index of the interface in the device info list
 *
 * Note: it is the responsibility of the caller to free the DEVICE_INTERFACE_DETAIL_DATA
 * structure returned and call this function repeatedly using the same guid (with an
 * incremented index starting at zero) until all interfaces have been returned.
 */
static SP_DEVICE_INTERFACE_DETAIL_DATA_A *get_interface_details(struct libusb_context *ctx,
	HDEVINFO *dev_info, SP_DEVINFO_DATA *dev_info_data, const GUID* guid, unsigned _index)
{
	SP_DEVICE_INTERFACE_DATA dev_interface_data;
	SP_DEVICE_INTERFACE_DETAIL_DATA_A *dev_interface_details = NULL;
	DWORD size;

	if (_index <= 0) {
		*dev_info = SetupDiGetClassDevsA(guid, NULL, NULL, DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
	}

	if (dev_info_data != NULL) {
		dev_info_data->cbSize = sizeof(SP_DEVINFO_DATA);
		if (!SetupDiEnumDeviceInfo(*dev_info, _index, dev_info_data)) {
			if (GetLastError() != ERROR_NO_MORE_ITEMS) {
				usbi_err(ctx, "Could not obtain device info data for index %u: %s",
					_index, windows_error_str(0));
			}
			SetupDiDestroyDeviceInfoList(*dev_info);
			*dev_info = INVALID_HANDLE_VALUE;
			return NULL;
		}
	}

	dev_interface_data.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
	if (!SetupDiEnumDeviceInterfaces(*dev_info, NULL, guid, _index, &dev_interface_data)) {
		if (GetLastError() != ERROR_NO_MORE_ITEMS) {
			usbi_err(ctx, "Could not obtain interface data for index %u: %s",
				_index, windows_error_str(0));
		}
		SetupDiDestroyDeviceInfoList(*dev_info);
		*dev_info = INVALID_HANDLE_VALUE;
		return NULL;
	}

	// Read interface data (dummy + actual) to access the device path
	if (!SetupDiGetDeviceInterfaceDetailA(*dev_info, &dev_interface_data, NULL, 0, &size, NULL)) {
		// The dummy call should fail with ERROR_INSUFFICIENT_BUFFER
		if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
			usbi_err(ctx, "could not access interface data (dummy) for index %u: %s",
				_index, windows_error_str(0));
			goto err_exit;
		}
	} else {
		usbi_err(ctx, "program assertion failed - http://msdn.microsoft.com/en-us/library/ms792901.aspx is wrong.");
		goto err_exit;
	}

	if ((dev_interface_details = (SP_DEVICE_INTERFACE_DETAIL_DATA_A*) calloc(size, 1)) == NULL) {
		usbi_err(ctx, "could not allocate interface data for index %u.", _index);
		goto err_exit;
	}

	dev_interface_details->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);
	if (!SetupDiGetDeviceInterfaceDetailA(*dev_info, &dev_interface_data,
		dev_interface_details, size, &size, NULL)) {
		usbi_err(ctx, "could not access interface data (actual) for index %u: %s",
			_index, windows_error_str(0));
	}

	return dev_interface_details;

err_exit:
	SetupDiDestroyDeviceInfoList(*dev_info);
	*dev_info = INVALID_HANDLE_VALUE;
	return NULL;
}

/* For libusb0 filter */
static SP_DEVICE_INTERFACE_DETAIL_DATA_A *get_interface_details_filter(struct libusb_context *ctx,
	HDEVINFO *dev_info, SP_DEVINFO_DATA *dev_info_data, const GUID* guid, unsigned _index, char* filter_path){
	SP_DEVICE_INTERFACE_DATA dev_interface_data;
	SP_DEVICE_INTERFACE_DETAIL_DATA_A *dev_interface_details = NULL;
	DWORD size;
	if (_index <= 0) {
		*dev_info = SetupDiGetClassDevsA(guid, NULL, NULL, DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);
	}
	if (dev_info_data != NULL) {
		dev_info_data->cbSize = sizeof(SP_DEVINFO_DATA);
		if (!SetupDiEnumDeviceInfo(*dev_info, _index, dev_info_data)) {
			if (GetLastError() != ERROR_NO_MORE_ITEMS) {
				usbi_err(ctx, "Could not obtain device info data for index %u: %s",
					_index, windows_error_str(0));
			}
			SetupDiDestroyDeviceInfoList(*dev_info);
			*dev_info = INVALID_HANDLE_VALUE;
			return NULL;
		}
	}
	dev_interface_data.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
	if (!SetupDiEnumDeviceInterfaces(*dev_info, NULL, guid, _index, &dev_interface_data)) {
		if (GetLastError() != ERROR_NO_MORE_ITEMS) {
			usbi_err(ctx, "Could not obtain interface data for index %u: %s",
				_index, windows_error_str(0));
		}
		SetupDiDestroyDeviceInfoList(*dev_info);
		*dev_info = INVALID_HANDLE_VALUE;
		return NULL;
	}
	// Read interface data (dummy + actual) to access the device path
	if (!SetupDiGetDeviceInterfaceDetailA(*dev_info, &dev_interface_data, NULL, 0, &size, NULL)) {
		// The dummy call should fail with ERROR_INSUFFICIENT_BUFFER
		if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
			usbi_err(ctx, "could not access interface data (dummy) for index %u: %s",
				_index, windows_error_str(0));
			goto err_exit;
		}
	} else {
		usbi_err(ctx, "program assertion failed - http://msdn.microsoft.com/en-us/library/ms792901.aspx is wrong.");
		goto err_exit;
	}
	if ((dev_interface_details = malloc(size)) == NULL) {
		usbi_err(ctx, "could not allocate interface data for index %u.", _index);
		goto err_exit;
	}
	dev_interface_details->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_A);
	if (!SetupDiGetDeviceInterfaceDetailA(*dev_info, &dev_interface_data,
		dev_interface_details, size, &size, NULL)) {
		usbi_err(ctx, "could not access interface data (actual) for index %u: %s",
			_index, windows_error_str(0));
	}
	// [trobinso] lookup the libusb0 symbolic index.
	if (dev_interface_details) {
		HKEY hkey_device_interface=SetupDiOpenDeviceInterfaceRegKey(*dev_info,&dev_interface_data,0,KEY_READ);
		if (hkey_device_interface != INVALID_HANDLE_VALUE) {
			DWORD libusb0_symboliclink_index=0;
			DWORD value_length=sizeof(DWORD);
			DWORD value_type=0;
			LONG status;
			status = RegQueryValueExW(hkey_device_interface, L"LUsb0", NULL, &value_type,
				(LPBYTE) &libusb0_symboliclink_index, &value_length);
			if (status == ERROR_SUCCESS) {
				if (libusb0_symboliclink_index < 256) {
					// libusb0.sys is connected to this device instance.
					// If the the device interface guid is {F9F3FF14-AE21-48A0-8A25-8011A7A931D9} then it's a filter.
					safe_sprintf(filter_path, sizeof("\\\\.\\libusb0-0000"), "\\\\.\\libusb0-%04d", libusb0_symboliclink_index);
					usbi_dbg("assigned libusb0 symbolic link %s", filter_path);
				} else {
					// libusb0.sys was connected to this device instance at one time; but not anymore.
				}
			}
			RegCloseKey(hkey_device_interface);
		}
	}
	return dev_interface_details;
err_exit:
	SetupDiDestroyDeviceInfoList(*dev_info);
	*dev_info = INVALID_HANDLE_VALUE;
	return NULL;}

/* Hash table functions - modified From glibc 2.3.2:
   [Aho,Sethi,Ullman] Compilers: Principles, Techniques and Tools, 1986
   [Knuth]            The Art of Computer Programming, part 3 (6.4)  */
typedef struct htab_entry {
	unsigned long used;
	char* str;
} htab_entry;
htab_entry* htab_table = NULL;
usbi_mutex_t htab_write_mutex = NULL;
unsigned long htab_size, htab_filled;

/* For the used double hash method the table size has to be a prime. To
   correct the user given table size we need a prime test.  This trivial
   algorithm is adequate because the code is called only during init and
   the number is likely to be small  */
static int isprime(unsigned long number)
{
	// no even number will be passed
	unsigned int divider = 3;

	while((divider * divider < number) && (number % divider != 0))
		divider += 2;

	return (number % divider != 0);
}

/* Before using the hash table we must allocate memory for it.
   We allocate one element more as the found prime number says.
   This is done for more effective indexing as explained in the
   comment for the hash function.  */
static int htab_create(struct libusb_context *ctx, unsigned long nel)
{
	if (htab_table != NULL) {
		usbi_err(ctx, "hash table already allocated");
	}

	// Create a mutex
	usbi_mutex_init(&htab_write_mutex, NULL);

	// Change nel to the first prime number not smaller as nel.
	nel |= 1;
	while(!isprime(nel))
		nel += 2;

	htab_size = nel;
	usbi_dbg("using %d entries hash table", nel);
	htab_filled = 0;

	// allocate memory and zero out.
	htab_table = (htab_entry*) calloc(htab_size + 1, sizeof(htab_entry));
	if (htab_table == NULL) {
		usbi_err(ctx, "could not allocate space for hash table");
		return 0;
	}

	return 1;
}

/* After using the hash table it has to be destroyed.  */
static void htab_destroy(void)
{
	size_t i;
	if (htab_table == NULL) {
		return;
	}

	for (i=0; i<htab_size; i++) {
		if (htab_table[i].used) {
			safe_free(htab_table[i].str);
		}
	}
	usbi_mutex_destroy(&htab_write_mutex);
	safe_free(htab_table);
}

/* This is the search function. It uses double hashing with open addressing.
   We use an trick to speed up the lookup. The table is created with one
   more element available. This enables us to use the index zero special.
   This index will never be used because we store the first hash index in
   the field used where zero means not used. Every other value means used.
   The used field can be used as a first fast comparison for equality of
   the stored and the parameter value. This helps to prevent unnecessary
   expensive calls of strcmp.  */
static unsigned long htab_hash(char* str)
{
	unsigned long hval, hval2;
	unsigned long idx;
	unsigned long r = 5381;
	int c;
	char* sz = str;

	if (str == NULL)
		return 0;

	// Compute main hash value (algorithm suggested by Nokia)
	while ((c = *sz++) != 0)
		r = ((r << 5) + r) + c;
	if (r == 0)
		++r;

	// compute table hash: simply take the modulus
	hval = r % htab_size;
	if (hval == 0)
		++hval;

	// Try the first index
	idx = hval;

	if (htab_table[idx].used) {
		if ( (htab_table[idx].used == hval)
		  && (safe_strcmp(str, htab_table[idx].str) == 0) ) {
			// existing hash
			return idx;
		}
		usbi_dbg("hash collision ('%s' vs '%s')", str, htab_table[idx].str);

		// Second hash function, as suggested in [Knuth]
		hval2 = 1 + hval % (htab_size - 2);

		do {
			// Because size is prime this guarantees to step through all available indexes
			if (idx <= hval2) {
				idx = htab_size + idx - hval2;
			} else {
				idx -= hval2;
			}

			// If we visited all entries leave the loop unsuccessfully
			if (idx == hval) {
				break;
			}

			// If entry is found use it.
			if ( (htab_table[idx].used == hval)
			  && (safe_strcmp(str, htab_table[idx].str) == 0) ) {
				return idx;
			}
		}
		while (htab_table[idx].used);
	}

	// Not found => New entry

	// If the table is full return an error
	if (htab_filled >= htab_size) {
		usbi_err(NULL, "hash table is full (%d entries)", htab_size);
		return 0;
	}

	// Concurrent threads might be storing the same entry at the same time
	// (eg. "simultaneous" enums from different threads) => use a mutex
	usbi_mutex_lock(&htab_write_mutex);
	// Just free any previously allocated string (which should be the same as
	// new one). The possibility of concurrent threads storing a collision
	// string (same hash, different string) at the same time is extremely low
	safe_free(htab_table[idx].str);
	htab_table[idx].used = hval;
	htab_table[idx].str = (char*) malloc(safe_strlen(str)+1);
	if (htab_table[idx].str == NULL) {
		usbi_err(NULL, "could not duplicate string for hash table");
		usbi_mutex_unlock(&htab_write_mutex);
		return 0;
	}
	memcpy(htab_table[idx].str, str, safe_strlen(str)+1);
	++htab_filled;
	usbi_mutex_unlock(&htab_write_mutex);

	return idx;
}

/*
 * Returns the session ID of a device's nth level ancestor
 * If there's no device at the nth level, return 0
 */
static unsigned long get_ancestor_session_id(DWORD devinst, unsigned level)
{
	DWORD parent_devinst;
	unsigned long session_id = 0;
	char* sanitized_path = NULL;
	char path[MAX_PATH_LENGTH];
	unsigned i;

	if (level < 1) return 0;
	for (i = 0; i<level; i++) {
		if (CM_Get_Parent(&parent_devinst, devinst, 0) != CR_SUCCESS) {
			return 0;
		}
		devinst = parent_devinst;
	}
	if (CM_Get_Device_IDA(devinst, path, MAX_PATH_LENGTH, 0) != CR_SUCCESS) {
		return 0;
	}
	// TODO: (post hotplug): try without sanitizing
	sanitized_path = sanitize_path(path);
	if (sanitized_path == NULL) {
		return 0;
	}
	session_id = htab_hash(sanitized_path);
	safe_free(sanitized_path);
	return session_id;
}

/*
 * Populate the endpoints addresses of the device_priv interface helper structs
 */
static int windows_assign_endpoints(struct libusb_device_handle *dev_handle, int iface, int altsetting)
{
	int i, r;
	struct windows_device_priv *priv = _device_priv(dev_handle->dev);
	struct libusb_config_descriptor *conf_desc;
	const struct libusb_interface_descriptor *if_desc;
	struct libusb_context *ctx = DEVICE_CTX(dev_handle->dev);

	r = libusb_get_config_descriptor(dev_handle->dev, 0, &conf_desc);
	if (r != LIBUSB_SUCCESS) {
		usbi_warn(ctx, "could not read config descriptor: error %d", r);
		return r;
	}

	if (iface >= conf_desc->bNumInterfaces ||
	    altsetting >= conf_desc->interface[iface].num_altsetting) {
		usbi_dbg("interface %d, altsetting %d out of range", iface, altsetting);
		return LIBUSB_ERROR_INVALID_PARAM;
	}

	if_desc = &conf_desc->interface[iface].altsetting[altsetting];
	safe_free(priv->usb_interface[iface].endpoint);

	if (if_desc->bNumEndpoints == 0) {
		usbi_dbg("no endpoints found for interface %d", iface);
		return LIBUSB_SUCCESS;
	}

	priv->usb_interface[iface].endpoint = (uint8_t*) malloc(if_desc->bNumEndpoints);
	if (priv->usb_interface[iface].endpoint == NULL) {
		return LIBUSB_ERROR_NO_MEM;
	}

	priv->usb_interface[iface].nb_endpoints = if_desc->bNumEndpoints;
	for (i=0; i<if_desc->bNumEndpoints; i++) {
		priv->usb_interface[iface].endpoint[i] = if_desc->endpoint[i].bEndpointAddress;
		usbi_dbg("(re)assigned endpoint %02X to interface %d", priv->usb_interface[iface].endpoint[i], iface);
	}
	libusb_free_config_descriptor(conf_desc);

	// Extra init may be required to configure endpoints
	return priv->apib->configure_endpoints(SUB_API_NOTSET, dev_handle, iface);
}

// Lookup for a match in the list of API driver names
// return -1 if not found, driver match number otherwise
static int get_sub_api(char* driver, int api){
	int i;
	const char sep_str[2] = {LIST_SEPARATOR, 0};
	char *tok, *tmp_str;
	size_t len = safe_strlen(driver);

	if (len == 0) return SUB_API_NOTSET;
	tmp_str = (char*) calloc(len+1, 1);
	if (tmp_str == NULL) return SUB_API_NOTSET;
	memcpy(tmp_str, driver, len+1);
	tok = strtok(tmp_str, sep_str);
	while (tok != NULL) {
		for (i=0; i<usb_api_backend[api].nb_driver_names; i++) {
			if (safe_stricmp(tok, usb_api_backend[api].driver_name_list[i]) == 0) {
				free(tmp_str);
				return i;
			}
		}
		tok = strtok(NULL, sep_str);
	}
	free (tmp_str);
	return SUB_API_NOTSET;
}

/*
 * auto-claiming and auto-release helper functions
 */
static int auto_claim(struct libusb_transfer *transfer, int *interface_number, int api_type)
{
	struct libusb_context *ctx = DEVICE_CTX(transfer->dev_handle->dev);
	struct windows_device_handle_priv *handle_priv = _device_handle_priv(
		transfer->dev_handle);
	struct windows_device_priv *priv = _device_priv(transfer->dev_handle->dev);
	int current_interface = *interface_number;
	int r = LIBUSB_SUCCESS;

	switch(api_type) {
	case USB_API_WINUSBX:
		break;
	default:
		return LIBUSB_ERROR_INVALID_PARAM;
	}

	usbi_mutex_lock(&autoclaim_lock);
	if (current_interface < 0)	// No serviceable interface was found
	{
		for (current_interface=0; current_interface<USB_MAXINTERFACES; current_interface++) {
			// Must claim an interface of the same API type
			if ( (priv->usb_interface[current_interface].apib->id == api_type)
			  && (libusb_claim_interface(transfer->dev_handle, current_interface) == LIBUSB_SUCCESS) ) {
				usbi_dbg("auto-claimed interface %d for control request", current_interface);
				if (handle_priv->autoclaim_count[current_interface] != 0) {
					usbi_warn(ctx, "program assertion failed - autoclaim_count was nonzero");
				}
				handle_priv->autoclaim_count[current_interface]++;
				break;
			}
		}
		if (current_interface == USB_MAXINTERFACES) {
			usbi_err(ctx, "could not auto-claim any interface");
			r = LIBUSB_ERROR_NOT_FOUND;
		}
	} else {
		// If we have a valid interface that was autoclaimed, we must increment
		// its autoclaim count so that we can prevent an early release.
		if (handle_priv->autoclaim_count[current_interface] != 0) {
			handle_priv->autoclaim_count[current_interface]++;
		}
	}
	usbi_mutex_unlock(&autoclaim_lock);

	*interface_number = current_interface;
	return r;

}

static void auto_release(struct usbi_transfer *itransfer)
{
	struct windows_transfer_priv *transfer_priv = (struct windows_transfer_priv*)usbi_transfer_get_os_priv(itransfer);
	struct libusb_transfer *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
	libusb_device_handle *dev_handle = transfer->dev_handle;
	struct windows_device_handle_priv* handle_priv = _device_handle_priv(dev_handle);
	int r;

	usbi_mutex_lock(&autoclaim_lock);
	if (handle_priv->autoclaim_count[transfer_priv->interface_number] > 0) {
		handle_priv->autoclaim_count[transfer_priv->interface_number]--;
		if (handle_priv->autoclaim_count[transfer_priv->interface_number] == 0) {
			r = libusb_release_interface(dev_handle, transfer_priv->interface_number);
			if (r == LIBUSB_SUCCESS) {
				usbi_dbg("auto-released interface %d", transfer_priv->interface_number);
			} else {
				usbi_dbg("failed to auto-release interface %d (%s)",
					transfer_priv->interface_number, libusb_error_name((enum libusb_error)r));
			}
		}
	}
	usbi_mutex_unlock(&autoclaim_lock);
}

/*
 * init: libusbx backend init function
 *
 * This function enumerates the HCDs (Host Controller Drivers) and populates our private HCD list
 * In our implementation, we equate Windows' "HCD" to libusbx's "bus". Note that bus is zero indexed.
 * HCDs are not expected to change after init (might not hold true for hot pluggable USB PCI card?)
 */
static int windows_init(struct libusb_context *ctx)
{
	int i, r = LIBUSB_ERROR_OTHER;
	OSVERSIONINFO os_version;
	HANDLE semaphore;
	char sem_name[11+1+8]; // strlen(libusb_init)+'\0'+(32-bit hex PID)

	sprintf(sem_name, "libusb_init%08X", (unsigned int)GetCurrentProcessId()&0xFFFFFFFF);
	semaphore = CreateSemaphoreA(NULL, 1, 1, sem_name);
	if (semaphore == NULL) {
		usbi_err(ctx, "could not create semaphore: %s", windows_error_str(0));
		return LIBUSB_ERROR_NO_MEM;
	}

	// A successful wait brings our semaphore count to 0 (unsignaled)
	// => any concurent wait stalls until the semaphore's release
	if (WaitForSingleObject(semaphore, INFINITE) != WAIT_OBJECT_0) {
		usbi_err(ctx, "failure to access semaphore: %s", windows_error_str(0));
		CloseHandle(semaphore);
		return LIBUSB_ERROR_NO_MEM;
	}

	// NB: concurrent usage supposes that init calls are equally balanced with
	// exit calls. If init is called more than exit, we will not exit properly
	if ( ++concurrent_usage == 0 ) {	// First init?
		// Detect OS version
		memset(&os_version, 0, sizeof(OSVERSIONINFO));
		os_version.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
		windows_version = WINDOWS_UNSUPPORTED;
		if ((GetVersionEx(&os_version) != 0) && (os_version.dwPlatformId == VER_PLATFORM_WIN32_NT)) {
			if ((os_version.dwMajorVersion == 5) && (os_version.dwMinorVersion == 1)) {
				windows_version = WINDOWS_XP;
			} else if ((os_version.dwMajorVersion == 5) && (os_version.dwMinorVersion == 2)) {
				windows_version = WINDOWS_2003;	// also includes XP 64
			} else if (os_version.dwMajorVersion >= 6) {
				windows_version = WINDOWS_VISTA_AND_LATER;
			}
		}
		if (windows_version == WINDOWS_UNSUPPORTED) {
			usbi_err(ctx, "This version of Windows is NOT supported");
			r = LIBUSB_ERROR_NOT_SUPPORTED;
			goto init_exit;
		}

		// We need a lock for proper auto-release
		usbi_mutex_init(&autoclaim_lock, NULL);

		// Initialize pollable file descriptors
		init_polling();

		// Initialize the low level APIs (we don't care about errors at this stage)
		for (i=0; i<USB_API_MAX; i++) {
			usb_api_backend[i].init(SUB_API_NOTSET, ctx);
		}

		// Because QueryPerformanceCounter might report different values when
		// running on different cores, we create a separate thread for the timer
		// calls, which we glue to the first core always to prevent timing discrepancies.
		r = LIBUSB_ERROR_NO_MEM;
		for (i = 0; i < 2; i++) {
			timer_request[i] = CreateEvent(NULL, TRUE, FALSE, NULL);
			if (timer_request[i] == NULL) {
				usbi_err(ctx, "could not create timer request event %d - aborting", i);
				goto init_exit;
			}
		}
		timer_response = CreateSemaphore(NULL, 0, MAX_TIMER_SEMAPHORES, NULL);
		if (timer_response == NULL) {
			usbi_err(ctx, "could not create timer response semaphore - aborting");
			goto init_exit;
		}
		timer_mutex = CreateMutex(NULL, FALSE, NULL);
		if (timer_mutex == NULL) {
			usbi_err(ctx, "could not create timer mutex - aborting");
			goto init_exit;
		}
		timer_thread = (HANDLE)_beginthreadex(NULL, 0, windows_clock_gettime_threaded, NULL, 0, NULL);
		if (timer_thread == NULL) {
			usbi_err(ctx, "Unable to create timer thread - aborting");
			goto init_exit;
		}
		SetThreadAffinityMask(timer_thread, 0);

		// Wait for timer thread to init before continuing.
		if (WaitForSingleObject(timer_response, INFINITE) != WAIT_OBJECT_0) {
			usbi_err(ctx, "Failed to wait for timer thread to become ready - aborting");
			goto init_exit;
		}

		// Create a hash table to store session ids. Second parameter is better if prime
		htab_create(ctx, HTAB_SIZE);
	}
	// At this stage, either we went through full init successfully, or didn't need to
	r = LIBUSB_SUCCESS;

init_exit: // Holds semaphore here.
	if (!concurrent_usage && r != LIBUSB_SUCCESS) { // First init failed?
		if (timer_thread) {
			SetEvent(timer_request[1]); // actually the signal to quit the thread.
			if (WAIT_OBJECT_0 != WaitForSingleObject(timer_thread, INFINITE)) {
				usbi_warn(ctx, "could not wait for timer thread to quit");
				TerminateThread(timer_thread, 1); // shouldn't happen, but we're destroying
												  // all objects it might have held anyway.
			}
			CloseHandle(timer_thread);
			timer_thread = NULL;
		}
		for (i = 0; i < 2; i++) {
			if (timer_request[i]) {
				CloseHandle(timer_request[i]);
				timer_request[i] = NULL;
			}
		}
		if (timer_response) {
			CloseHandle(timer_response);
			timer_response = NULL;
		}
		if (timer_mutex) {
			CloseHandle(timer_mutex);
			timer_mutex = NULL;
		}
		htab_destroy();
	}

	if (r != LIBUSB_SUCCESS)
		--concurrent_usage; // Not expected to call libusb_exit if we failed.

	ReleaseSemaphore(semaphore, 1, NULL);	// increase count back to 1
	CloseHandle(semaphore);
	return r;
}

/*
 * HCD (root) hubs need to have their device descriptor manually populated
 *
 * Note that, like Microsoft does in the device manager, we populate the
 * Vendor and Device ID for HCD hubs with the ones from the PCI HCD device.
 */
static int force_hcd_device_descriptor(struct libusb_device *dev)
{
	struct windows_device_priv *parent_priv, *priv = _device_priv(dev);
	struct libusb_context *ctx = DEVICE_CTX(dev);
	int vid, pid;

	dev->num_configurations = 1;
	priv->dev_descriptor.bLength = sizeof(USB_DEVICE_DESCRIPTOR);
	priv->dev_descriptor.bDescriptorType = USB_DEVICE_DESCRIPTOR_TYPE;
	priv->dev_descriptor.bNumConfigurations = 1;
	priv->active_config = 1;

	if (priv->parent_dev == NULL) {
		usbi_err(ctx, "program assertion failed - HCD hub has no parent");
		return LIBUSB_ERROR_NO_DEVICE;
	}
	parent_priv = _device_priv(priv->parent_dev);
	if (sscanf(parent_priv->path, "\\\\.\\PCI#VEN_%04x&DEV_%04x%*s", &vid, &pid) == 2) {
		priv->dev_descriptor.idVendor = (uint16_t)vid;
		priv->dev_descriptor.idProduct = (uint16_t)pid;
	} else {
		usbi_warn(ctx, "could not infer VID/PID of HCD hub from '%s'", parent_priv->path);
		priv->dev_descriptor.idVendor = 0x1d6b;		// Linux Foundation root hub
		priv->dev_descriptor.idProduct = 1;
	}
	return LIBUSB_SUCCESS;
}

/*
 * fetch and cache all the config descriptors through I/O
 */
static int cache_config_descriptors(struct libusb_device *dev, HANDLE hub_handle, char* device_id)
{
	DWORD size, ret_size;
	struct libusb_context *ctx = DEVICE_CTX(dev);
	struct windows_device_priv *priv = _device_priv(dev);
	int r;
	uint8_t i;

	USB_CONFIGURATION_DESCRIPTOR_SHORT cd_buf_short;    // dummy request
	PUSB_DESCRIPTOR_REQUEST cd_buf_actual = NULL;       // actual request
	PUSB_CONFIGURATION_DESCRIPTOR cd_data = NULL;

	if (dev->num_configurations == 0)
		return LIBUSB_ERROR_INVALID_PARAM;

	priv->config_descriptor = (unsigned char**) calloc(dev->num_configurations, sizeof(unsigned char*));
	if (priv->config_descriptor == NULL)
		return LIBUSB_ERROR_NO_MEM;
	for (i=0; i<dev->num_configurations; i++)
		priv->config_descriptor[i] = NULL;

	for (i=0, r=LIBUSB_SUCCESS; ; i++)
	{
		// safe loop: release all dynamic resources
		safe_free(cd_buf_actual);

		// safe loop: end of loop condition
		if ((i >= dev->num_configurations) || (r != LIBUSB_SUCCESS))
			break;

		size = sizeof(USB_CONFIGURATION_DESCRIPTOR_SHORT);
		memset(&cd_buf_short, 0, size);

		cd_buf_short.req.ConnectionIndex = (ULONG)priv->port;
		cd_buf_short.req.SetupPacket.bmRequest = LIBUSB_ENDPOINT_IN;
		cd_buf_short.req.SetupPacket.bRequest = USB_REQUEST_GET_DESCRIPTOR;
		cd_buf_short.req.SetupPacket.wValue = (USB_CONFIGURATION_DESCRIPTOR_TYPE << 8) | i;
		cd_buf_short.req.SetupPacket.wIndex = i;
		cd_buf_short.req.SetupPacket.wLength = (USHORT)(size - sizeof(USB_DESCRIPTOR_REQUEST));

		// Dummy call to get the required data size. Initial failures are reported as info rather
		// than error as they can occur for non-penalizing situations, such as with some hubs.
		if (!DeviceIoControl(hub_handle, IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION, &cd_buf_short, size,
			&cd_buf_short, size, &ret_size, NULL)) {
			usbi_info(ctx, "could not access configuration descriptor (dummy) for '%s': %s", device_id, windows_error_str(0));
			LOOP_BREAK(LIBUSB_ERROR_IO);
		}

		if ((ret_size != size) || (cd_buf_short.data.wTotalLength < sizeof(USB_CONFIGURATION_DESCRIPTOR))) {
			usbi_info(ctx, "unexpected configuration descriptor size (dummy) for '%s'.", device_id);
			LOOP_BREAK(LIBUSB_ERROR_IO);
		}

		size = sizeof(USB_DESCRIPTOR_REQUEST) + cd_buf_short.data.wTotalLength;
		if ((cd_buf_actual = (PUSB_DESCRIPTOR_REQUEST) calloc(1, size)) == NULL) {
			usbi_err(ctx, "could not allocate configuration descriptor buffer for '%s'.", device_id);
			LOOP_BREAK(LIBUSB_ERROR_NO_MEM);
		}
		memset(cd_buf_actual, 0, size);

		// Actual call
		cd_buf_actual->ConnectionIndex = (ULONG)priv->port;
		cd_buf_actual->SetupPacket.bmRequest = LIBUSB_ENDPOINT_IN;
		cd_buf_actual->SetupPacket.bRequest = USB_REQUEST_GET_DESCRIPTOR;
		cd_buf_actual->SetupPacket.wValue = (USB_CONFIGURATION_DESCRIPTOR_TYPE << 8) | i;
		cd_buf_actual->SetupPacket.wIndex = i;
		cd_buf_actual->SetupPacket.wLength = (USHORT)(size - sizeof(USB_DESCRIPTOR_REQUEST));

		if (!DeviceIoControl(hub_handle, IOCTL_USB_GET_DESCRIPTOR_FROM_NODE_CONNECTION, cd_buf_actual, size,
			cd_buf_actual, size, &ret_size, NULL)) {
			usbi_err(ctx, "could not access configuration descriptor (actual) for '%s': %s", device_id, windows_error_str(0));
			LOOP_BREAK(LIBUSB_ERROR_IO);
		}

		cd_data = (PUSB_CONFIGURATION_DESCRIPTOR)((UCHAR*)cd_buf_actual+sizeof(USB_DESCRIPTOR_REQUEST));

		if ((size != ret_size) || (cd_data->wTotalLength != cd_buf_short.data.wTotalLength)) {
			usbi_err(ctx, "unexpected configuration descriptor size (actual) for '%s'.", device_id);
			LOOP_BREAK(LIBUSB_ERROR_IO);
		}

		if (cd_data->bDescriptorType != USB_CONFIGURATION_DESCRIPTOR_TYPE) {
			usbi_err(ctx, "not a configuration descriptor for '%s'", device_id);
			LOOP_BREAK(LIBUSB_ERROR_IO);
		}

		usbi_dbg("cached config descriptor %d (bConfigurationValue=%d, %d bytes)",
			i, cd_data->bConfigurationValue, cd_data->wTotalLength);

		// Cache the descriptor
		priv->config_descriptor[i] = (unsigned char*) malloc(cd_data->wTotalLength);
		if (priv->config_descriptor[i] == NULL)
			return LIBUSB_ERROR_NO_MEM;
		memcpy(priv->config_descriptor[i], cd_data, cd_data->wTotalLength);
	}
	return LIBUSB_SUCCESS;
}

/*
 * Populate a libusbx device structure
 */
static int init_device(struct libusb_device* dev, struct libusb_device* parent_dev,
					   uint8_t port_number, char* device_id, DWORD devinst)
{
	HANDLE handle;
	DWORD size;
	USB_NODE_CONNECTION_INFORMATION_EX conn_info;
	struct windows_device_priv *priv, *parent_priv;
	struct libusb_context *ctx = DEVICE_CTX(dev);
	struct libusb_device* tmp_dev;
	unsigned i;

	if ((dev == NULL) || (parent_dev == NULL)) {
		return LIBUSB_ERROR_NOT_FOUND;
	}
	priv = _device_priv(dev);
	parent_priv = _device_priv(parent_dev);
	if (parent_priv->apib->id != USB_API_HUB) {
		usbi_warn(ctx, "parent for device '%s' is not a hub", device_id);
		return LIBUSB_ERROR_NOT_FOUND;
	}

	// It is possible for the parent hub not to have been initialized yet
	// If that's the case, lookup the ancestors to set the bus number
	if (parent_dev->bus_number == 0) {
		for (i=2; ; i++) {
			tmp_dev = usbi_get_device_by_session_id(ctx, get_ancestor_session_id(devinst, i));
			if (tmp_dev == NULL) break;
			if (tmp_dev->bus_number != 0) {
				usbi_dbg("got bus number from ancestor #%d", i);
				parent_dev->bus_number = tmp_dev->bus_number;
				break;
			}
		}
	}
	if (parent_dev->bus_number == 0) {
		usbi_err(ctx, "program assertion failed: unable to find ancestor bus number for '%s'", device_id);
		return LIBUSB_ERROR_NOT_FOUND;
	}
	dev->bus_number = parent_dev->bus_number;
	priv->port = port_number;
	dev->port_number = port_number;
	priv->depth = parent_priv->depth + 1;
	priv->parent_dev = parent_dev;
	if (dev->parent_dev != parent_dev) {
		safe_unref_device(dev->parent_dev);
		dev->parent_dev = libusb_ref_device(parent_dev);
	}

	// If the device address is already set, we can stop here
	if (dev->device_address != 0) {
		return LIBUSB_SUCCESS;
	}
	memset(&conn_info, 0, sizeof(conn_info));
	if (priv->depth != 0) {	// Not a HCD hub
		handle = CreateFileA(parent_priv->path, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
			FILE_FLAG_OVERLAPPED, NULL);
		if (handle == INVALID_HANDLE_VALUE) {
			usbi_warn(ctx, "could not open hub %s: %s", parent_priv->path, windows_error_str(0));
			return LIBUSB_ERROR_ACCESS;
		}
		size = sizeof(conn_info);
		conn_info.ConnectionIndex = (ULONG)port_number;
		if (!DeviceIoControl(handle, IOCTL_USB_GET_NODE_CONNECTION_INFORMATION_EX, &conn_info, size,
			&conn_info, size, &size, NULL)) {
			usbi_warn(ctx, "could not get node connection information for device '%s': %s",
				device_id, windows_error_str(0));
			safe_closehandle(handle);
			return LIBUSB_ERROR_NO_DEVICE;
		}
		if (conn_info.ConnectionStatus == NoDeviceConnected) {
			usbi_err(ctx, "device '%s' is no longer connected!", device_id);
			safe_closehandle(handle);
			return LIBUSB_ERROR_NO_DEVICE;
		}
		memcpy(&priv->dev_descriptor, &(conn_info.DeviceDescriptor), sizeof(USB_DEVICE_DESCRIPTOR));
		dev->num_configurations = priv->dev_descriptor.bNumConfigurations;
		priv->active_config = conn_info.CurrentConfigurationValue;
		usbi_dbg("found %d configurations (active conf: %d)", dev->num_configurations, priv->active_config);
		// If we can't read the config descriptors, just set the number of confs to zero
		if (cache_config_descriptors(dev, handle, device_id) != LIBUSB_SUCCESS) {
			dev->num_configurations = 0;
			priv->dev_descriptor.bNumConfigurations = 0;
		}
		safe_closehandle(handle);

		if (conn_info.DeviceAddress > UINT8_MAX) {
			usbi_err(ctx, "program assertion failed: device address overflow");
		}
		dev->device_address = (uint8_t)conn_info.DeviceAddress + 1;
		if (dev->device_address == 1) {
			usbi_err(ctx, "program assertion failed: device address collision with root hub");
		}
		switch (conn_info.Speed) {
		case 0: dev->speed = LIBUSB_SPEED_LOW; break;
		case 1: dev->speed = LIBUSB_SPEED_FULL; break;
		case 2: dev->speed = LIBUSB_SPEED_HIGH; break;
		case 3: dev->speed = LIBUSB_SPEED_SUPER; break;
		default:
			usbi_warn(ctx, "Got unknown device speed %d", conn_info.Speed);
			break;
		}
	} else {
		dev->device_address = 1;	// root hubs are set to use device number 1
		force_hcd_device_descriptor(dev);
	}

	usbi_sanitize_device(dev);

	usbi_dbg("(bus: %d, addr: %d, depth: %d, port: %d): '%s'",
		dev->bus_number, dev->device_address, priv->depth, priv->port, device_id);

	return LIBUSB_SUCCESS;
}

// Returns the api type, or 0 if not found/unsupported
static void get_api_type(struct libusb_context *ctx, HDEVINFO *dev_info,
	SP_DEVINFO_DATA *dev_info_data, int *api, int *sub_api)
{
	// Precedence for filter drivers vs driver is in the order of this array
	struct driver_lookup lookup[3] = {
		{"\0\0", SPDRP_SERVICE, "driver"},
		{"\0\0", SPDRP_UPPERFILTERS, "upper filter driver"},
		{"\0\0", SPDRP_LOWERFILTERS, "lower filter driver"}
	};
	DWORD size, reg_type;
	unsigned k, l;
	int i, j;

	*api = USB_API_UNSUPPORTED;
	*sub_api = SUB_API_NOTSET;
	// Check the service & filter names to know the API we should use
	for (k=0; k<3; k++) {
		if (SetupDiGetDeviceRegistryPropertyA(*dev_info, dev_info_data, lookup[k].reg_prop,
			&reg_type, (BYTE*)lookup[k].list, MAX_KEY_LENGTH, &size)) {
			// Turn the REG_SZ SPDRP_SERVICE into REG_MULTI_SZ
			if (lookup[k].reg_prop == SPDRP_SERVICE) {
				// our buffers are MAX_KEY_LENGTH+1 so we can overflow if needed
				lookup[k].list[safe_strlen(lookup[k].list)+1] = 0;
			}
			// MULTI_SZ is a pain to work with. Turn it into something much more manageable
			// NB: none of the driver names we check against contain LIST_SEPARATOR,
			// (currently ';'), so even if an unsuported one does, it's not an issue
			for (l=0; (lookup[k].list[l] != 0) || (lookup[k].list[l+1] != 0); l++) {
				if (lookup[k].list[l] == 0) {
					lookup[k].list[l] = LIST_SEPARATOR;
				}
			}
			usbi_dbg("%s(s): %s", lookup[k].designation, lookup[k].list);
		} else {
			if (GetLastError() != ERROR_INVALID_DATA) {
				usbi_dbg("could not access %s: %s", lookup[k].designation, windows_error_str(0));
			}
			lookup[k].list[0] = 0;
		}
	}

	for (i=1; i<USB_API_MAX; i++) {
		for (k=0; k<3; k++) {
			j = get_sub_api(lookup[k].list, i);
			if (j >= 0) {
				usbi_dbg("matched %s name against %s API", 
					lookup[k].designation, (i!=USB_API_WINUSBX)?usb_api_backend[i].designation:sub_api_name[j]);
				*api = i;
				*sub_api = j;
				return;
			}
		}
	}
}

static int set_composite_interface(struct libusb_context* ctx, struct libusb_device* dev,
							char* dev_interface_path, char* device_id, int api, int sub_api)
{
	unsigned i;
	struct windows_device_priv *priv = _device_priv(dev);
	int interface_number;

	if (priv->apib->id != USB_API_COMPOSITE) {
		usbi_err(ctx, "program assertion failed: '%s' is not composite", device_id);
		return LIBUSB_ERROR_NO_DEVICE;
	}

	// Because MI_## are not necessarily in sequential order (some composite
	// devices will have only MI_00 & MI_03 for instance), we retrieve the actual
	// interface number from the path's MI value
	interface_number = 0;
	for (i=0; device_id[i] != 0; ) {
		if ( (device_id[i++] == 'M') && (device_id[i++] == 'I')
		  && (device_id[i++] == '_') ) {
			interface_number = (device_id[i++] - '0')*10;
			interface_number += device_id[i] - '0';
			break;
		}
	}

	if (device_id[i] == 0) {
		usbi_warn(ctx, "failure to read interface number for %s. Using default value %d",
			device_id, interface_number);
	}

	if (priv->usb_interface[interface_number].path != NULL) {
		safe_free(priv->usb_interface[interface_number].path);
	}

	usbi_dbg("interface[%d] = %s", interface_number, dev_interface_path);
	priv->usb_interface[interface_number].path = dev_interface_path;
	priv->usb_interface[interface_number].apib = &usb_api_backend[api];
	priv->usb_interface[interface_number].sub_api = sub_api;

	return LIBUSB_SUCCESS;
}

/*
 * get_device_list: libusbx backend device enumeration function
 */
static int windows_get_device_list(struct libusb_context *ctx, struct discovered_devs **_discdevs)
{
	struct discovered_devs *discdevs;
	HDEVINFO dev_info = { 0 };
	const char* usb_class[] = {"USB", "NUSB3", "IUSB3"};
	SP_DEVINFO_DATA dev_info_data = { 0 };
	SP_DEVICE_INTERFACE_DETAIL_DATA_A *dev_interface_details = NULL;
#define MAX_ENUM_GUIDS 64
	const GUID* guid[MAX_ENUM_GUIDS];
#define HCD_PASS 0
#define HUB_PASS 1
#define GEN_PASS 2
#define DEV_PASS 3
	int r = LIBUSB_SUCCESS;
	int api, sub_api;
	size_t class_index = 0;
	unsigned int nb_guids, pass, i, j, ancestor;
	char path[MAX_PATH_LENGTH];
	char strbuf[MAX_PATH_LENGTH];
	struct libusb_device *dev, *parent_dev;
	struct windows_device_priv *priv, *parent_priv;
	char* dev_interface_path = NULL;
	char* dev_id_path = NULL;
	unsigned long session_id;
	DWORD size, reg_type, port_nr, install_state;
	HKEY key;
	WCHAR guid_string_w[MAX_GUID_STRING_LENGTH];
	GUID* if_guid;
	LONG s;
	// Keep a list of newly allocated devs to unref
	libusb_device** unref_list;
	unsigned int unref_size = 64;
	unsigned int unref_cur = 0;

	// PASS 1 : (re)enumerate HCDs (allows for HCD hotplug)
	// PASS 2 : (re)enumerate HUBS
	// PASS 3 : (re)enumerate generic USB devices (including driverless)
	//           and list additional USB device interface GUIDs to explore
	// PASS 4 : (re)enumerate master USB devices that have a device interface
	// PASS 5+: (re)enumerate device interfaced GUIDs and set the device interfaces.

	// Init the GUID table
	guid[HCD_PASS] = &GUID_DEVINTERFACE_USB_HOST_CONTROLLER;
	guid[HUB_PASS] = &GUID_DEVINTERFACE_USB_HUB;
	guid[GEN_PASS] = NULL;
	guid[DEV_PASS] = &GUID_DEVINTERFACE_USB_DEVICE;
	nb_guids = DEV_PASS+1;

	unref_list = (libusb_device**) calloc(unref_size, sizeof(libusb_device*));
	if (unref_list == NULL) {
		return LIBUSB_ERROR_NO_MEM;
	}

	for (pass = 0; ((pass < nb_guids) && (r == LIBUSB_SUCCESS)); pass++) {
//#define ENUM_DEBUG
#ifdef ENUM_DEBUG
		const char *passname[] = { "HCD", "HUB", "GEN", "DEV", "EXT" };
		usbi_dbg("\n#### PROCESSING %ss %s", passname[(pass<=DEV_PASS)?pass:DEV_PASS+1],
			(pass!=GEN_PASS)?guid_to_string(guid[pass]):"");
#endif
		for (i = 0; ; i++) {
			// safe loop: free up any (unprotected) dynamic resource
			// NB: this is always executed before breaking the loop
			safe_free(dev_interface_details);
			safe_free(dev_interface_path);
			safe_free(dev_id_path);
			priv = parent_priv = NULL;
			dev = parent_dev = NULL;

			// Safe loop: end of loop conditions
			if (r != LIBUSB_SUCCESS) {
				break;
			}
			if ((pass == HCD_PASS) && (i == UINT8_MAX)) {
				usbi_warn(ctx, "program assertion failed - found more than %d buses, skipping the rest.", UINT8_MAX);
				break;
			}
			if (pass != GEN_PASS) {
				// Except for GEN, all passes deal with device interfaces
				dev_interface_details = get_interface_details(ctx, &dev_info, &dev_info_data, guid[pass], i);
				if (dev_interface_details == NULL) {
					break;
				} else {
					dev_interface_path = sanitize_path(dev_interface_details->DevicePath);
					if (dev_interface_path == NULL) {
						usbi_warn(ctx, "could not sanitize device interface path for '%s'", dev_interface_details->DevicePath);
						continue;
					}
				}
			} else {
				// Workaround for a Nec/Renesas USB 3.0 driver bug where root hubs are
				// being listed under the "NUSB3" PnP Symbolic Name rather than "USB".
				// The Intel USB 3.0 driver behaves similar, but uses "IUSB3"
				for (; class_index < ARRAYSIZE(usb_class); class_index++) {
					if (get_devinfo_data(ctx, &dev_info, &dev_info_data, usb_class[class_index], i))
						break;
					i = 0;
				}
				if (class_index >= ARRAYSIZE(usb_class))
					break;
			}

			// Read the Device ID path. This is what we'll use as UID
			// Note that if the device is plugged in a different port or hub, the Device ID changes
			if (CM_Get_Device_IDA(dev_info_data.DevInst, path, sizeof(path), 0) != CR_SUCCESS) {
				usbi_warn(ctx, "could not read the device id path for devinst %X, skipping",
					dev_info_data.DevInst);
				continue;
			}
			dev_id_path = sanitize_path(path);
			if (dev_id_path == NULL) {
				usbi_warn(ctx, "could not sanitize device id path for devinst %X, skipping",
					dev_info_data.DevInst);
				continue;
			}
#ifdef ENUM_DEBUG
			usbi_dbg("PRO: %s", dev_id_path);
#endif

			// The SPDRP_ADDRESS for USB devices is the device port number on the hub
			port_nr = 0;
			if ((pass >= HUB_PASS) && (pass <= GEN_PASS)) {
				if ( (!SetupDiGetDeviceRegistryPropertyA(dev_info, &dev_info_data, SPDRP_ADDRESS,
					&reg_type, (BYTE*)&port_nr, 4, &size))
				  || (size != 4) ) {
					usbi_warn(ctx, "could not retrieve port number for device '%s', skipping: %s",
						dev_id_path, windows_error_str(0));
					continue;
				}
			}

			// Set API to use or get additional data from generic pass
			api = USB_API_UNSUPPORTED;
			sub_api = SUB_API_NOTSET;
			switch (pass) {
			case HCD_PASS:
				break;
			case GEN_PASS:
				// We use the GEN pass to detect driverless devices...
				size = sizeof(strbuf);
				if (!SetupDiGetDeviceRegistryPropertyA(dev_info, &dev_info_data, SPDRP_DRIVER,
					&reg_type, (BYTE*)strbuf, size, &size)) {
						usbi_info(ctx, "The following device has no driver: '%s'", dev_id_path);
						usbi_info(ctx, "libusbx will not be able to access it.");
				}
				// ...and to add the additional device interface GUIDs
				key = SetupDiOpenDevRegKey(dev_info, &dev_info_data, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
				if (key != INVALID_HANDLE_VALUE) {
					size = sizeof(guid_string_w);
					s = RegQueryValueExW(key, L"DeviceInterfaceGUIDs", NULL, &reg_type,
						(BYTE*)guid_string_w, &size);
					RegCloseKey(key);
					if (s == ERROR_SUCCESS) {
						if (nb_guids >= MAX_ENUM_GUIDS) {
							// If this assert is ever reported, grow a GUID table dynamically
							usbi_err(ctx, "program assertion failed: too many GUIDs");
							LOOP_BREAK(LIBUSB_ERROR_OVERFLOW);
						}
						if_guid = (GUID*) calloc(1, sizeof(GUID));
						CLSIDFromString(guid_string_w, if_guid);
						guid[nb_guids++] = if_guid;
						usbi_dbg("extra GUID: %s", guid_to_string(if_guid));
					}
				}
				break;
			default:
				// Get the API type (after checking that the driver installation is OK)
				if ( (!SetupDiGetDeviceRegistryPropertyA(dev_info, &dev_info_data, SPDRP_INSTALL_STATE,
					&reg_type, (BYTE*)&install_state, 4, &size))
				  || (size != 4) ){
					usbi_warn(ctx, "could not detect installation state of driver for '%s': %s",
						dev_id_path, windows_error_str(0));
				} else if (install_state != 0) {
					usbi_warn(ctx, "driver for device '%s' is reporting an issue (code: %d) - skipping",
						dev_id_path, install_state);
					continue;
				}
				get_api_type(ctx, &dev_info, &dev_info_data, &api, &sub_api);
				break;
			}

			// Find parent device (for the passes that need it)
			switch (pass) {
			case HCD_PASS:
			case DEV_PASS:
			case HUB_PASS:
				break;
			default:
				// Go through the ancestors until we see a face we recognize
				parent_dev = NULL;
				for (ancestor = 1; parent_dev == NULL; ancestor++) {
					session_id = get_ancestor_session_id(dev_info_data.DevInst, ancestor);
					if (session_id == 0) {
						break;
					}
					parent_dev = usbi_get_device_by_session_id(ctx, session_id);
				}
				if (parent_dev == NULL) {
					usbi_dbg("unlisted ancestor for '%s' (non USB HID, newly connected, etc.) - ignoring", dev_id_path);
					continue;
				}
				parent_priv = _device_priv(parent_dev);
				// virtual USB devices are also listed during GEN - don't process these yet
				if ( (pass == GEN_PASS) && (parent_priv->apib->id != USB_API_HUB) ) {
					continue;
				}
				break;
			}

			// Create new or match existing device, using the (hashed) device_id as session id
			if (pass <= DEV_PASS) {	// For subsequent passes, we'll lookup the parent
				// These are the passes that create "new" devices
				session_id = htab_hash(dev_id_path);
				dev = usbi_get_device_by_session_id(ctx, session_id);
				if (dev == NULL) {
					if (pass == DEV_PASS) {
						// This can occur if the OS only reports a newly plugged device after we started enum
						usbi_warn(ctx, "'%s' was only detected in late pass (newly connected device?)"
							" - ignoring", dev_id_path);
						continue;
					}
					usbi_dbg("allocating new device for session [%X]", session_id);
					if ((dev = usbi_alloc_device(ctx, session_id)) == NULL) {
						LOOP_BREAK(LIBUSB_ERROR_NO_MEM);
					}
					windows_device_priv_init(dev);
					// Keep track of devices that need unref
					unref_list[unref_cur++] = dev;
					if (unref_cur >= unref_size) {
						unref_size += 64;
						unref_list = usbi_reallocf(unref_list, unref_size*sizeof(libusb_device*));
						if (unref_list == NULL) {
							usbi_err(ctx, "could not realloc list for unref - aborting.");
							LOOP_BREAK(LIBUSB_ERROR_NO_MEM);
						}
					}
				} else {
					usbi_dbg("found existing device for session [%X] (%d.%d)",
						session_id, dev->bus_number, dev->device_address);
				}
				priv = _device_priv(dev);
			}

			// Setup device
			switch (pass) {
			case HCD_PASS:
				dev->bus_number = (uint8_t)(i + 1);	// bus 0 is reserved for disconnected
				dev->device_address = 0;
				dev->num_configurations = 0;
				priv->apib = &usb_api_backend[USB_API_HUB];
				priv->sub_api = SUB_API_NOTSET;
				priv->depth = UINT8_MAX;	// Overflow to 0 for HCD Hubs
				priv->path = dev_interface_path; dev_interface_path = NULL;
				break;
			case HUB_PASS:
			case DEV_PASS:
				// If the device has already been setup, don't do it again
				if (priv->path != NULL)
					break;
				// Take care of API initialization
				priv->path = dev_interface_path; dev_interface_path = NULL;
				priv->apib = &usb_api_backend[api];
				priv->sub_api = sub_api;
				switch(api) {
				case USB_API_COMPOSITE:
				case USB_API_HUB:
					break;
				default:
					// For other devices, the first interface is the same as the device
					priv->usb_interface[0].path = (char*) calloc(safe_strlen(priv->path)+1, 1);
					if (priv->usb_interface[0].path != NULL) {
						safe_strcpy(priv->usb_interface[0].path, safe_strlen(priv->path)+1, priv->path);
					} else {
						usbi_warn(ctx, "could not duplicate interface path '%s'", priv->path);
					}
					// The following is needed if we want API calls to work for both simple
					// and composite devices.
					for(j=0; j<USB_MAXINTERFACES; j++) {
						priv->usb_interface[j].apib = &usb_api_backend[api];
					}
					break;
				}
				break;
			case GEN_PASS:
				r = init_device(dev, parent_dev, (uint8_t)port_nr, dev_id_path, dev_info_data.DevInst);
				if (r == LIBUSB_SUCCESS) {
					// Append device to the list of discovered devices
					discdevs = discovered_devs_append(*_discdevs, dev);
					if (!discdevs) {
						LOOP_BREAK(LIBUSB_ERROR_NO_MEM);
					}
					*_discdevs = discdevs;
				} else if (r == LIBUSB_ERROR_NO_DEVICE) {
					// This can occur if the device was disconnected but Windows hasn't
					// refreshed its enumeration yet - in that case, we ignore the device
					r = LIBUSB_SUCCESS;
				}
				break;
			default:	// later passes
				if (parent_priv->apib->id == USB_API_COMPOSITE) {
					usbi_dbg("setting composite interface for [%lX]:", parent_dev->session_data);
					switch (set_composite_interface(ctx, parent_dev, dev_interface_path, dev_id_path, api, sub_api)) {
					case LIBUSB_SUCCESS:
						dev_interface_path = NULL;
						break;
					case LIBUSB_ERROR_ACCESS:
						// interface has already been set => make sure dev_interface_path is freed then
						break;
					default:
						LOOP_BREAK(r);
						break;
					}
				}
				break;
			}
		}
	}

	// Free any additional GUIDs
	for (pass = DEV_PASS+1; pass < nb_guids; pass++) {
		safe_free(guid[pass]);
	}

	// Unref newly allocated devs
	for (i=0; i<unref_cur; i++) {
		safe_unref_device(unref_list[i]);
	}
	safe_free(unref_list);

	return r;
}

/*
 * exit: libusbx backend deinitialization function
 */
static void windows_exit(void)
{
	int i;
	HANDLE semaphore;
	char sem_name[11+1+8]; // strlen(libusb_init)+'\0'+(32-bit hex PID)

	sprintf(sem_name, "libusb_init%08X", (unsigned int)GetCurrentProcessId()&0xFFFFFFFF);
	semaphore = CreateSemaphoreA(NULL, 1, 1, sem_name);
	if (semaphore == NULL) {
		return;
	}

	// A successful wait brings our semaphore count to 0 (unsignaled)
	// => any concurent wait stalls until the semaphore release
	if (WaitForSingleObject(semaphore, INFINITE) != WAIT_OBJECT_0) {
		CloseHandle(semaphore);
		return;
	}

	// Only works if exits and inits are balanced exactly
	if (--concurrent_usage < 0) {	// Last exit
		for (i=0; i<USB_API_MAX; i++) {
			usb_api_backend[i].exit(SUB_API_NOTSET);
		}
		exit_polling();

		if (timer_thread) {
			SetEvent(timer_request[1]); // actually the signal to quit the thread.
			if (WAIT_OBJECT_0 != WaitForSingleObject(timer_thread, INFINITE)) {
				usbi_dbg("could not wait for timer thread to quit");
				TerminateThread(timer_thread, 1);
			}
			CloseHandle(timer_thread);
			timer_thread = NULL;
		}
		for (i = 0; i < 2; i++) {
			if (timer_request[i]) {
				CloseHandle(timer_request[i]);
				timer_request[i] = NULL;
			}
		}
		if (timer_response) {
			CloseHandle(timer_response);
			timer_response = NULL;
		}
		if (timer_mutex) {
			CloseHandle(timer_mutex);
			timer_mutex = NULL;
		}
		htab_destroy();
	}

	ReleaseSemaphore(semaphore, 1, NULL);	// increase count back to 1
	CloseHandle(semaphore);
}

static int windows_get_device_descriptor(struct libusb_device *dev, unsigned char *buffer, int *host_endian)
{
	struct windows_device_priv *priv = _device_priv(dev);

	memcpy(buffer, &(priv->dev_descriptor), DEVICE_DESC_LENGTH);
	*host_endian = 0;

	return LIBUSB_SUCCESS;
}

static int windows_get_config_descriptor(struct libusb_device *dev, uint8_t config_index, unsigned char *buffer, size_t len, int *host_endian)
{
	struct windows_device_priv *priv = _device_priv(dev);
	PUSB_CONFIGURATION_DESCRIPTOR config_header;
	size_t size;

	// config index is zero based
	if (config_index >= dev->num_configurations)
		return LIBUSB_ERROR_INVALID_PARAM;

	if ((priv->config_descriptor == NULL) || (priv->config_descriptor[config_index] == NULL))
		return LIBUSB_ERROR_NOT_FOUND;

	config_header = (PUSB_CONFIGURATION_DESCRIPTOR)priv->config_descriptor[config_index];

	size = min(config_header->wTotalLength, len);
	memcpy(buffer, priv->config_descriptor[config_index], size);
	*host_endian = 0;

	return (int)size;
}

/*
 * return the cached copy of the active config descriptor
 */
static int windows_get_active_config_descriptor(struct libusb_device *dev, unsigned char *buffer, size_t len, int *host_endian)
{
	struct windows_device_priv *priv = _device_priv(dev);

	if (priv->active_config == 0)
		return LIBUSB_ERROR_NOT_FOUND;

	// config index is zero based
	return windows_get_config_descriptor(dev, (uint8_t)(priv->active_config-1), buffer, len, host_endian);
}

static int windows_open(struct libusb_device_handle *dev_handle)
{
	struct windows_device_priv *priv = _device_priv(dev_handle->dev);
	struct libusb_context *ctx = DEVICE_CTX(dev_handle->dev);

	if (priv->apib == NULL) {
		usbi_err(ctx, "program assertion failed - device is not initialized");
		return LIBUSB_ERROR_NO_DEVICE;
	}

	return priv->apib->open(SUB_API_NOTSET, dev_handle);
}

static void windows_close(struct libusb_device_handle *dev_handle)
{
	struct windows_device_priv *priv = _device_priv(dev_handle->dev);

	priv->apib->close(SUB_API_NOTSET, dev_handle);
}

static int windows_get_configuration(struct libusb_device_handle *dev_handle, int *config)
{
	struct windows_device_priv *priv = _device_priv(dev_handle->dev);

	if (priv->active_config == 0) {
		*config = 0;
		return LIBUSB_ERROR_NOT_FOUND;
	}

	*config = priv->active_config;
	return LIBUSB_SUCCESS;
}

/*
 * from http://msdn.microsoft.com/en-us/library/ms793522.aspx: "The port driver
 * does not currently expose a service that allows higher-level drivers to set
 * the configuration."
 */
static int windows_set_configuration(struct libusb_device_handle *dev_handle, int config)
{
	struct windows_device_priv *priv = _device_priv(dev_handle->dev);
	int r = LIBUSB_SUCCESS;

	if (config >= USB_MAXCONFIG)
		return LIBUSB_ERROR_INVALID_PARAM;

	r = libusb_control_transfer(dev_handle, LIBUSB_ENDPOINT_OUT |
		LIBUSB_REQUEST_TYPE_STANDARD | LIBUSB_RECIPIENT_DEVICE,
		LIBUSB_REQUEST_SET_CONFIGURATION, (uint16_t)config,
		0, NULL, 0, 1000);

	if (r == LIBUSB_SUCCESS) {
		priv->active_config = (uint8_t)config;
	}
	return r;
}

static int windows_claim_interface(struct libusb_device_handle *dev_handle, int iface)
{
	int r = LIBUSB_SUCCESS;
	struct windows_device_priv *priv = _device_priv(dev_handle->dev);

	if (iface >= USB_MAXINTERFACES)
		return LIBUSB_ERROR_INVALID_PARAM;

	safe_free(priv->usb_interface[iface].endpoint);
	priv->usb_interface[iface].nb_endpoints= 0;

	r = priv->apib->claim_interface(SUB_API_NOTSET, dev_handle, iface);

	if (r == LIBUSB_SUCCESS) {
		r = windows_assign_endpoints(dev_handle, iface, 0);
	}

	return r;
}

static int windows_set_interface_altsetting(struct libusb_device_handle *dev_handle, int iface, int altsetting)
{
	int r = LIBUSB_SUCCESS;
	struct windows_device_priv *priv = _device_priv(dev_handle->dev);

	safe_free(priv->usb_interface[iface].endpoint);
	priv->usb_interface[iface].nb_endpoints= 0;

	r = priv->apib->set_interface_altsetting(SUB_API_NOTSET, dev_handle, iface, altsetting);

	if (r == LIBUSB_SUCCESS) {
		r = windows_assign_endpoints(dev_handle, iface, altsetting);
	}

	return r;
}

static int windows_release_interface(struct libusb_device_handle *dev_handle, int iface)
{
	struct windows_device_priv *priv = _device_priv(dev_handle->dev);

	return priv->apib->release_interface(SUB_API_NOTSET, dev_handle, iface);
}

static int windows_clear_halt(struct libusb_device_handle *dev_handle, unsigned char endpoint)
{
	struct windows_device_priv *priv = _device_priv(dev_handle->dev);
	return priv->apib->clear_halt(SUB_API_NOTSET, dev_handle, endpoint);
}

static int windows_reset_device(struct libusb_device_handle *dev_handle)
{
	struct windows_device_priv *priv = _device_priv(dev_handle->dev);
	return priv->apib->reset_device(SUB_API_NOTSET, dev_handle);
}

// The 3 functions below are unlikely to ever get supported on Windows
static int windows_kernel_driver_active(struct libusb_device_handle *dev_handle, int iface)
{
	return LIBUSB_ERROR_NOT_SUPPORTED;
}

static int windows_attach_kernel_driver(struct libusb_device_handle *dev_handle, int iface)
{
	return LIBUSB_ERROR_NOT_SUPPORTED;
}

static int windows_detach_kernel_driver(struct libusb_device_handle *dev_handle, int iface)
{
	return LIBUSB_ERROR_NOT_SUPPORTED;
}

static void windows_destroy_device(struct libusb_device *dev)
{
	windows_device_priv_release(dev);
}

static void windows_clear_transfer_priv(struct usbi_transfer *itransfer)
{
	struct windows_transfer_priv *transfer_priv = (struct windows_transfer_priv*)usbi_transfer_get_os_priv(itransfer);

	usbi_free_fd(&transfer_priv->pollable_fd);
	// When auto claim is in use, attempt to release the auto-claimed interface
	auto_release(itransfer);
}

static int submit_bulk_transfer(struct usbi_transfer *itransfer)
{
	struct libusb_transfer *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
	struct libusb_context *ctx = DEVICE_CTX(transfer->dev_handle->dev);
	struct windows_transfer_priv *transfer_priv = (struct windows_transfer_priv*)usbi_transfer_get_os_priv(itransfer);
	struct windows_device_priv *priv = _device_priv(transfer->dev_handle->dev);
	int r;

	r = priv->apib->submit_bulk_transfer(SUB_API_NOTSET, itransfer);
	if (r != LIBUSB_SUCCESS) {
		return r;
	}

	usbi_add_pollfd(ctx, transfer_priv->pollable_fd.fd,
		(short)(IS_XFERIN(transfer) ? POLLIN : POLLOUT));

	itransfer->flags |= USBI_TRANSFER_UPDATED_FDS;
	return LIBUSB_SUCCESS;
}

static int submit_iso_transfer(struct usbi_transfer *itransfer)
{
	struct libusb_transfer *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
	struct libusb_context *ctx = DEVICE_CTX(transfer->dev_handle->dev);
	struct windows_transfer_priv *transfer_priv = (struct windows_transfer_priv*)usbi_transfer_get_os_priv(itransfer);
	struct windows_device_priv *priv = _device_priv(transfer->dev_handle->dev);
	int r;

	r = priv->apib->submit_iso_transfer(SUB_API_NOTSET, itransfer);
	if (r != LIBUSB_SUCCESS) {
		return r;
	}

	usbi_add_pollfd(ctx, transfer_priv->pollable_fd.fd,
		(short)(IS_XFERIN(transfer) ? POLLIN : POLLOUT));

	itransfer->flags |= USBI_TRANSFER_UPDATED_FDS;
	return LIBUSB_SUCCESS;
}

static int submit_control_transfer(struct usbi_transfer *itransfer)
{
	struct libusb_transfer *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
	struct libusb_context *ctx = DEVICE_CTX(transfer->dev_handle->dev);
	struct windows_transfer_priv *transfer_priv = (struct windows_transfer_priv*)usbi_transfer_get_os_priv(itransfer);
	struct windows_device_priv *priv = _device_priv(transfer->dev_handle->dev);
	int r;

	r = priv->apib->submit_control_transfer(SUB_API_NOTSET, itransfer);
	if (r != LIBUSB_SUCCESS) {
		return r;
	}

	usbi_add_pollfd(ctx, transfer_priv->pollable_fd.fd, POLLIN);

	itransfer->flags |= USBI_TRANSFER_UPDATED_FDS;
	return LIBUSB_SUCCESS;

}

static int windows_submit_transfer(struct usbi_transfer *itransfer)
{
	struct libusb_transfer *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);

	switch (transfer->type) {
	case LIBUSB_TRANSFER_TYPE_CONTROL:
		return submit_control_transfer(itransfer);
	case LIBUSB_TRANSFER_TYPE_BULK:
	case LIBUSB_TRANSFER_TYPE_INTERRUPT:
		if (IS_XFEROUT(transfer) &&
		    transfer->flags & LIBUSB_TRANSFER_ADD_ZERO_PACKET)
			return LIBUSB_ERROR_NOT_SUPPORTED;
		return submit_bulk_transfer(itransfer);
	case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS:
		return submit_iso_transfer(itransfer);
	default:
		usbi_err(TRANSFER_CTX(transfer), "unknown endpoint type %d", transfer->type);
		return LIBUSB_ERROR_INVALID_PARAM;
	}
}

static int windows_abort_control(struct usbi_transfer *itransfer)
{
	struct libusb_transfer *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
	struct windows_device_priv *priv = _device_priv(transfer->dev_handle->dev);

	return priv->apib->abort_control(SUB_API_NOTSET, itransfer);
}

static int windows_abort_transfers(struct usbi_transfer *itransfer)
{
	struct libusb_transfer *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
	struct windows_device_priv *priv = _device_priv(transfer->dev_handle->dev);

	return priv->apib->abort_transfers(SUB_API_NOTSET, itransfer);
}

static int windows_cancel_transfer(struct usbi_transfer *itransfer)
{
	struct libusb_transfer *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);

	switch (transfer->type) {
	case LIBUSB_TRANSFER_TYPE_CONTROL:
		return windows_abort_control(itransfer);
	case LIBUSB_TRANSFER_TYPE_BULK:
	case LIBUSB_TRANSFER_TYPE_INTERRUPT:
	case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS:
		return windows_abort_transfers(itransfer);
	default:
		usbi_err(ITRANSFER_CTX(itransfer), "unknown endpoint type %d", transfer->type);
		return LIBUSB_ERROR_INVALID_PARAM;
	}
}

static void windows_transfer_callback(struct usbi_transfer *itransfer, uint32_t io_result, uint32_t io_size)
{
	struct libusb_transfer *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
	struct windows_device_priv *priv = _device_priv(transfer->dev_handle->dev);
	int status, istatus;

	usbi_dbg("handling I/O completion with errcode %d, size %d", io_result, io_size);

	switch(io_result) {
	case NO_ERROR:
		status = priv->apib->copy_transfer_data(SUB_API_NOTSET, itransfer, io_size);
		break;
	case ERROR_GEN_FAILURE:
		usbi_dbg("detected endpoint stall");
		status = LIBUSB_TRANSFER_STALL;
		break;
	case ERROR_SEM_TIMEOUT:
		usbi_dbg("detected semaphore timeout");
		status = LIBUSB_TRANSFER_TIMED_OUT;
		break;
	case ERROR_OPERATION_ABORTED:
		istatus = priv->apib->copy_transfer_data(SUB_API_NOTSET, itransfer, io_size);
		if (istatus != LIBUSB_TRANSFER_COMPLETED) {
			usbi_dbg("Failed to copy partial data in aborted operation: %d", istatus);
		}
		if (itransfer->flags & USBI_TRANSFER_TIMED_OUT) {
			usbi_dbg("detected timeout");
			status = LIBUSB_TRANSFER_TIMED_OUT;
		} else {
			usbi_dbg("detected operation aborted");
			status = LIBUSB_TRANSFER_CANCELLED;
		}
		break;
	default:
		usbi_err(ITRANSFER_CTX(itransfer), "detected I/O error %d: %s", io_result, windows_error_str(0));
		status = LIBUSB_TRANSFER_ERROR;
		break;
	}
	windows_clear_transfer_priv(itransfer);	// Cancel polling
	usbi_handle_transfer_completion(itransfer, (enum libusb_transfer_status)status);
}

static void windows_handle_callback (struct usbi_transfer *itransfer, uint32_t io_result, uint32_t io_size)
{
	struct libusb_transfer *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);

	switch (transfer->type) {
	case LIBUSB_TRANSFER_TYPE_CONTROL:
	case LIBUSB_TRANSFER_TYPE_BULK:
	case LIBUSB_TRANSFER_TYPE_INTERRUPT:
	case LIBUSB_TRANSFER_TYPE_ISOCHRONOUS:
		windows_transfer_callback (itransfer, io_result, io_size);
		break;
	default:
		usbi_err(ITRANSFER_CTX(itransfer), "unknown endpoint type %d", transfer->type);
	}
}

static int windows_handle_events(struct libusb_context *ctx, struct pollfd *fds, POLL_NFDS_TYPE nfds, int num_ready)
{
	struct windows_transfer_priv* transfer_priv = NULL;
	POLL_NFDS_TYPE i = 0;
	bool found = false;
	struct usbi_transfer *transfer;
	DWORD io_size, io_result;

	usbi_mutex_lock(&ctx->open_devs_lock);
	for (i = 0; i < nfds && num_ready > 0; i++) {

		usbi_dbg("checking fd %d with revents = %04x", fds[i].fd, fds[i].revents);

		if (!fds[i].revents) {
			continue;
		}

		num_ready--;

		// Because a Windows OVERLAPPED is used for poll emulation,
		// a pollable fd is created and stored with each transfer
		usbi_mutex_lock(&ctx->flying_transfers_lock);
		list_for_each_entry(transfer, &ctx->flying_transfers, list, struct usbi_transfer) {
			transfer_priv = usbi_transfer_get_os_priv(transfer);
			if (transfer_priv->pollable_fd.fd == fds[i].fd) {
				found = true;
				break;
			}
		}
		usbi_mutex_unlock(&ctx->flying_transfers_lock);

		if (found) {
			// Handle async requests that completed synchronously first
			if (HasOverlappedIoCompletedSync(transfer_priv->pollable_fd.overlapped)) {
				io_result = NO_ERROR;
				io_size = (DWORD)transfer_priv->pollable_fd.overlapped->InternalHigh;
			// Regular async overlapped
			} else if (GetOverlappedResult(transfer_priv->pollable_fd.handle,
				transfer_priv->pollable_fd.overlapped, &io_size, false)) {
				io_result = NO_ERROR;
			} else {
				io_result = GetLastError();
			}
			usbi_remove_pollfd(ctx, transfer_priv->pollable_fd.fd);
			// let handle_callback free the event using the transfer wfd
			// If you don't use the transfer wfd, you run a risk of trying to free a
			// newly allocated wfd that took the place of the one from the transfer.
			windows_handle_callback(transfer, io_result, io_size);
		} else {
			usbi_err(ctx, "could not find a matching transfer for fd %x", fds[i]);
			usbi_mutex_unlock(&ctx->open_devs_lock);
			return LIBUSB_ERROR_NOT_FOUND;
		}
	}

	usbi_mutex_unlock(&ctx->open_devs_lock);
	return LIBUSB_SUCCESS;
}

/*
 * Monotonic and real time functions
 */
unsigned __stdcall windows_clock_gettime_threaded(void* param)
{
	LARGE_INTEGER hires_counter, li_frequency;
	LONG nb_responses;
	int timer_index;

	// Init - find out if we have access to a monotonic (hires) timer
	if (!QueryPerformanceFrequency(&li_frequency)) {
		usbi_dbg("no hires timer available on this platform");
		hires_frequency = 0;
		hires_ticks_to_ps = UINT64_C(0);
	} else {
		hires_frequency = li_frequency.QuadPart;
		// The hires frequency can go as high as 4 GHz, so we'll use a conversion
		// to picoseconds to compute the tv_nsecs part in clock_gettime
		hires_ticks_to_ps = UINT64_C(1000000000000) / hires_frequency;
		usbi_dbg("hires timer available (Frequency: %"PRIu64" Hz)", hires_frequency);
	}

	// Signal windows_init() that we're ready to service requests
	if (ReleaseSemaphore(timer_response, 1, NULL) == 0) {
		usbi_dbg("unable to release timer semaphore: %s", windows_error_str(0));
	}

	// Main loop - wait for requests
	while (1) {
		timer_index = WaitForMultipleObjects(2, timer_request, FALSE, INFINITE) - WAIT_OBJECT_0;
		if ( (timer_index != 0) && (timer_index != 1) ) {
			usbi_dbg("failure to wait on requests: %s", windows_error_str(0));
			continue;
		}
		if (request_count[timer_index] == 0) {
			// Request already handled
			ResetEvent(timer_request[timer_index]);
			// There's still a possiblity that a thread sends a request between the
			// time we test request_count[] == 0 and we reset the event, in which case
			// the request would be ignored. The simple solution to that is to test
			// request_count again and process requests if non zero.
			if (request_count[timer_index] == 0)
				continue;
		}
		switch (timer_index) {
		case 0:
			WaitForSingleObject(timer_mutex, INFINITE);
			// Requests to this thread are for hires always
			if (QueryPerformanceCounter(&hires_counter) != 0) {
				timer_tp.tv_sec = (long)(hires_counter.QuadPart / hires_frequency);
				timer_tp.tv_nsec = (long)(((hires_counter.QuadPart % hires_frequency)/1000) * hires_ticks_to_ps);
			} else {
				// Fallback to real-time if we can't get monotonic value
				// Note that real-time clock does not wait on the mutex or this thread.
				windows_clock_gettime(USBI_CLOCK_REALTIME, &timer_tp);
			}
			ReleaseMutex(timer_mutex);

			nb_responses = InterlockedExchange((LONG*)&request_count[0], 0);
			if ( (nb_responses)
			  && (ReleaseSemaphore(timer_response, nb_responses, NULL) == 0) ) {
				usbi_dbg("unable to release timer semaphore: %s", windows_error_str(0));
			}
			continue;
		case 1: // time to quit
			usbi_dbg("timer thread quitting");
			return 0;
		}
	}
}

static int windows_clock_gettime(int clk_id, struct timespec *tp)
{
	FILETIME filetime;
	ULARGE_INTEGER rtime;
	DWORD r;
	switch(clk_id) {
	case USBI_CLOCK_MONOTONIC:
		if (hires_frequency != 0) {
			while (1) {
				InterlockedIncrement((LONG*)&request_count[0]);
				SetEvent(timer_request[0]);
				r = WaitForSingleObject(timer_response, TIMER_REQUEST_RETRY_MS);
				switch(r) {
				case WAIT_OBJECT_0:
					WaitForSingleObject(timer_mutex, INFINITE);
					*tp = timer_tp;
					ReleaseMutex(timer_mutex);
					return LIBUSB_SUCCESS;
				case WAIT_TIMEOUT:
					usbi_dbg("could not obtain a timer value within reasonable timeframe - too much load?");
					break; // Retry until successful
				default:
					usbi_dbg("WaitForSingleObject failed: %s", windows_error_str(0));
					return LIBUSB_ERROR_OTHER;
				}
			}
		}
		// Fall through and return real-time if monotonic was not detected @ timer init
	case USBI_CLOCK_REALTIME:
		// We follow http://msdn.microsoft.com/en-us/library/ms724928%28VS.85%29.aspx
		// with a predef epoch_time to have an epoch that starts at 1970.01.01 00:00
		// Note however that our resolution is bounded by the Windows system time
		// functions and is at best of the order of 1 ms (or, usually, worse)
		GetSystemTimeAsFileTime(&filetime);
		rtime.LowPart = filetime.dwLowDateTime;
		rtime.HighPart = filetime.dwHighDateTime;
		rtime.QuadPart -= epoch_time;
		tp->tv_sec = (long)(rtime.QuadPart / 10000000);
		tp->tv_nsec = (long)((rtime.QuadPart % 10000000)*100);
		return LIBUSB_SUCCESS;
	default:
		return LIBUSB_ERROR_INVALID_PARAM;
	}
}


// NB: MSVC6 does not support named initializers.
const struct usbi_os_backend windows_backend = {
	"Windows",
	USBI_CAP_HAS_HID_ACCESS,
	windows_init,
	windows_exit,

	windows_get_device_list,
	NULL,				/* hotplug_poll */
	windows_open,
	NULL,       /* open_fd */
	windows_close,

	windows_get_device_descriptor,
	windows_get_active_config_descriptor,
	windows_get_config_descriptor,
	NULL,				/* get_config_descriptor_by_value() */

	windows_get_configuration,
	windows_set_configuration,
	windows_claim_interface,
	windows_release_interface,

	windows_set_interface_altsetting,
	windows_clear_halt,
	windows_reset_device,

	windows_kernel_driver_active,
	windows_detach_kernel_driver,
	windows_attach_kernel_driver,

	windows_destroy_device,

	windows_submit_transfer,
	windows_cancel_transfer,
	windows_clear_transfer_priv,

	windows_handle_events,

	windows_clock_gettime,
#if defined(USBI_TIMERFD_AVAILABLE)
	NULL,
#endif
	sizeof(struct windows_device_priv),
	sizeof(struct windows_device_handle_priv),
	sizeof(struct windows_transfer_priv),
	0,
};


/*
 * USB API backends
 */
static int unsupported_init(int sub_api, struct libusb_context *ctx) {
	return LIBUSB_SUCCESS;
}
static int unsupported_exit(int sub_api) {
	return LIBUSB_SUCCESS;
}
static int unsupported_open(int sub_api, struct libusb_device_handle *dev_handle) {
	PRINT_UNSUPPORTED_API(open);
}
static void unsupported_close(int sub_api, struct libusb_device_handle *dev_handle) {
	usbi_dbg("unsupported API call for 'close'");
}
static int unsupported_configure_endpoints(int sub_api, struct libusb_device_handle *dev_handle, int iface) {
	PRINT_UNSUPPORTED_API(configure_endpoints);
}
static int unsupported_claim_interface(int sub_api, struct libusb_device_handle *dev_handle, int iface) {
	PRINT_UNSUPPORTED_API(claim_interface);
}
static int unsupported_set_interface_altsetting(int sub_api, struct libusb_device_handle *dev_handle, int iface, int altsetting) {
	PRINT_UNSUPPORTED_API(set_interface_altsetting);
}
static int unsupported_release_interface(int sub_api, struct libusb_device_handle *dev_handle, int iface) {
	PRINT_UNSUPPORTED_API(release_interface);
}
static int unsupported_clear_halt(int sub_api, struct libusb_device_handle *dev_handle, unsigned char endpoint) {
	PRINT_UNSUPPORTED_API(clear_halt);
}
static int unsupported_reset_device(int sub_api, struct libusb_device_handle *dev_handle) {
	PRINT_UNSUPPORTED_API(reset_device);
}
static int unsupported_submit_bulk_transfer(int sub_api, struct usbi_transfer *itransfer) {
	PRINT_UNSUPPORTED_API(submit_bulk_transfer);
}
static int unsupported_submit_iso_transfer(int sub_api, struct usbi_transfer *itransfer) {
	PRINT_UNSUPPORTED_API(submit_iso_transfer);
}
static int unsupported_submit_control_transfer(int sub_api, struct usbi_transfer *itransfer) {
	PRINT_UNSUPPORTED_API(submit_control_transfer);
}
static int unsupported_abort_control(int sub_api, struct usbi_transfer *itransfer) {
	PRINT_UNSUPPORTED_API(abort_control);
}
static int unsupported_abort_transfers(int sub_api, struct usbi_transfer *itransfer) {
	PRINT_UNSUPPORTED_API(abort_transfers);
}
static int unsupported_copy_transfer_data(int sub_api, struct usbi_transfer *itransfer, uint32_t io_size) {
	PRINT_UNSUPPORTED_API(copy_transfer_data);
}
static int common_configure_endpoints(int sub_api, struct libusb_device_handle *dev_handle, int iface) {
	return LIBUSB_SUCCESS;
}
// These names must be uppercase
const char* hub_driver_names[] = {"USBHUB", "USBHUB3", "NUSB3HUB", "RUSB3HUB", "FLXHCIH", "TIHUB3", "ETRONHUB3", "VIAHUB3", "ASMTHUB3", "IUSB3HUB"};
const char* composite_driver_names[] = {"USBCCGP"};
const char* winusbx_driver_names[] = WINUSBX_DRV_NAMES;
const struct windows_usb_api_backend usb_api_backend[USB_API_MAX] = {
	{
		USB_API_UNSUPPORTED,
		"Unsupported API",
		NULL,
		0,
		unsupported_init,
		unsupported_exit,
		unsupported_open,
		unsupported_close,
		unsupported_configure_endpoints,
		unsupported_claim_interface,
		unsupported_set_interface_altsetting,
		unsupported_release_interface,
		unsupported_clear_halt,
		unsupported_reset_device,
		unsupported_submit_bulk_transfer,
		unsupported_submit_iso_transfer,
		unsupported_submit_control_transfer,
		unsupported_abort_control,
		unsupported_abort_transfers,
		unsupported_copy_transfer_data,
	}, {
		USB_API_HUB,
		"HUB API",
		hub_driver_names,
		ARRAYSIZE(hub_driver_names),
		unsupported_init,
		unsupported_exit,
		unsupported_open,
		unsupported_close,
		unsupported_configure_endpoints,
		unsupported_claim_interface,
		unsupported_set_interface_altsetting,
		unsupported_release_interface,
		unsupported_clear_halt,
		unsupported_reset_device,
		unsupported_submit_bulk_transfer,
		unsupported_submit_iso_transfer,
		unsupported_submit_control_transfer,
		unsupported_abort_control,
		unsupported_abort_transfers,
		unsupported_copy_transfer_data,
	}, {
		USB_API_COMPOSITE,
		"Composite API",
		composite_driver_names,
		ARRAYSIZE(composite_driver_names),
		composite_init,
		composite_exit,
		composite_open,
		composite_close,
		common_configure_endpoints,
		composite_claim_interface,
		composite_set_interface_altsetting,
		composite_release_interface,
		composite_clear_halt,
		composite_reset_device,
		composite_submit_bulk_transfer,
		composite_submit_iso_transfer,
		composite_submit_control_transfer,
		composite_abort_control,
		composite_abort_transfers,
		composite_copy_transfer_data,
	}, {
		USB_API_WINUSBX,
		"WinUSB-like APIs",
		winusbx_driver_names,
		ARRAYSIZE(winusbx_driver_names),
		winusbx_init,
		winusbx_exit,
		winusbx_open,
		winusbx_close,
		winusbx_configure_endpoints,
		winusbx_claim_interface,
		winusbx_set_interface_altsetting,
		winusbx_release_interface,
		winusbx_clear_halt,
		winusbx_reset_device,
		winusbx_submit_bulk_transfer,
		unsupported_submit_iso_transfer,
		winusbx_submit_control_transfer,
		winusbx_abort_control,
		winusbx_abort_transfers,
		winusbx_copy_transfer_data,
	},
};


/*
 * WinUSB-like (WinUSB, libusb0/libusbK through libusbk DLL) API functions
 */

static int winusbx_init(int sub_api, struct libusb_context *ctx)
{
	return LIBUSB_SUCCESS;
}

static int winusbx_exit(int sub_api)
{
	return LIBUSB_SUCCESS;
}

// NB: open and close must ensure that they only handle interface of
// the right API type, as these functions can be called wholesale from
// composite_open(), with interfaces belonging to different APIs
static int winusbx_open(int sub_api, struct libusb_device_handle *dev_handle)
{
	struct libusb_context *ctx = DEVICE_CTX(dev_handle->dev);
	struct windows_device_priv *priv = _device_priv(dev_handle->dev);
	struct windows_device_handle_priv *handle_priv = _device_handle_priv(dev_handle);

	HANDLE file_handle;
	int i;

	// WinUSB requires a seperate handle for each interface
	for (i = 0; i < USB_MAXINTERFACES; i++) {
		if ( (priv->usb_interface[i].path != NULL)
		  && (priv->usb_interface[i].apib->id == USB_API_WINUSBX) ) {
			file_handle = CreateFileA(priv->usb_interface[i].path, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_WRITE | FILE_SHARE_READ,
				NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
			if (file_handle == INVALID_HANDLE_VALUE) {
				usbi_err(ctx, "could not open device %s (interface %d): %s", priv->usb_interface[i].path, i, windows_error_str(0));
				switch(GetLastError()) {
				case ERROR_FILE_NOT_FOUND:	// The device was disconnected
					return LIBUSB_ERROR_NO_DEVICE;
				case ERROR_ACCESS_DENIED:
					return LIBUSB_ERROR_ACCESS;
				default:
					return LIBUSB_ERROR_IO;
				}
			}
			handle_priv->interface_handle[i].dev_handle = file_handle;
		}
	}

	return LIBUSB_SUCCESS;
}

static void winusbx_close(int sub_api, struct libusb_device_handle *dev_handle)
{
	struct windows_device_handle_priv *handle_priv = _device_handle_priv(dev_handle);
	struct windows_device_priv *priv = _device_priv(dev_handle->dev);
	HANDLE file_handle;
	int i;

	if (sub_api == SUB_API_NOTSET)
		sub_api = priv->sub_api;

	for (i = 0; i < USB_MAXINTERFACES; i++) {
		if (priv->usb_interface[i].apib->id == USB_API_WINUSBX) {
			file_handle = handle_priv->interface_handle[i].dev_handle;
			if ( (file_handle != 0) && (file_handle != INVALID_HANDLE_VALUE)) {
				CloseHandle(file_handle);
			}
		}
	}
}

static int winusbx_configure_endpoints(int sub_api, struct libusb_device_handle *dev_handle, int iface)
{
	struct windows_device_handle_priv *handle_priv = _device_handle_priv(dev_handle);
	struct windows_device_priv *priv = _device_priv(dev_handle->dev);
	HANDLE winusb_handle = handle_priv->interface_handle[iface].api_handle;
	UCHAR policy;
	ULONG timeout = 0;
	uint8_t endpoint_address;
	int i;

	// With handle and enpoints set (in parent), we can setup the default pipe properties
	// see http://download.microsoft.com/download/D/1/D/D1DD7745-426B-4CC3-A269-ABBBE427C0EF/DVC-T705_DDC08.pptx
	for (i=-1; i<priv->usb_interface[iface].nb_endpoints; i++) {
		endpoint_address =(i==-1)?0:priv->usb_interface[iface].endpoint[i];
		if (!WinUsb_SetPipePolicy(winusb_handle, endpoint_address,
			PIPE_TRANSFER_TIMEOUT, sizeof(ULONG), &timeout)) {
			usbi_dbg("failed to set PIPE_TRANSFER_TIMEOUT for control endpoint %02X", endpoint_address);
		}
		if ((i == -1) || (sub_api == SUB_API_LIBUSB0)) {
			continue;	// Other policies don't apply to control endpoint or libusb0
		}
		policy = false;
		if (!WinUsb_SetPipePolicy(winusb_handle, endpoint_address,
			SHORT_PACKET_TERMINATE, sizeof(UCHAR), &policy)) {
			usbi_dbg("failed to disable SHORT_PACKET_TERMINATE for endpoint %02X", endpoint_address);
		}
		if (!WinUsb_SetPipePolicy(winusb_handle, endpoint_address,
			IGNORE_SHORT_PACKETS, sizeof(UCHAR), &policy)) {
			usbi_dbg("failed to disable IGNORE_SHORT_PACKETS for endpoint %02X", endpoint_address);
		}
		policy = true;
		/* ALLOW_PARTIAL_READS must be enabled due to likely libusbK bug. See:
		   https://sourceforge.net/mailarchive/message.php?msg_id=29736015 */
		if (!WinUsb_SetPipePolicy(winusb_handle, endpoint_address,
			ALLOW_PARTIAL_READS, sizeof(UCHAR), &policy)) {
			usbi_dbg("failed to enable ALLOW_PARTIAL_READS for endpoint %02X", endpoint_address);
		}
		if (!WinUsb_SetPipePolicy(winusb_handle, endpoint_address,
			AUTO_CLEAR_STALL, sizeof(UCHAR), &policy)) {
			usbi_dbg("failed to enable AUTO_CLEAR_STALL for endpoint %02X", endpoint_address);
		}
	}

	return LIBUSB_SUCCESS;
}

static int winusbx_claim_interface(int sub_api, struct libusb_device_handle *dev_handle, int iface)
{
	struct libusb_context *ctx = DEVICE_CTX(dev_handle->dev);
	struct windows_device_handle_priv *handle_priv = _device_handle_priv(dev_handle);
	struct windows_device_priv *priv = _device_priv(dev_handle->dev);
	bool is_using_usbccgp = (priv->apib->id == USB_API_COMPOSITE);
	HANDLE file_handle, winusb_handle;
	DWORD err;
	int i;
	SP_DEVICE_INTERFACE_DETAIL_DATA_A *dev_interface_details = NULL;
	HDEVINFO dev_info = INVALID_HANDLE_VALUE;
	SP_DEVINFO_DATA dev_info_data;
	char* dev_path_no_guid = NULL;
	char filter_path[] = "\\\\.\\libusb0-0000";
	bool found_filter = false;

	// If the device is composite, but using the default Windows composite parent driver (usbccgp)
	// or if it's the first WinUSB-like interface, we get a handle through Initialize().
	if ((is_using_usbccgp) || (iface == 0)) {
		// composite device (independent interfaces) or interface 0
		file_handle = handle_priv->interface_handle[iface].dev_handle;
		if ((file_handle == 0) || (file_handle == INVALID_HANDLE_VALUE)) {
			return LIBUSB_ERROR_NOT_FOUND;
		}

		if (!WinUsb_Initialize(file_handle, &winusb_handle)) {
			handle_priv->interface_handle[iface].api_handle = INVALID_HANDLE_VALUE;
			err = GetLastError();
			switch(err) {
			case ERROR_BAD_COMMAND:
				// The device was disconnected
				usbi_err(ctx, "could not access interface %d: %s", iface, windows_error_str(0));
				return LIBUSB_ERROR_NO_DEVICE;
			default:
				// it may be that we're using the libusb0 filter driver.
				// TODO: can we move this whole business into the K/0 DLL?
				for (i = 0; ; i++) {
					safe_free(dev_interface_details);
					safe_free(dev_path_no_guid);
					dev_interface_details = get_interface_details_filter(ctx, &dev_info, &dev_info_data, &GUID_DEVINTERFACE_LIBUSB0_FILTER, i, filter_path);
					if ((found_filter) || (dev_interface_details == NULL)) {
						break;
					}
					// ignore GUID part
					dev_path_no_guid = sanitize_path(strtok(dev_interface_details->DevicePath, "{"));
					if (safe_strncmp(dev_path_no_guid, priv->usb_interface[iface].path, safe_strlen(dev_path_no_guid)) == 0) {
						file_handle = CreateFileA(filter_path, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_WRITE | FILE_SHARE_READ,
							NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
						if (file_handle == INVALID_HANDLE_VALUE) {
							usbi_err(ctx, "could not open device %s: %s", filter_path, windows_error_str(0));
						} else {
							WinUsb_Free(winusb_handle);
							if (!WinUsb_Initialize(file_handle, &winusb_handle)) {
								continue;
							}
							found_filter = true;
							break;
						}
					}
				}
				if (!found_filter) {
					usbi_err(ctx, "could not access interface %d: %s", iface, windows_error_str(err));
					return LIBUSB_ERROR_ACCESS;
				}
			}
		}
		handle_priv->interface_handle[iface].api_handle = winusb_handle;
	} else {
		// For all other interfaces, use GetAssociatedInterface()
		winusb_handle = handle_priv->interface_handle[0].api_handle;
		// It is a requirement for multiple interface devices on Windows that, to you
		// must first claim the first interface before you claim the others
		if ((winusb_handle == 0) || (winusb_handle == INVALID_HANDLE_VALUE)) {
			file_handle = handle_priv->interface_handle[0].dev_handle;
			if (WinUsb_Initialize(file_handle, &winusb_handle)) {
				handle_priv->interface_handle[0].api_handle = winusb_handle;
				usbi_warn(ctx, "auto-claimed interface 0 (required to claim %d with WinUSB)", iface);
			} else {
				usbi_warn(ctx, "failed to auto-claim interface 0 (required to claim %d with WinUSB): %s", iface, windows_error_str(0));
				return LIBUSB_ERROR_ACCESS;
			}
		}
		if (!WinUsb_GetAssociatedInterface(winusb_handle, (UCHAR)(iface-1),
			&handle_priv->interface_handle[iface].api_handle)) {
			handle_priv->interface_handle[iface].api_handle = INVALID_HANDLE_VALUE;
			switch(GetLastError()) {
			case ERROR_NO_MORE_ITEMS:   // invalid iface
				return LIBUSB_ERROR_NOT_FOUND;
			case ERROR_BAD_COMMAND:     // The device was disconnected
				return LIBUSB_ERROR_NO_DEVICE;
			case ERROR_ALREADY_EXISTS:  // already claimed
				return LIBUSB_ERROR_BUSY;
			default:
				usbi_err(ctx, "could not claim interface %d: %s", iface, windows_error_str(0));
				return LIBUSB_ERROR_ACCESS;
			}
		}
	}
	usbi_dbg("claimed interface %d", iface);
	handle_priv->active_interface = iface;

	return LIBUSB_SUCCESS;
}

static int winusbx_release_interface(int sub_api, struct libusb_device_handle *dev_handle, int iface)
{
	struct windows_device_handle_priv *handle_priv = _device_handle_priv(dev_handle);
	struct windows_device_priv *priv = _device_priv(dev_handle->dev);
	HANDLE winusb_handle;

	winusb_handle = handle_priv->interface_handle[iface].api_handle;
	if ((winusb_handle == 0) || (winusb_handle == INVALID_HANDLE_VALUE)) {
		return LIBUSB_ERROR_NOT_FOUND;
	}

	WinUsb_Free(winusb_handle);
	handle_priv->interface_handle[iface].api_handle = INVALID_HANDLE_VALUE;

	return LIBUSB_SUCCESS;
}

/*
 * Return the first valid interface (of the same API type), for control transfers
 */
static int get_valid_interface(struct libusb_device_handle *dev_handle, int api_id)
{
	struct windows_device_handle_priv *handle_priv = _device_handle_priv(dev_handle);
	struct windows_device_priv *priv = _device_priv(dev_handle->dev);
	int i;

	if ((api_id < USB_API_WINUSBX) || (api_id >= USB_API_MAX)) {
		usbi_dbg("unsupported API ID");
		return -1;
	}

	for (i=0; i<USB_MAXINTERFACES; i++) {
		if ( (handle_priv->interface_handle[i].dev_handle != 0)
		  && (handle_priv->interface_handle[i].dev_handle != INVALID_HANDLE_VALUE)
		  && (handle_priv->interface_handle[i].api_handle != 0)
		  && (handle_priv->interface_handle[i].api_handle != INVALID_HANDLE_VALUE)
		  && (priv->usb_interface[i].apib->id == api_id) ) {
			return i;
		}
	}
	return -1;
}

/*
 * Lookup interface by endpoint address. -1 if not found
 */
static int interface_by_endpoint(struct windows_device_priv *priv,
	struct windows_device_handle_priv *handle_priv, uint8_t endpoint_address)
{
	int i, j;
	for (i=0; i<USB_MAXINTERFACES; i++) {
		if (handle_priv->interface_handle[i].api_handle == INVALID_HANDLE_VALUE)
			continue;
		if (handle_priv->interface_handle[i].api_handle == 0)
			continue;
		if (priv->usb_interface[i].endpoint == NULL)
			continue;
		for (j=0; j<priv->usb_interface[i].nb_endpoints; j++) {
			if (priv->usb_interface[i].endpoint[j] == endpoint_address) {
				return i;
			}
		}
	}
	return -1;
}

static int winusbx_submit_control_transfer(int sub_api, struct usbi_transfer *itransfer)
{
	struct libusb_transfer *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
	struct libusb_context *ctx = DEVICE_CTX(transfer->dev_handle->dev);
	struct windows_device_priv *priv = _device_priv(transfer->dev_handle->dev);
	struct windows_transfer_priv *transfer_priv = (struct windows_transfer_priv*)usbi_transfer_get_os_priv(itransfer);
	struct windows_device_handle_priv *handle_priv = _device_handle_priv(
		transfer->dev_handle);
	WINUSB_SETUP_PACKET *setup = (WINUSB_SETUP_PACKET *) transfer->buffer;
	ULONG size;
	HANDLE winusb_handle;
	int current_interface;
	struct winfd wfd;

	transfer_priv->pollable_fd = INVALID_WINFD;
	size = transfer->length - LIBUSB_CONTROL_SETUP_SIZE;

	if (size > MAX_CTRL_BUFFER_LENGTH)
		return LIBUSB_ERROR_INVALID_PARAM;

	current_interface = get_valid_interface(transfer->dev_handle, USB_API_WINUSBX);
	if (current_interface < 0) {
		if (auto_claim(transfer, &current_interface, USB_API_WINUSBX) != LIBUSB_SUCCESS) {
			return LIBUSB_ERROR_NOT_FOUND;
		}
	}

	usbi_dbg("will use interface %d", current_interface);
	winusb_handle = handle_priv->interface_handle[current_interface].api_handle;

	wfd = usbi_create_fd(winusb_handle, RW_READ, NULL, NULL);
	// Always use the handle returned from usbi_create_fd (wfd.handle)
	if (wfd.fd < 0) {
		return LIBUSB_ERROR_NO_MEM;
	}

	// Sending of set configuration control requests from WinUSB creates issues
	if ( ((setup->RequestType & (0x03 << 5)) == LIBUSB_REQUEST_TYPE_STANDARD)
	  && (setup->Request == LIBUSB_REQUEST_SET_CONFIGURATION) ) {
		if (setup->Value != priv->active_config) {
			usbi_warn(ctx, "cannot set configuration other than the default one");
			usbi_free_fd(&wfd);
			return LIBUSB_ERROR_INVALID_PARAM;
		}
		wfd.overlapped->Internal = STATUS_COMPLETED_SYNCHRONOUSLY;
		wfd.overlapped->InternalHigh = 0;
	} else {
		if (!WinUsb_ControlTransfer(wfd.handle, *setup, transfer->buffer + LIBUSB_CONTROL_SETUP_SIZE, size, NULL, wfd.overlapped)) {
			if(GetLastError() != ERROR_IO_PENDING) {
				usbi_warn(ctx, "ControlTransfer failed: %s", windows_error_str(0));
				usbi_free_fd(&wfd);
				return LIBUSB_ERROR_IO;
			}
		} else {
			wfd.overlapped->Internal = STATUS_COMPLETED_SYNCHRONOUSLY;
			wfd.overlapped->InternalHigh = (DWORD)size;
		}
	}

	// Use priv_transfer to store data needed for async polling
	transfer_priv->pollable_fd = wfd;
	transfer_priv->interface_number = (uint8_t)current_interface;

	return LIBUSB_SUCCESS;
}

static int winusbx_set_interface_altsetting(int sub_api, struct libusb_device_handle *dev_handle, int iface, int altsetting)
{
	struct libusb_context *ctx = DEVICE_CTX(dev_handle->dev);
	struct windows_device_handle_priv *handle_priv = _device_handle_priv(dev_handle);
	struct windows_device_priv *priv = _device_priv(dev_handle->dev);
	HANDLE winusb_handle;

	if (altsetting > 255) {
		return LIBUSB_ERROR_INVALID_PARAM;
	}

	winusb_handle = handle_priv->interface_handle[iface].api_handle;
	if ((winusb_handle == 0) || (winusb_handle == INVALID_HANDLE_VALUE)) {
		usbi_err(ctx, "interface must be claimed first");
		return LIBUSB_ERROR_NOT_FOUND;
	}

	if (!WinUsb_SetCurrentAlternateSetting(winusb_handle, (UCHAR)altsetting)) {
		usbi_err(ctx, "SetCurrentAlternateSetting failed: %s", windows_error_str(0));
		return LIBUSB_ERROR_IO;
	}

	return LIBUSB_SUCCESS;
}

static int winusbx_submit_bulk_transfer(int sub_api, struct usbi_transfer *itransfer)
{
	struct libusb_transfer *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
	struct libusb_context *ctx = DEVICE_CTX(transfer->dev_handle->dev);
	struct windows_transfer_priv *transfer_priv = (struct windows_transfer_priv*)usbi_transfer_get_os_priv(itransfer);
	struct windows_device_handle_priv *handle_priv = _device_handle_priv(transfer->dev_handle);
	struct windows_device_priv *priv = _device_priv(transfer->dev_handle->dev);
	HANDLE winusb_handle;
	bool ret;
	int current_interface;
	struct winfd wfd;

	transfer_priv->pollable_fd = INVALID_WINFD;

	current_interface = interface_by_endpoint(priv, handle_priv, transfer->endpoint);
	if (current_interface < 0) {
		usbi_err(ctx, "unable to match endpoint to an open interface - cancelling transfer");
		return LIBUSB_ERROR_NOT_FOUND;
	}

	usbi_dbg("matched endpoint %02X with interface %d", transfer->endpoint, current_interface);

	winusb_handle = handle_priv->interface_handle[current_interface].api_handle;

	wfd = usbi_create_fd(winusb_handle, IS_XFERIN(transfer) ? RW_READ : RW_WRITE, NULL, NULL);
	// Always use the handle returned from usbi_create_fd (wfd.handle)
	if (wfd.fd < 0) {
		return LIBUSB_ERROR_NO_MEM;
	}

	if (IS_XFERIN(transfer)) {
		usbi_dbg("reading %d bytes", transfer->length);
		ret = WinUsb_ReadPipe(wfd.handle, transfer->endpoint, transfer->buffer, transfer->length, NULL, wfd.overlapped);
	} else {
		usbi_dbg("writing %d bytes", transfer->length);
		ret = WinUsb_WritePipe(wfd.handle, transfer->endpoint, transfer->buffer, transfer->length, NULL, wfd.overlapped);
	}
	if (!ret) {
		if(GetLastError() != ERROR_IO_PENDING) {
			usbi_err(ctx, "ReadPipe/WritePipe failed: %s", windows_error_str(0));
			usbi_free_fd(&wfd);
			return LIBUSB_ERROR_IO;
		}
	} else {
		wfd.overlapped->Internal = STATUS_COMPLETED_SYNCHRONOUSLY;
		wfd.overlapped->InternalHigh = (DWORD)transfer->length;
	}

	transfer_priv->pollable_fd = wfd;
	transfer_priv->interface_number = (uint8_t)current_interface;

	return LIBUSB_SUCCESS;
}

static int winusbx_clear_halt(int sub_api, struct libusb_device_handle *dev_handle, unsigned char endpoint)
{
	struct libusb_context *ctx = DEVICE_CTX(dev_handle->dev);
	struct windows_device_handle_priv *handle_priv = _device_handle_priv(dev_handle);
	struct windows_device_priv *priv = _device_priv(dev_handle->dev);
	HANDLE winusb_handle;
	int current_interface;

	current_interface = interface_by_endpoint(priv, handle_priv, endpoint);
	if (current_interface < 0) {
		usbi_err(ctx, "unable to match endpoint to an open interface - cannot clear");
		return LIBUSB_ERROR_NOT_FOUND;
	}

	usbi_dbg("matched endpoint %02X with interface %d", endpoint, current_interface);
	winusb_handle = handle_priv->interface_handle[current_interface].api_handle;

	if (!WinUsb_ResetPipe(winusb_handle, endpoint)) {
		usbi_err(ctx, "ResetPipe failed: %s", windows_error_str(0));
		return LIBUSB_ERROR_NO_DEVICE;
	}

	return LIBUSB_SUCCESS;
}

/*
 * from http://www.winvistatips.com/winusb-bugchecks-t335323.html (confirmed
 * through testing as well):
 * "You can not call WinUsb_AbortPipe on control pipe. You can possibly cancel
 * the control transfer using CancelIo"
 */
static int winusbx_abort_control(int sub_api, struct usbi_transfer *itransfer)
{
	// Cancelling of the I/O is done in the parent
	return LIBUSB_SUCCESS;
}

static int winusbx_abort_transfers(int sub_api, struct usbi_transfer *itransfer)
{
	struct libusb_transfer *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
	struct libusb_context *ctx = DEVICE_CTX(transfer->dev_handle->dev);
	struct windows_device_handle_priv *handle_priv = _device_handle_priv(transfer->dev_handle);
	struct windows_transfer_priv *transfer_priv = (struct windows_transfer_priv*)usbi_transfer_get_os_priv(itransfer);
	struct windows_device_priv *priv = _device_priv(transfer->dev_handle->dev);
	HANDLE winusb_handle;
	int current_interface;

	current_interface = transfer_priv->interface_number;
	if ((current_interface < 0) || (current_interface >= USB_MAXINTERFACES)) {
		usbi_err(ctx, "program assertion failed: invalid interface_number");
		return LIBUSB_ERROR_NOT_FOUND;
	}
	usbi_dbg("will use interface %d", current_interface);

	winusb_handle = handle_priv->interface_handle[current_interface].api_handle;

	if (!WinUsb_AbortPipe(winusb_handle, transfer->endpoint)) {
		usbi_err(ctx, "AbortPipe failed: %s", windows_error_str(0));
		return LIBUSB_ERROR_NO_DEVICE;
	}

	return LIBUSB_SUCCESS;
}

/*
 * from the "How to Use WinUSB to Communicate with a USB Device" Microsoft white paper
 * (http://www.microsoft.com/whdc/connect/usb/winusb_howto.mspx):
 * "WinUSB does not support host-initiated reset port and cycle port operations" and
 * IOCTL_INTERNAL_USB_CYCLE_PORT is only available in kernel mode and the
 * IOCTL_USB_HUB_CYCLE_PORT ioctl was removed from Vista => the best we can do is
 * cycle the pipes (and even then, the control pipe can not be reset using WinUSB)
 */
// TODO: (post hotplug): see if we can force eject the device and redetect it (reuse hotplug?)
static int winusbx_reset_device(int sub_api, struct libusb_device_handle *dev_handle)
{
	struct libusb_context *ctx = DEVICE_CTX(dev_handle->dev);
	struct windows_device_handle_priv *handle_priv = _device_handle_priv(dev_handle);
	struct windows_device_priv *priv = _device_priv(dev_handle->dev);
	struct winfd wfd;
	HANDLE winusb_handle;
	int i, j;

	// Reset any available pipe (except control)
	for (i=0; i<USB_MAXINTERFACES; i++) {
		winusb_handle = handle_priv->interface_handle[i].api_handle;
		for (wfd = handle_to_winfd(winusb_handle); wfd.fd > 0;)
		{
			// Cancel any pollable I/O
			usbi_remove_pollfd(ctx, wfd.fd);
			usbi_free_fd(&wfd);
			wfd = handle_to_winfd(winusb_handle);
		}

		if ( (winusb_handle != 0) && (winusb_handle != INVALID_HANDLE_VALUE)) {
			for (j=0; j<priv->usb_interface[i].nb_endpoints; j++) {
				usbi_dbg("resetting ep %02X", priv->usb_interface[i].endpoint[j]);
				if (!WinUsb_AbortPipe(winusb_handle, priv->usb_interface[i].endpoint[j])) {
					usbi_err(ctx, "AbortPipe (pipe address %02X) failed: %s",
						priv->usb_interface[i].endpoint[j], windows_error_str(0));
				}
				// FlushPipe seems to fail on OUT pipes
				if (IS_EPIN(priv->usb_interface[i].endpoint[j])
				  && (!WinUsb_FlushPipe(winusb_handle, priv->usb_interface[i].endpoint[j])) ) {
					usbi_err(ctx, "FlushPipe (pipe address %02X) failed: %s",
						priv->usb_interface[i].endpoint[j], windows_error_str(0));
				}
				if (!WinUsb_ResetPipe(winusb_handle, priv->usb_interface[i].endpoint[j])) {
					usbi_err(ctx, "ResetPipe (pipe address %02X) failed: %s",
						priv->usb_interface[i].endpoint[j], windows_error_str(0));
				}
			}
		}
	}

	return LIBUSB_SUCCESS;
}

static int winusbx_copy_transfer_data(int sub_api, struct usbi_transfer *itransfer, uint32_t io_size)
{
	itransfer->transferred += io_size;
	return LIBUSB_TRANSFER_COMPLETED;
}

/*
 * Composite API functions
 */
static int composite_init(int sub_api, struct libusb_context *ctx)
{
	return LIBUSB_SUCCESS;
}

static int composite_exit(int sub_api)
{
	return LIBUSB_SUCCESS;
}

static int composite_open(int sub_api, struct libusb_device_handle *dev_handle)
{
	struct windows_device_priv *priv = _device_priv(dev_handle->dev);
	int r = LIBUSB_ERROR_NOT_FOUND;
	uint8_t i;
	bool available[SUB_API_MAX] = {0};

	for (i=0; i<USB_MAXINTERFACES; i++) {
		switch (priv->usb_interface[i].apib->id) {
		case USB_API_WINUSBX:
			if (priv->usb_interface[i].sub_api != SUB_API_NOTSET)
				available[priv->usb_interface[i].sub_api] = true;
			break;
		default:
			break;
		}
	}

	for (i=0; i<SUB_API_MAX; i++) {	// WinUSB-like drivers
		if (available[i]) {
			r = usb_api_backend[USB_API_WINUSBX].open(i, dev_handle);
			if (r != LIBUSB_SUCCESS) {
				return r;
			}
		}
	}
	return r;
}

static void composite_close(int sub_api, struct libusb_device_handle *dev_handle)
{
	struct windows_device_priv *priv = _device_priv(dev_handle->dev);
	uint8_t i;
	bool available[SUB_API_MAX];

	for (i = 0; i<SUB_API_MAX; i++) {
		available[i] = false;
	}

	for (i=0; i<USB_MAXINTERFACES; i++) {
		if ( (priv->usb_interface[i].apib->id == USB_API_WINUSBX)
		  && (priv->usb_interface[i].sub_api != SUB_API_NOTSET) ) {
			available[priv->usb_interface[i].sub_api] = true;
		}
	}

	for (i=0; i<SUB_API_MAX; i++) {
		if (available[i]) {
			usb_api_backend[USB_API_WINUSBX].close(i, dev_handle);
		}
	}
}

static int composite_claim_interface(int sub_api, struct libusb_device_handle *dev_handle, int iface)
{
	struct windows_device_priv *priv = _device_priv(dev_handle->dev);
	return priv->usb_interface[iface].apib->
		claim_interface(priv->usb_interface[iface].sub_api, dev_handle, iface);
}

static int composite_set_interface_altsetting(int sub_api, struct libusb_device_handle *dev_handle, int iface, int altsetting)
{
	struct windows_device_priv *priv = _device_priv(dev_handle->dev);
	return priv->usb_interface[iface].apib->
		set_interface_altsetting(priv->usb_interface[iface].sub_api, dev_handle, iface, altsetting);
}

static int composite_release_interface(int sub_api, struct libusb_device_handle *dev_handle, int iface)
{
	struct windows_device_priv *priv = _device_priv(dev_handle->dev);
	return priv->usb_interface[iface].apib->
		release_interface(priv->usb_interface[iface].sub_api, dev_handle, iface);
}

static int composite_submit_control_transfer(int sub_api, struct usbi_transfer *itransfer)
{
	struct libusb_transfer *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
	struct libusb_context *ctx = DEVICE_CTX(transfer->dev_handle->dev);
	struct windows_device_priv *priv = _device_priv(transfer->dev_handle->dev);
	int i, pass;

	// Interface shouldn't matter for control, but it does in practice, with Windows'
	// restrictions with regards to accessing HID keyboards and mice. Try a 2 pass approach
	for (pass = 0; pass < 2; pass++) {
		for (i=0; i<USB_MAXINTERFACES; i++) {
			if (priv->usb_interface[i].path != NULL) {
				if ((pass == 0) && (priv->usb_interface[i].restricted_functionality)) {
					usbi_dbg("trying to skip restricted interface #%d (HID keyboard or mouse?)", i);
					continue;
				}
				usbi_dbg("using interface %d", i);
				return priv->usb_interface[i].apib->submit_control_transfer(priv->usb_interface[i].sub_api, itransfer);
			}
		}
	}

	usbi_err(ctx, "no libusbx supported interfaces to complete request");
	return LIBUSB_ERROR_NOT_FOUND;
}

static int composite_submit_bulk_transfer(int sub_api, struct usbi_transfer *itransfer) {
	struct libusb_transfer *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
	struct libusb_context *ctx = DEVICE_CTX(transfer->dev_handle->dev);
	struct windows_device_handle_priv *handle_priv = _device_handle_priv(transfer->dev_handle);
	struct windows_device_priv *priv = _device_priv(transfer->dev_handle->dev);
	int current_interface;

	current_interface = interface_by_endpoint(priv, handle_priv, transfer->endpoint);
	if (current_interface < 0) {
		usbi_err(ctx, "unable to match endpoint to an open interface - cancelling transfer");
		return LIBUSB_ERROR_NOT_FOUND;
	}

	return priv->usb_interface[current_interface].apib->
		submit_bulk_transfer(priv->usb_interface[current_interface].sub_api, itransfer);}

static int composite_submit_iso_transfer(int sub_api, struct usbi_transfer *itransfer) {
	struct libusb_transfer *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
	struct libusb_context *ctx = DEVICE_CTX(transfer->dev_handle->dev);
	struct windows_device_handle_priv *handle_priv = _device_handle_priv(transfer->dev_handle);
	struct windows_device_priv *priv = _device_priv(transfer->dev_handle->dev);
	int current_interface;

	current_interface = interface_by_endpoint(priv, handle_priv, transfer->endpoint);
	if (current_interface < 0) {
		usbi_err(ctx, "unable to match endpoint to an open interface - cancelling transfer");
		return LIBUSB_ERROR_NOT_FOUND;
	}

	return priv->usb_interface[current_interface].apib->
		submit_iso_transfer(priv->usb_interface[current_interface].sub_api, itransfer);}

static int composite_clear_halt(int sub_api, struct libusb_device_handle *dev_handle, unsigned char endpoint)
{
	struct libusb_context *ctx = DEVICE_CTX(dev_handle->dev);
	struct windows_device_handle_priv *handle_priv = _device_handle_priv(dev_handle);
	struct windows_device_priv *priv = _device_priv(dev_handle->dev);
	int current_interface;

	current_interface = interface_by_endpoint(priv, handle_priv, endpoint);
	if (current_interface < 0) {
		usbi_err(ctx, "unable to match endpoint to an open interface - cannot clear");
		return LIBUSB_ERROR_NOT_FOUND;
	}

	return priv->usb_interface[current_interface].apib->
		clear_halt(priv->usb_interface[current_interface].sub_api, dev_handle, endpoint);}

static int composite_abort_control(int sub_api, struct usbi_transfer *itransfer)
{
	struct libusb_transfer *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
	struct windows_transfer_priv *transfer_priv = usbi_transfer_get_os_priv(itransfer);
	struct windows_device_priv *priv = _device_priv(transfer->dev_handle->dev);

	return priv->usb_interface[transfer_priv->interface_number].apib->
		abort_control(priv->usb_interface[transfer_priv->interface_number].sub_api, itransfer);}

static int composite_abort_transfers(int sub_api, struct usbi_transfer *itransfer)
{
	struct libusb_transfer *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
	struct windows_transfer_priv *transfer_priv = usbi_transfer_get_os_priv(itransfer);
	struct windows_device_priv *priv = _device_priv(transfer->dev_handle->dev);

	return priv->usb_interface[transfer_priv->interface_number].apib->
		abort_transfers(priv->usb_interface[transfer_priv->interface_number].sub_api, itransfer);}

static int composite_reset_device(int sub_api, struct libusb_device_handle *dev_handle)
{
	struct windows_device_priv *priv = _device_priv(dev_handle->dev);
	int r;
	uint8_t i; 
	bool available[SUB_API_MAX];
	for (i = 0; i<SUB_API_MAX; i++) {
		available[i] = false;
	}
	for (i=0; i<USB_MAXINTERFACES; i++) {
		if ( (priv->usb_interface[i].apib->id == USB_API_WINUSBX)
		  && (priv->usb_interface[i].sub_api != SUB_API_NOTSET) ) {
			available[priv->usb_interface[i].sub_api] = true;
		}
	}
	for (i=0; i<SUB_API_MAX; i++) {
		if (available[i]) {
			r = usb_api_backend[USB_API_WINUSBX].reset_device(i, dev_handle);
			if (r != LIBUSB_SUCCESS) {
				return r;
			}
		}
	}
	return LIBUSB_SUCCESS;
}

static int composite_copy_transfer_data(int sub_api, struct usbi_transfer *itransfer, uint32_t io_size)
{
	struct libusb_transfer *transfer = USBI_TRANSFER_TO_LIBUSB_TRANSFER(itransfer);
	struct windows_transfer_priv *transfer_priv = usbi_transfer_get_os_priv(itransfer);
	struct windows_device_priv *priv = _device_priv(transfer->dev_handle->dev);

	return priv->usb_interface[transfer_priv->interface_number].apib->
		copy_transfer_data(priv->usb_interface[transfer_priv->interface_number].sub_api, itransfer, io_size);
}
