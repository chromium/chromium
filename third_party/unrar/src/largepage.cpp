#include "rar.hpp"

/*
To enable, disable or check Large Memory pages manually:
- open "Local Security Policy" from "Start Menu";
- open "Lock Pages in Memory" in "Local Policies\User Rights Assignment";
- add or remove the user and sign out and sign in or restart Windows.
*/

#if defined(_WIN_ALL) && !defined(SFX_MODULE) && !defined(RARDLL)
#define ALLOW_LARGE_PAGES
#endif

LargePageAlloc::LargePageAlloc()
{
  UseLargePages=false;
#ifdef ALLOW_LARGE_PAGES
  PageSize=0;
#endif
}


void LargePageAlloc::AllowLargePages(bool Allow)
{
#ifdef ALLOW_LARGE_PAGES
  if (Allow && PageSize==0)
  {
    HMODULE hKernel=GetModuleHandle(L"kernel32.dll");
    if (hKernel!=nullptr)
    {
      typedef SIZE_T (*GETLARGEPAGEMINIMUM)();
      GETLARGEPAGEMINIMUM pGetLargePageMinimum=(GETLARGEPAGEMINIMUM)GetProcAddress(hKernel, "GetLargePageMinimum");
      if (pGetLargePageMinimum!=nullptr)
        PageSize=pGetLargePageMinimum();
    }
    if (PageSize==0 || !SetPrivilege(SE_LOCK_MEMORY_NAME))
    {
      UseLargePages=false;
      return;
    }
  }

  UseLargePages=Allow;
#endif
}


bool LargePageAlloc::IsPrivilegeAssigned()
{
#ifdef ALLOW_LARGE_PAGES
  return SetPrivilege(SE_LOCK_MEMORY_NAME);
#else
  return true;
#endif
}


bool LargePageAlloc::AssignPrivilege()
{
#ifdef ALLOW_LARGE_PAGES
  HANDLE hToken = NULL;

  if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken))
    return false;

  // Get the required buffer size.
  DWORD BufSize=0;
  GetTokenInformation(hToken, TokenUser, NULL, 0, &BufSize);
  if (BufSize==0 || BufSize>1000000) // Sanity check for returned value.
  {
    CloseHandle(hToken);
    return false;
  }

  TOKEN_USER *TokenInfo = (TOKEN_USER*)malloc(BufSize);

  // Get the current user token information.
  if (GetTokenInformation(hToken,TokenUser,TokenInfo,BufSize,&BufSize)==0)
  {
    CloseHandle(hToken);
    return false;
  }

  // Get SID string for the current user.
  LPWSTR ApiSidStr;
  ConvertSidToStringSid(TokenInfo->User.Sid, &ApiSidStr);

  // Convert SID to C++ string and release API based buffer.
  std::wstring SidStr=ApiSidStr;
  LocalFree(ApiSidStr);
  CloseHandle(hToken);

  if (IsUserAdmin())
    AssignPrivilegeBySid(SidStr);
  else
  {
    // Define here, so they survive until ShellExecuteEx call.
    std::wstring ExeName=GetModuleFileStr();
    std::wstring Param=std::wstring(L"-") + LOCKMEM_SWITCH + SidStr;

    SHELLEXECUTEINFO shExecInfo{};
    shExecInfo.cbSize = sizeof(shExecInfo);

    shExecInfo.hwnd = NULL; // Specifying WinRAR main window here does not work well in command line mode.
    shExecInfo.lpVerb = L"runas";
    shExecInfo.lpFile = ExeName.c_str();
    shExecInfo.lpParameters = Param.c_str();
    shExecInfo.nShow = SW_SHOWNORMAL;
    BOOL Result=ShellExecuteEx(&shExecInfo);
  }
#endif

  return true;
}


bool LargePageAlloc::AssignPrivilegeBySid(const std::wstring &Sid)
{
#ifdef ALLOW_LARGE_PAGES
  LSA_HANDLE PolicyHandle;
  LSA_OBJECT_ATTRIBUTES ObjectAttributes{}; // Docs require to zero initalize it.

#ifndef STATUS_SUCCESS // Can be defined through WIL package in WinRAR.
  // We define STATUS_SUCCESS here instead of including ntstatus.h to avoid
  // macro redefinition warnings. We tried UMDF_USING_NTSTATUS define
  // and other workarounds, but it didn't help.
  const uint STATUS_SUCCESS=0;
#endif

  if (LsaOpenPolicy(NULL,&ObjectAttributes,POLICY_CREATE_ACCOUNT|
                    POLICY_LOOKUP_NAMES,&PolicyHandle)!=STATUS_SUCCESS)
    return false;

  PSID UserSid;
  ConvertStringSidToSid(Sid.c_str(),&UserSid);

  LSA_UNICODE_STRING LsaString;
  LsaString.Buffer=(PWSTR)SE_LOCK_MEMORY_NAME;
  // It must be in bytes, so multiple it to sizeof(wchar_t).
  LsaString.Length=(USHORT)wcslen(LsaString.Buffer)*sizeof(LsaString.Buffer[0]);
  LsaString.MaximumLength=LsaString.Length;

  bool Success=LsaAddAccountRights(PolicyHandle,UserSid,&LsaString,1)==STATUS_SUCCESS;

  LocalFree(UserSid);
  LsaClose(PolicyHandle);

  mprintf(St(MPrivilegeAssigned));
  if (Ask(St(MYesNo)) == 1)
    Shutdown(POWERMODE_RESTART);

  return Success;
#else
  return true;
#endif
}


bool LargePageAlloc::AssignConfirmation()
{
#ifdef ALLOW_LARGE_PAGES
  mprintf(St(MLockInMemoryNeeded));
  return Ask(St(MYesNo)) == 1;
#else
  return false;
#endif
}


void* LargePageAlloc::new_large(size_t Size)
{
  void *Allocated=nullptr;

#ifdef ALLOW_LARGE_PAGES
  if (UseLargePages && Size>=PageSize)
  {
    // VirtualAlloc fails if allocation size isn't multiple of page size.
    SIZE_T AllocSize=Size%PageSize==0 ? Size:(Size/PageSize+1)*PageSize;
    Allocated=VirtualAlloc(nullptr,AllocSize,MEM_COMMIT|MEM_RESERVE|MEM_LARGE_PAGES,PAGE_READWRITE);
    if (Allocated!=nullptr)
      LargeAlloc.push_back(Allocated);
  }
#endif
  return Allocated;
}


bool LargePageAlloc::delete_large(void *Addr)
{
#ifdef ALLOW_LARGE_PAGES
  if (Addr!=nullptr)
    for (size_t I=0;I<LargeAlloc.size();I++)
      if (LargeAlloc[I]==Addr)
      {
        LargeAlloc[I]=nullptr;
        VirtualFree(Addr,0,MEM_RELEASE);
        return true;
      }
#endif
  return false;
}
