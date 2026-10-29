
#include <TiltedOnlineApp.h>
#include <TiltedOnlinePCH.h>

#include <Commctrl.h>
#include <Windows.h>

#include <base/dialogues/win/TaskDialog.h>

std::unique_ptr<TiltedOnlineApp> g_appInstance{nullptr};

extern HICON g_SharedWindowIcon;

static void ShowAddressLibraryError(const wchar_t* apGamePath)
{
#if TP_SKYRIM64
    auto errorDetail = fmt::format(L"Looking for it here: {}\\Data\\SKSE\\Plugins", apGamePath);

    Base::TaskDialog dia(g_SharedWindowIcon, L"Error", L"Failed to load Skyrim Address Library", L"Make sure to use \"All in one (1.6.X)\"", errorDetail.c_str());
#elif TP_FALLOUT4
    auto errorDetail = fmt::format(L"Looking for it here: {}\\Data\\F4SE\\Plugins", apGamePath);

    Base::TaskDialog dia(g_SharedWindowIcon, L"Error", L"Failed to load Fallout 4 Address Library", L"", errorDetail.c_str());
#endif

    dia.AppendButton(0xBEED, L"Visit troubleshooting page on wiki.tiltedphoques.com");
    dia.AppendButton(0xBEEF, L"Visit Address Library modpage on nexusmods.com");
    const int result = dia.Show();
    if (result == 0xBEEF)
    {
#if TP_SKYRIM64
        ShellExecuteW(nullptr, L"open", LR"(https://www.nexusmods.com/skyrimspecialedition/mods/32444?tab=files)", nullptr, nullptr, SW_SHOWNORMAL);
#elif TP_FALLOUT4
        ShellExecuteW(nullptr, L"open", LR"(https://www.nexusmods.com/fallout4/mods/47327)", nullptr, nullptr, SW_SHOWNORMAL);
#endif
    }
    else if (result == 0xBEED)
    {
        ShellExecuteW(nullptr, L"open", LR"(https://wiki.tiltedphoques.com/tilted-online/guides/troubleshooting/address-library-error)", nullptr, nullptr, SW_SHOWNORMAL);
    }

    exit(4);
}

void RunTiltedInit(const std::filesystem::path& acGamePath, const String& aExeVersion)
{
#if TP_SKYRIM64
    if (!VersionDb::Get().Load(acGamePath, aExeVersion))
#else
    if (!VersionDb::Get().LoadFallout4(acGamePath, aExeVersion))
#endif
    {
        ShowAddressLibraryError(acGamePath.c_str());
    }

#if TP_FALLOUT4
    // Report unresolved IDs up front instead of crashing on the first null pointer.
    if (const auto missing = VersionDb::Get().FindMissingIds(); !missing.empty())
    {
        wchar_t modulePath[MAX_PATH]{};
        GetModuleFileNameW(nullptr, modulePath, MAX_PATH);
        const auto logDir = std::filesystem::path(modulePath).parent_path() / "logs";
        std::filesystem::create_directories(logDir);
        const auto reportPath = logDir / fmt::format("missing-offsets-{}.txt", aExeVersion.c_str());
        {
            std::ofstream report(reportPath);
            for (const auto id : missing)
                report << id << '\n';
        }

        if (!GetEnvironmentVariableW(L"TP_ALLOW_MISSING_OFFSETS", nullptr, 0))
        {
            const auto detail = fmt::format(L"{} of {} game addresses are unknown for Fallout 4 {}. List: {}", missing.size(), VersionDb::RequestedIds().size(),
                                            std::wstring(aExeVersion.begin(), aExeVersion.end()), reportPath.wstring());
            Base::TaskDialog dia(g_SharedWindowIcon, L"Error", L"This Fallout 4 version is not supported yet", L"", detail.c_str());
            dia.Show();
            exit(5);
        }
    }
#endif

    // VersionDb::Get().DumpToTextFile(R"(S:\Work\Tilted\fallout\_addresslib.txt)");

    g_appInstance = std::make_unique<TiltedOnlineApp>();

    TiltedOnlineApp::InstallHooks2();
    TP_HOOK_COMMIT;
}

void RunTiltedApp()
{
    g_appInstance->BeginMain();
}
