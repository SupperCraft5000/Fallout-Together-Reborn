#include <BranchInfo.h>

#include <Havok/hkbStateMachine.h>
#include <Structs/AnimationGraphDescriptorManager.h>

#include <Havok/BShkbAnimationGraph.h>
#include <Havok/BShkbHkxDB.h>
#include <Havok/hkbBehaviorGraph.h>

#include <Services/ImguiService.h>
#include <Services/DebugService.h>
#include <Services/TransportService.h>
#include <Services/PapyrusService.h>
#include <Services/QuestService.h>

#include <Events/UpdateEvent.h>
#include <Events/DialogueEvent.h>
#include <Events/SubtitleEvent.h>
#include <Events/MoveActorEvent.h>
#include <Events/ConnectionErrorEvent.h>

#include <Games/References.h>

#include <BSAnimationGraphManager.h>
#include <Forms/TESFaction.h>
#include <Forms/TESQuest.h>

#include <Forms/BGSAction.h>
#include <Forms/TESIdleForm.h>
#include <Forms/TESNPC.h>
#include <Games/Animation/ActorMediator.h>
#include <Games/Animation/TESActionData.h>
#include <Magic/ActorMagicCaster.h>
#include <Misc/BSFixedString.h>
#include <Structs/ActionEvent.h>

#include <Components.h>
#include <World.h>

#include <Forms/TESObjectCELL.h>
#include <Forms/TESWorldSpace.h>
#include <Games/TES.h>

#include <AI/AIProcess.h>

#include <Messages/RequestRespawn.h>

#include <Games/Misc/SubtitleManager.h>
#include <Games/Overrides.h>
#include <OverlayApp.hpp>
#include <filesystem>
#include <fstream>
#include <Structs/ActorValues.h>
#include <Structs/Inventory.h>

#include <EquipManager.h>
#include <Forms/TESAmmo.h>
#include <BSGraphics/BSGraphicsRenderer.h>
#include <Interface/UI.h>

// TODO: ft
#if TP_SKYRIM64
#include <Combat/CombatController.h>
#include <Camera/PlayerCamera.h>
#include <AI/Movement/PlayerControls.h>
#include <Interface/IMenu.h>
#include <Camera/PlayerCamera.h>
#include <DefaultObjectManager.h>
#include <Misc/InventoryEntry.h>
#include <Misc/MiddleProcess.h>
#endif

#include <imgui.h>
#include <inttypes.h>
extern thread_local bool g_overrideFormId;

constexpr char kBuildTag[] = "Build: " BUILD_COMMIT " " BUILD_BRANCH " EVO\nBuilt: " __TIMESTAMP__;
static void DrawBuildTag()
{
    auto* pWindow = BSGraphics::GetMainWindow();
    const ImVec2 coord{50.f, static_cast<float>((pWindow->uiWindowHeight + 25) - 100)};
    ImGui::GetBackgroundDrawList()->AddText(ImGui::GetFont(), ImGui::GetFontSize(), coord, ImColor::ImColor(255.f, 0.f, 0.f), kBuildTag);
}

void __declspec(noinline) DebugService::PlaceActorInWorld() noexcept
{
    if (m_actors.size())
        return;

    const auto pPlayerBaseForm = static_cast<TESNPC*>(PlayerCharacter::Get()->baseForm);

    auto pActor = Actor::Create(pPlayerBaseForm);

    const Inventory inventory = PlayerCharacter::Get()->GetActorInventory();
    pActor->SetActorInventory(inventory);

    pActor->GetExtension()->SetPlayer(true);

    m_actors.emplace_back(pActor);
}

DebugService::DebugService(entt::dispatcher& aDispatcher, World& aWorld, TransportService& aTransport, ImguiService& aImguiService)
    : m_dispatcher(aDispatcher)
    , m_transport(aTransport)
    , m_world(aWorld)
{
    m_updateConnection = m_dispatcher.sink<UpdateEvent>().connect<&DebugService::OnUpdate>(this);
    m_drawImGuiConnection = aImguiService.OnDraw.connect<&DebugService::OnDraw>(this);
    m_dialogueConnection = m_dispatcher.sink<DialogueEvent>().connect<&DebugService::OnDialogue>(this);
    m_dispatcher.sink<SubtitleEvent>().connect<&DebugService::OnSubtitle>(this);
    m_dispatcher.sink<MoveActorEvent>().connect<&DebugService::OnMoveActor>(this);
}

