#include <TiltedOnlinePCH.h>
#include "TiltedOnlineApp.h"

#if TP_FALLOUT4

#include <GameVM.h>

extern std::unique_ptr<TiltedOnlineApp> g_appInstance;

struct Main;
struct VMContext
{
    bool IsInactive() const noexcept
    {
        // next-gen moved the flag by 0x10
        const auto offset = VersionDb::Get().IsFallout4NextGen() ? 0x610 : 0x600;
        return reinterpret_cast<const uint8_t*>(this)[offset] != 0;
    }
};

TP_THIS_FUNCTION(TVMUpdate, int, VMContext);
TP_THIS_FUNCTION(TMainLoop, short, Main);
TP_THIS_FUNCTION(TVMDestructor, uintptr_t, void);
using TMainShutdown = void();

static TVMUpdate* VMUpdate = nullptr;
static TMainLoop* MainLoop = nullptr;
static TVMDestructor* VMDestructor = nullptr;

int TP_MAKE_THISCALL(HookVMUpdate, VMContext)
{
    if (!apThis->IsInactive())
        g_appInstance->Update();

    return TiltedPhoques::ThisCall(VMUpdate, apThis);
}

short TP_MAKE_THISCALL(HookMainLoop, Main)
{
    TP_EMPTY_HOOK_PLACEHOLDER;

    return TiltedPhoques::ThisCall(MainLoop, apThis);
}

uintptr_t TP_MAKE_THISCALL(HookVMDestructor, void)
{
    TP_EMPTY_HOOK_PLACEHOLDER

    return TiltedPhoques::ThisCall(VMDestructor, apThis);
}

// The engine's teardown crashes (next-gen) or hangs (1.10.163) with the client attached, and there is nothing left
// to save at this point, so leave right away.
void HookMainShutdown()
{
    spdlog::info("Game is shutting down, skipping engine teardown");
    TerminateProcess(GetCurrentProcess(), 0);
}

// Lets the launcher open a save right after the main menu. The game runs this setting as a console command and
// stays in the menu if the load fails, which is why no engine address is needed for this.
using TGetPrivateProfileStringA = decltype(&GetPrivateProfileStringA);
static TGetPrivateProfileStringA RealGetPrivateProfileStringA = nullptr;

DWORD WINAPI HookGetPrivateProfileStringA(LPCSTR apSection, LPCSTR apKey, LPCSTR apDefault, LPSTR apResult, DWORD aSize, LPCSTR apFile)
{
    char save[MAX_PATH]{};
    if (apKey && _stricmp(apKey, "sStartingConsoleCommand") == 0 && GetEnvironmentVariableA("TP_AUTOLOAD", save, static_cast<DWORD>(std::size(save))))
    {
        const int cWritten = snprintf(apResult, aSize, "LoadGame \"%s\"", save);
        if (cWritten > 0 && static_cast<DWORD>(cWritten) < aSize)
        {
            spdlog::info("Loading save on start: {}", save);
            return static_cast<DWORD>(cWritten);
        }
    }

    return RealGetPrivateProfileStringA(apSection, apKey, apDefault, apResult, aSize, apFile);
}

static TiltedPhoques::Initializer s_mainHooks(
    []()
    {
        POINTER_FALLOUT4(TMainLoop, cMainLoop, 633524);
        POINTER_FALLOUT4(TVMUpdate, cVMUpdate, 759508);
        POINTER_FALLOUT4(TVMDestructor, cVMDestructor, 694340);
        POINTER_FALLOUT4(TMainShutdown, cMainShutdown, 234348);

        VMUpdate = cVMUpdate.Get();
        MainLoop = cMainLoop.Get();
        VMDestructor = cVMDestructor.Get();

        TP_HOOK(&VMUpdate, HookVMUpdate);
        TP_HOOK(&MainLoop, HookMainLoop);
        TP_HOOK(&VMDestructor, HookVMDestructor);

        TP_HOOK_IAT(GetPrivateProfileStringA, "kernel32.dll");

        static TMainShutdown* s_mainShutdown = cMainShutdown.Get();
        TP_HOOK(&s_mainShutdown, HookMainShutdown);
    });

#endif
