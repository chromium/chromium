#ifndef _RAR_RESOURCE_
#define _RAR_RESOURCE_

#ifdef RARDLL
#define St(x) (L"")
#else
const wchar *St(MSGID StringId);
#endif


#endif