void DebugService::OnDialogue(const DialogueEvent& acEvent) noexcept
{
    if (ActorID)
        return;
    ActorID = acEvent.ActorID;
    VoiceFile = acEvent.VoiceFile;
}

void DebugService::OnSubtitle(const SubtitleEvent& acEvent) noexcept
{
    if (SubActorID)
        return;
    SubActorID = acEvent.SpeakerID;
    SubtitleText = acEvent.Text;
    TopicID = acEvent.TopicFormID;
}

// TODO: yeah, i'm aware of how dumb this looks, but things crash if
// you do it directly by adding an event to the queue, no symbols for tiltedcore when debugging,
// so this'll do for now
struct MoveData
{
    Actor* pActor = nullptr;
    TESObjectCELL* pCell = nullptr;
    NiPoint3 position;
} moveData;

void DebugService::OnMoveActor(const MoveActorEvent& acEvent) noexcept
{
    Actor* pActor = Cast<Actor>(TESForm::GetById(acEvent.FormId));
    TESObjectCELL* pCell = Cast<TESObjectCELL>(TESForm::GetById(acEvent.CellId));

    if (!pActor || !pCell)
        return;

    moveData.pActor = pActor;
    moveData.pCell = pCell;
    moveData.position = acEvent.Position;
}

extern thread_local bool g_forceAnimation;

