#ifndef _NTOS_H
#define _NTOS_H
/* $Id: ntos.h,v 1.8 2002/11/24 18:24:51 robd Exp $ */

#if defined(NTOS_MODE_USER)
// include windows.h before ntddk.h to get user mode prototype for InterlockedXxx functions
#include <windows.h>
#include <ddk/ntddk.h>
#include <ddk/ntifs.h>
#include "ntos/types.h"
#include "ntos/cdrom.h"
#include "ntos/console.h"
#include "ntos/disk.h"
#include "ntos/except.h"
#include "ntos/file.h"
#include "ntos/gditypes.h"
#include "ntos/heap.h"
#include "ntos/kdbgsyms.h"
#include "ntos/keyboard.h"
#include "ntos/minmax.h"
#include "ntos/mm.h"
#include "ntos/ntdef.h"
#include "ntos/port.h"
#include "ntos/ps.h"
#include "ntos/registry.h"
#include "ntos/security.h"
#include "ntos/synch.h"
#include "ntos/time.h"
#include "napi/i386/segment.h"
#include "napi/types.h"
#include "napi/core.h"
#include "napi/dbg.h"
#include "napi/lpc.h"
#include "napi/npipe.h"
#include "napi/shared_data.h"
#include "napi/win32.h"
#include "ntos/rtltypes.h"
#include "ntos/rtl.h"
#include "ntos/zwtypes.h"
#include "ntos/zw.h"
#include "ntdll/csr.h"
#include "ntdll/dbg.h"
#include "ntdll/ldr.h"
#include "ntdll/registry.h"
#include "ntdll/rtl.h"
#include "ntdll/trace.h"
#include "kernel32/error.h"
#else // Assume kernel mode
#include <ddk/ntddk.h>
//#include "ntos/types.h" // robd
#include <ddk/ntifs.h>
#include "ntos/types.h"
#include "ntos/cdrom.h"
#include "ntos/console.h"
#include "ntos/disk.h"
#include "ntos/except.h"
#include "ntos/file.h"
#include "ntos/gditypes.h"
#include "ntos/heap.h"
#include "ntos/kdbgsyms.h"
#include "ntos/keyboard.h"
#include "ntos/minmax.h"
#include "ntos/mm.h"
#include "ntos/ntdef.h"
#include "ntos/port.h"
#include "ntos/ps.h"
#include "ntos/registry.h"
#include "ntos/security.h"
#include "ntos/synch.h"
#include "ntos/time.h"
#include "napi/i386/segment.h"
#include "napi/types.h"
#include "napi/core.h"
#include "napi/dbg.h"
#include "napi/lpc.h"
#include "napi/npipe.h"
#include "napi/shared_data.h"
#include "napi/win32.h"
#include "ntos/rtltypes.h"
#include "ntos/rtl.h"
#include "ntos/zwtypes.h"
#include "ntos/zw.h"
#endif

#endif /* ndef _NTOS_H */
