#include "app/Platform.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <shellapi.h>
#include <cstdint>
#endif
namespace lf::platform {
bool openCreatorProfile(){
#ifdef _WIN32
    return reinterpret_cast<std::intptr_t>(ShellExecuteW(nullptr,L"open",L"https://github.com/itsedeyad",nullptr,nullptr,SW_SHOWNORMAL))>32;
#else
    return false;
#endif
}
}