namespace
{
constexpr uint32_t kFigureValues[]{ActorValueInfo::kStrength,     ActorValueInfo::kPerception, ActorValueInfo::kEndurance, ActorValueInfo::kCharisma,
                                   ActorValueInfo::kIntelligence, ActorValueInfo::kAgility,    ActorValueInfo::kLuck};

void SaveFigure(PlayerCharacter* apPlayer, const std::string& acPath)
{
    ActorValues values;
    for (const uint32_t id : kFigureValues)
        values.ActorValuesList.insert({id, apPlayer->GetActorValue(id)});

    Buffer buffer(1 << 22);
    Buffer::Writer writer(&buffer);
    apPlayer->GetActorInventory().Serialize(writer);
    values.Serialize(writer);

    if (auto* pNpc = Cast<TESNPC>(apPlayer->baseForm))
    {
        // same flags as a player spawn, these carry the face
        pNpc->MarkChanged(0x2000800);
        const uint32_t changeFlags = pNpc->GetChangeFlags();
        String appearance;
        pNpc->Serialize(&appearance);
        const std::string name = pNpc->fullName.value.AsAscii();

        const uint32_t appearanceSize = static_cast<uint32_t>(appearance.size());
        const uint32_t nameSize = static_cast<uint32_t>(name.size());
        writer.WriteBytes(reinterpret_cast<const uint8_t*>(&changeFlags), sizeof(changeFlags));
        writer.WriteBytes(reinterpret_cast<const uint8_t*>(&appearanceSize), sizeof(appearanceSize));
        writer.WriteBytes(reinterpret_cast<const uint8_t*>(appearance.data()), appearanceSize);
        writer.WriteBytes(reinterpret_cast<const uint8_t*>(&nameSize), sizeof(nameSize));
        writer.WriteBytes(reinterpret_cast<const uint8_t*>(name.data()), nameSize);
    }

    const std::string temp = acPath + ".tmp";
    {
        std::ofstream file(temp, std::ios::binary | std::ios::trunc);
        file.write(reinterpret_cast<const char*>(buffer.GetData()), static_cast<std::streamsize>(writer.Size()));
    }
    std::error_code error;
    std::filesystem::rename(temp, acPath, error);
    if (error)
        spdlog::error("Could not save figure: {}", error.message());
}

void ApplyFigure(PlayerCharacter* apPlayer, const std::string& acPath)
{
    std::ifstream file(acPath, std::ios::binary);
    const std::string data((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    if (data.empty())
    {
        spdlog::warn("No figure found at {}", acPath);
        return;
    }

    Buffer buffer(reinterpret_cast<const uint8_t*>(data.data()), data.size());
    Buffer::Reader reader(&buffer);
    Inventory inventory{};
    ActorValues values{};
    inventory.Deserialize(reader);
    values.Deserialize(reader);

    // figures written by older builds end here
    uint32_t changeFlags = 0;
    uint32_t appearanceSize = 0;
    if (reader.ReadBytes(reinterpret_cast<uint8_t*>(&changeFlags), sizeof(changeFlags)) &&
        reader.ReadBytes(reinterpret_cast<uint8_t*>(&appearanceSize), sizeof(appearanceSize)) && appearanceSize > 0 &&
        reader.GetBytePosition() + appearanceSize <= data.size())
    {
        String appearance(appearanceSize, '\0');
        reader.ReadBytes(reinterpret_cast<uint8_t*>(appearance.data()), appearanceSize);

        uint32_t nameSize = 0;
        reader.ReadBytes(reinterpret_cast<uint8_t*>(&nameSize), sizeof(nameSize));
        std::string name(nameSize, '\0');
        reader.ReadBytes(reinterpret_cast<uint8_t*>(name.data()), nameSize);

        if (auto* pNpc = Cast<TESNPC>(apPlayer->baseForm))
        {
            pNpc->Deserialize(appearance, changeFlags);
            if (!name.empty())
                pNpc->fullName.value = BSFixedString(name.c_str());
        }
    }

    apPlayer->SetActorInventory(inventory);
    for (const auto& value : values.ActorValuesList)
        apPlayer->SetActorValue(value.first, value.second);
    apPlayer->QueueUpdate();

    spdlog::info("Applied figure: {} items, {} values", inventory.Entries.size(), values.ActorValuesList.size());
}

// The ids in a figure are server mod ids, so reading and writing has to wait until the server assigned them.
// Returns true once a figure was applied.
bool UpdateFigure(const bool acOnline, const double acDelta)
{
    static double s_wait = 10.0;
    static bool s_finished = false;
    if (s_finished || !acOnline)
        return false;

    s_wait -= acDelta;
    if (s_wait > 0.0)
        return false;

    char mode[16]{};
    char path[MAX_PATH]{};
    if (!GetEnvironmentVariableA("TP_FIGURE_MODE", mode, static_cast<DWORD>(std::size(mode))) ||
        !GetEnvironmentVariableA("TP_FIGURE_FILE", path, static_cast<DWORD>(std::size(path))))
    {
        s_finished = true;
        return false;
    }

    PlayerCharacter* pPlayer = PlayerCharacter::Get();
    if (!pPlayer)
        return false;

    if (strcmp(mode, "apply") == 0)
    {
        s_finished = true;
        ApplyFigure(pPlayer, path);
        return true;
    }

    SaveFigure(pPlayer, path);

    // the launcher starts the game a second time with the host's save
    if (strcmp(mode, "capture") == 0)
    {
        s_finished = true;
        PostMessageW(reinterpret_cast<HWND>(BSGraphics::GetMainWindow()->hWnd), WM_CLOSE, 0, 0);
    }
    s_wait = 60.0;
    return false;
}

// Only for sessions started by the launcher, where nobody could reconnect by hand.
void UpdateHostLost(const bool acOnline, bool& arExpected)
{
    static bool s_wasOnline = false;
    if (acOnline)
    {
        s_wasOnline = true;
        return;
    }
    if (!s_wasOnline)
        return;
    s_wasOnline = false;

    char address[8]{};
    if (arExpected || !GetEnvironmentVariableA("TP_AUTOCONNECT", address, static_cast<DWORD>(std::size(address))))
    {
        arExpected = false;
        return;
    }

    HWND window = reinterpret_cast<HWND>(BSGraphics::GetMainWindow()->hWnd);
    MessageBoxW(window, L"Die Verbindung zum Host wurde getrennt.\nDas Spiel wird jetzt beendet.", L"Fallout Together Reborn", MB_OK | MB_ICONINFORMATION | MB_TOPMOST);
    PostMessageW(window, WM_CLOSE, 0, 0);
}
} // namespace

void DebugService::OnUpdate(const UpdateEvent& acUpdateEvent) noexcept
{
    // Automated tests: TP_AUTOCONNECT=host:port (and optionally TP_PASSWORD) connects once the player is in a cell,
    // independent of window focus so several instances can run side by side.
    static bool s_autoConnectChecked = false;
    if (!s_autoConnectChecked)
    {
        char address[256]{};
        if (!GetEnvironmentVariableA("TP_AUTOCONNECT", address, static_cast<DWORD>(std::size(address))))
        {
            s_autoConnectChecked = true;
        }
        else if (PlayerCharacter* pPlayer = PlayerCharacter::Get(); pPlayer && pPlayer->parentCell)
        {
            s_autoConnectChecked = true;
            char password[256]{};
            GetEnvironmentVariableA("TP_PASSWORD", password, static_cast<DWORD>(std::size(password)));
            spdlog::info("Auto connect to {}", address);
            m_transport.SetServerPassword(password);
            m_transport.Connect(address);
        }
    }

    // the look only reaches the others with the spawn request, so join again with the new one
    static bool s_reconnecting = false;
    if (UpdateFigure(m_transport.IsOnline(), acUpdateEvent.Delta))
    {
        s_reconnecting = true;
        m_transport.Close();
        s_autoConnectChecked = false;
    }
    UpdateHostLost(m_transport.IsOnline(), s_reconnecting);

    if (!BSGraphics::GetMainWindow()->IsForeground())
        return;

    if (moveData.pActor)
    {
        moveData.pActor->MoveTo(moveData.pCell, moveData.position);
        moveData.pActor = nullptr;
    }

    static std::atomic<bool> s_f8Pressed = false;
    static std::atomic<bool> s_f7Pressed = false;
    static std::atomic<bool> s_f6Pressed = false;

    if (GetAsyncKeyState(VK_F3) & 0x01)
    {
        m_showDebugStuff = !m_showDebugStuff;
    }

#if (!IS_MASTER)
    if (GetAsyncKeyState(VK_F6))
    {
        if (!s_f6Pressed)
        {
            s_f6Pressed = true;

            static char s_address[256] = "127.0.0.1:10578";
            if (!m_transport.IsOnline())
                m_transport.Connect(s_address);
            else
                m_transport.Close();
        }
    }
    else
        s_f6Pressed = false;

#if 0
    if (GetAsyncKeyState(VK_F10) & 0x8000)
    {
        UI::Get()->PrintActiveMenus();
    }
#endif

    if (GetAsyncKeyState(VK_F7))
    {
        if (!s_f7Pressed)
        {
            s_f7Pressed = true;

            //
        }
    }
    else
        s_f7Pressed = false;

    if (GetAsyncKeyState(VK_F8) & 0x01)
    {
        if (!s_f8Pressed)
        {
            s_f8Pressed = true;

            //PlaceActorInWorld();
        }
    }
    else
        s_f8Pressed = false;
#endif
}

static bool g_enableServerWindow{false};
static bool g_enableAnimWindow{false};
static bool g_enableEntitiesWindow{false};
static bool g_enableInventoryWindow{false};
static bool g_enableNetworkWindow{false};
static bool g_enableFormsWindow{false};
static bool g_enablePlayerWindow{false};
static bool g_enableSkillsWindow{false};
static bool g_enablePartyWindow{false};
static bool g_enableActorValuesWindow{false};
static bool g_enableQuestWindow{false};
static bool g_enableCellWindow{false};
static bool g_enableProcessesWindow{false};
static bool g_enableWeatherWindow{false};
static bool g_enableCombatWindow{false};
static bool g_enableCalendarWindow{false};
static bool g_enableDragonSpawnerWindow{false};

void DebugService::DrawServerView() noexcept
{
    ImGui::SetNextWindowSize(ImVec2(250, 440), ImGuiCond_FirstUseEver);
    ImGui::Begin("Server");

    static char s_address[1024] = "127.0.0.1:10578";
    static char s_password[1024] = "";
    ImGui::InputText("Address", s_address, std::size(s_address));
    ImGui::InputText("Password", s_password, std::size(s_password));

    if (m_transport.IsOnline())
    {
        if (ImGui::Button("Disconnect"))
            m_transport.Close();
    }
    else
    {
        if (ImGui::Button("Connect"))
        {
            m_transport.SetServerPassword(s_password);
            m_transport.Connect(s_address);
        }
    }

    ImGui::End();
}

void DebugService::OnDraw() noexcept
{
    const auto view = m_world.view<FormIdComponent>();
    if (view.empty() || !m_showDebugStuff)
        return;

    ImGui::BeginMainMenuBar();
    if (ImGui::BeginMenu("Helpers"))
    {
#if TP_SKYRIM64
        if (ImGui::Button("Unstuck player"))
        {
            auto* pPlayer = PlayerCharacter::Get();
            pPlayer->currentProcess->KnockExplosion(pPlayer, &pPlayer->position, 0.f);
        }

        if (ImGui::Button("Stop all combat"))
        {
            auto* pPlayer = PlayerCharacter::Get();
            pPlayer->PayCrimeGoldToAllFactions();

            ProcessLists* const pProcessLists = ProcessLists::Get();
            if (pProcessLists)
            {
                for (uint32_t i = 0; i < pProcessLists->highActorHandleArray.length; ++i)
                {
                    Actor* const pRefr = Cast<Actor>(TESObjectREFR::GetByHandle(pProcessLists->highActorHandleArray[i]));
                    if (pRefr && pRefr->GetNiNode())
                        pRefr->StopCombat();
                }
            }
        }
#endif
        ImGui::EndMenu();
    }
#if (!IS_MASTER)

    if (ImGui::BeginMenu("Components"))
    {
        ImGui::MenuItem("Show selected entity in world", nullptr, &m_drawComponentsInWorldSpace);
        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("UI"))
    {
        ImGui::MenuItem("Show build tag", nullptr, &m_showBuildTag);
        if (ImGui::Button("Log all open windows"))
        {
            UI* pUI = UI::Get();
            for (const auto& it : pUI->menuMap)
            {
                if (pUI->GetMenuOpen(it.key))
                    spdlog::info("{}", it.key.AsAscii());
            }
        }

#if TP_SKYRIM64
        if (ImGui::Button("Close all menus"))
        {
            UI::Get()->CloseAllMenus();
        }
#endif

        ImGui::EndMenu();
    }
#endif
    if (ImGui::BeginMenu("Debuggers"))
    {
        ImGui::MenuItem("Quests", nullptr, &g_enableQuestWindow);
        ImGui::MenuItem("Entities", nullptr, &g_enableEntitiesWindow);
        ImGui::MenuItem("Server", nullptr, &g_enableServerWindow);
        ImGui::MenuItem("Party", nullptr, &g_enablePartyWindow);
        ImGui::MenuItem("Dragon spawner", nullptr, &g_enableDragonSpawnerWindow);

#if (!IS_MASTER)
        ImGui::MenuItem("Network", nullptr, &g_enableNetworkWindow);
        ImGui::MenuItem("Forms", nullptr, &g_enableFormsWindow);
        ImGui::MenuItem("Inventory", nullptr, &g_enableInventoryWindow);
        ImGui::MenuItem("Animations", nullptr, &g_enableAnimWindow);
        ImGui::MenuItem("Player", nullptr, &g_enablePlayerWindow);
        ImGui::MenuItem("Skills", nullptr, &g_enableSkillsWindow);
        ImGui::MenuItem("Cell", nullptr, &g_enableCellWindow);
        ImGui::MenuItem("Processes", nullptr, &g_enableProcessesWindow);
        ImGui::MenuItem("Weather", nullptr, &g_enableWeatherWindow);
        ImGui::MenuItem("Combat", nullptr, &g_enableCombatWindow);
        ImGui::MenuItem("Calendar", nullptr, &g_enableCalendarWindow);
#endif

        ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Misc"))
    {
        if (ImGui::Button("Crash Client"))
        {
#if (!IS_MASTER)
            spdlog::error("Crash client");
            int* m = 0;
            *m = 1338;
#else
            ConnectionErrorEvent errorEvent{};
            errorEvent.ErrorDetail = "Skyrim Together never crashes ;)  With love, Yamashi, Force, Dragonisser, Cosideci.";

            m_world.GetRunner().Trigger(errorEvent);
#endif
        }
        ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();

    if (g_enableQuestWindow)
        DrawQuestDebugView();
    if (g_enableEntitiesWindow)
        DrawEntitiesView();
    if (g_enableServerWindow)
        DrawServerView();
    if (g_enablePartyWindow)
        DrawPartyView();
    if (g_enableDragonSpawnerWindow)
        DrawDragonSpawnerView();

#if (!IS_MASTER)
    if (g_enableNetworkWindow)
        DrawNetworkView();
    if (g_enableFormsWindow)
        DrawFormDebugView();
    if (g_enableInventoryWindow)
        DrawContainerDebugView();
    if (g_enableAnimWindow)
        DrawAnimDebugView();
    if (g_enablePlayerWindow)
        DrawPlayerDebugView();
    if (g_enableSkillsWindow)
        DrawSkillView();
    if (g_enableActorValuesWindow)
        DrawActorValuesView();
    if (g_enableCellWindow)
        DrawCellView();
    if (g_enableProcessesWindow)
        DrawProcessView();
    if (g_enableWeatherWindow)
        DrawWeatherView();
    if (g_enableCombatWindow)
        DrawCombatView();
    if (g_enableCalendarWindow)
        DrawCalendarView();

    if (m_drawComponentsInWorldSpace)
        DrawComponentDebugView();
#endif

    if (m_showBuildTag)
        DrawBuildTag();
}
