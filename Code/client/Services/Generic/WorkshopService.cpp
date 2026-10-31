#include <TiltedOnlinePCH.h>

#include <Services/WorkshopService.h>
#include <Services/PapyrusService.h>
#include <Services/TransportService.h>

#include <World.h>

#include <Events/UpdateEvent.h>
#include <Events/DisconnectedEvent.h>
#include <Messages/RequestWorkshopChange.h>
#include <Messages/NotifyWorkshopChange.h>

#include <Actor.h>
#include <PlayerCharacter.h>

#include <mutex>

namespace
{
constexpr uint32_t kWorkshopItemKeyword = 0x54BA6;
constexpr float kRadToDeg = 57.2957795f;
// Saves round positions a little
constexpr float kSameSpotRadius = 2.f;
// Spreads a burst of incoming pieces over a few frames
constexpr size_t kMaxAppliedPerFrame = 8;

struct WorkshopEventData
{
    TESObjectREFR* pWorkshop;
    TESObjectREFR* pItem;
};

// The game hands these to scripts, the data is copied right away as the
// sinks are not guaranteed to run where the services do.
struct WorkshopEvent
{
    WorkshopChange::Action Action;
    uint32_t ItemId;
    uint32_t WorkshopId;
    uint32_t BaseId;
    bool IsActor;
    NiPoint3 Position;
    NiPoint3 Rotation;
};

std::mutex s_eventLock;
Vector<WorkshopEvent> s_events;

void QueueWorkshopEvent(WorkshopChange::Action aAction, const WorkshopEventData* apEvent) noexcept
{
    if (!apEvent || !apEvent->pWorkshop || !apEvent->pItem)
        return;

    TESObjectREFR* pWorkshop = apEvent->pWorkshop;
    TESObjectREFR* pItem = apEvent->pItem;

    // A workbench comes from a plugin, freshly built objects never do
    if (pWorkshop->IsTemporary() && !pItem->IsTemporary())
        std::swap(pWorkshop, pItem);

    if (!pItem->baseForm)
        return;

    WorkshopEvent event{};
    event.Action = aAction;
    event.ItemId = pItem->formID;
    event.WorkshopId = pWorkshop->formID;
    event.BaseId = pItem->baseForm->formID;
    event.IsActor = Cast<Actor>(pItem) != nullptr;
    event.Position = pItem->position;
    event.Rotation = pItem->rotation;

    std::scoped_lock lock(s_eventLock);
    s_events.push_back(event);
}

TP_THIS_FUNCTION(TWorkshopSink, uint64_t, void, const WorkshopEventData*, void*);

TWorkshopSink* RealWorkshopPlaced = nullptr;
TWorkshopSink* RealWorkshopMoved = nullptr;
TWorkshopSink* RealWorkshopDestroyed = nullptr;

uint64_t TP_MAKE_THISCALL(HookWorkshopPlaced, void, const WorkshopEventData* apEvent, void* apSource)
{
    QueueWorkshopEvent(WorkshopChange::kPlaced, apEvent);
    return TiltedPhoques::ThisCall(RealWorkshopPlaced, apThis, apEvent, apSource);
}

uint64_t TP_MAKE_THISCALL(HookWorkshopMoved, void, const WorkshopEventData* apEvent, void* apSource)
{
    QueueWorkshopEvent(WorkshopChange::kMoved, apEvent);
    return TiltedPhoques::ThisCall(RealWorkshopMoved, apThis, apEvent, apSource);
}

uint64_t TP_MAKE_THISCALL(HookWorkshopDestroyed, void, const WorkshopEventData* apEvent, void* apSource)
{
    QueueWorkshopEvent(WorkshopChange::kDestroyed, apEvent);
    return TiltedPhoques::ThisCall(RealWorkshopDestroyed, apThis, apEvent, apSource);
}

bool HasPapyrusFunctions() noexcept
{
    const auto& papyrus = World::Get().ctx().at<PapyrusService>();
    for (const char* cpName : {"PlaceAtMe", "SetPosition", "SetAngle", "SetLinkedRef"})
    {
        if (!papyrus.Get("ObjectReference", cpName))
        {
            spdlog::error("Workshop sync: papyrus function {} is missing", cpName);
            return false;
        }
    }

    return true;
}

TESObjectREFR* PlaceCopy(TESObjectREFR* apWorkshop, TESForm* apBase) noexcept
{
    using ObjectReference = TESObjectREFR;

    // count, force persist, initially disabled, delete when able
    LATENT_PAPYRUS_FUNCTION(TESObjectREFR*, ObjectReference, PlaceAtMe, TESForm*, int32_t, bool, bool, bool);

    return s_pPlaceAtMe(apWorkshop, apBase, 1, false, false, false);
}

void SetTransform(TESObjectREFR* apObject, const glm::vec3& acPosition, const glm::vec3& acRotation) noexcept
{
    using ObjectReference = TESObjectREFR;

    LATENT_PAPYRUS_FUNCTION(bool, ObjectReference, SetPosition, float, float, float);
    LATENT_PAPYRUS_FUNCTION(bool, ObjectReference, SetAngle, float, float, float);

    s_pSetPosition(apObject, acPosition.x, acPosition.y, acPosition.z);
    s_pSetAngle(apObject, acRotation.x * kRadToDeg, acRotation.y * kRadToDeg, acRotation.z * kRadToDeg);
}

// Makes the copy scrappable and part of the settlement on this side
void LinkToWorkshop(TESObjectREFR* apObject, TESObjectREFR* apWorkshop) noexcept
{
    using ObjectReference = TESObjectREFR;

    PAPYRUS_FUNCTION(void, ObjectReference, SetLinkedRef, TESObjectREFR*, TESForm*);

    if (TESForm* pKeyword = TESForm::GetById(kWorkshopItemKeyword))
        s_pSetLinkedRef(apObject, apWorkshop, pKeyword);
}

// Only looks through the loaded cells, which holds as the workbench is loaded
TESObjectREFR* FindAtSpot(TESForm* apBase, const glm::vec3& acPosition) noexcept
{
    // Game functions do not show up in PapyrusService, this is the native behind Game.FindClosestReferenceOfType
    using TFindClosest = TESObjectREFR*(__fastcall)(BSScript::IVirtualMachine*, uint32_t, void*, TESForm*, float, float, float, float);

    POINTER_FALLOUT4(TFindClosest, s_findClosest, 1459241);

    return s_findClosest.Get()(GameVM::Get()->virtualMachine, 0, nullptr, apBase, acPosition.x, acPosition.y, acPosition.z, kSameSpotRadius);
}

uint64_t MakeKey(uint32_t aOwnerId, uint32_t aObjectId) noexcept
{
    return (static_cast<uint64_t>(aOwnerId) << 32) | aObjectId;
}
} // namespace

WorkshopService::WorkshopService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransport) noexcept
    : m_world(aWorld)
    , m_transport(aTransport)
{
    m_updateConnection = aDispatcher.sink<UpdateEvent>().connect<&WorkshopService::OnUpdate>(this);
    m_disconnectedConnection = aDispatcher.sink<DisconnectedEvent>().connect<&WorkshopService::OnDisconnected>(this);
    m_notifyConnection = aDispatcher.sink<NotifyWorkshopChange>().connect<&WorkshopService::OnNotifyWorkshopChange>(this);
}

void WorkshopService::OnUpdate(const UpdateEvent&) noexcept
{
    SendLocalChanges();

    if (m_transport.IsOnline())
        ApplyPendingChanges();
}

void WorkshopService::OnDisconnected(const DisconnectedEvent&) noexcept
{
    m_pending.clear();
    m_keyToLocal.clear();
    m_localToKey.clear();
}

void WorkshopService::OnNotifyWorkshopChange(const NotifyWorkshopChange& acMessage) noexcept
{
    m_pending.push_back(acMessage.Change);
}

void WorkshopService::SendLocalChanges() noexcept
{
    Vector<WorkshopEvent> events;
    {
        std::scoped_lock lock(s_eventLock);
        events.swap(s_events);
    }

    if (!m_transport.IsOnline())
        return;

    const auto& modSystem = m_world.GetModSystem();

    for (const WorkshopEvent& event : events)
    {
        spdlog::info("Workshop event {}: item {:X} base {:X} workshop {:X}", static_cast<int>(event.Action), event.ItemId, event.BaseId, event.WorkshopId);

        // Turrets and the like are actors, those go through the character sync
        if (event.IsActor)
            continue;

        const auto localItor = m_localToKey.find(event.ItemId);

        // Our own copy of something a remote player built
        if (event.Action == WorkshopChange::kPlaced && localItor != std::end(m_localToKey))
            continue;

        // Bases made during play and temporary workbenches mean nothing on the other side
        if ((event.BaseId >> 24) == 0xFF || (event.WorkshopId >> 24) == 0xFF)
            continue;

        RequestWorkshopChange request;
        WorkshopChange& change = request.Change;
        change.Kind = event.Action;
        change.Position = event.Position;
        change.Rotation = event.Rotation;
        if (event.Action == WorkshopChange::kPlaced)
            change.OriginalPosition = event.Position;

        if (!modSystem.GetServerModId(event.WorkshopId, change.WorkshopId) || !modSystem.GetServerModId(event.BaseId, change.BaseId))
            continue;

        if (localItor != std::end(m_localToKey))
        {
            change.OwnerId = static_cast<uint32_t>(localItor->second >> 32);
            change.ObjectId = static_cast<uint32_t>(localItor->second);
        }
        else if ((event.ItemId >> 24) == 0xFF)
        {
            change.OwnerId = m_transport.GetLocalPlayerId();
            change.ObjectId = event.ItemId;
        }
        else if (!modSystem.GetServerModId(event.ItemId, change.ObjectServerId))
            continue;

        if (event.Action == WorkshopChange::kDestroyed && localItor != std::end(m_localToKey))
        {
            m_keyToLocal.erase(localItor->second);
            m_localToKey.erase(localItor);
        }

        m_transport.Send(request);
    }
}

void WorkshopService::ApplyPendingChanges() noexcept
{
    if (m_pending.empty())
        return;

    // Somebody joining gets these while the game may still be loading
    PlayerCharacter* pPlayer = PlayerCharacter::Get();
    if (!pPlayer || !pPlayer->GetNiNode())
        return;

    static const bool s_ready = HasPapyrusFunctions();
    if (!s_ready)
    {
        m_pending.clear();
        return;
    }

    const auto& modSystem = m_world.GetModSystem();

    size_t applied = 0;
    Vector<WorkshopChange> waiting;

    for (const WorkshopChange& change : m_pending)
    {
        auto* pWorkshop = Cast<TESObjectREFR>(TESForm::GetById(modSystem.GetGameId(change.WorkshopId)));
        if (!pWorkshop)
        {
            spdlog::warn("Workshop sync: workbench {:X}:{:X} not found", change.WorkshopId.ModId, change.WorkshopId.BaseId);
            continue;
        }

        if (applied >= kMaxAppliedPerFrame || !pWorkshop->GetNiNode())
        {
            waiting.push_back(change);
            continue;
        }

        ApplyChange(change, pWorkshop);
        ++applied;
    }

    m_pending = std::move(waiting);
}

void WorkshopService::ApplyChange(const WorkshopChange& acChange, TESObjectREFR* apWorkshop) noexcept
{
    if (acChange.Kind == WorkshopChange::kPlaced)
    {
        const uint64_t key = MakeKey(acChange.OwnerId, acChange.ObjectId);
        if (m_keyToLocal.count(key))
            return;

        TESForm* pBase = TESForm::GetById(m_world.GetModSystem().GetGameId(acChange.BaseId));
        if (!pBase)
        {
            spdlog::warn("Workshop sync: base {:X}:{:X} not found", acChange.BaseId.ModId, acChange.BaseId.BaseId);
            return;
        }

        // The save of somebody joining can already hold it
        if (acChange.Replay)
        {
            if (TESObjectREFR* pExisting = FindSavedObject(acChange, pBase))
            {
                SetTransform(pExisting, acChange.Position, acChange.Rotation);

                m_keyToLocal[key] = pExisting->formID;
                m_localToKey[pExisting->formID] = key;

                spdlog::info("Workshop sync: {:X} is already here as {:X}", pBase->formID, pExisting->formID);
                return;
            }
        }

        TESObjectREFR* pObject = PlaceCopy(apWorkshop, pBase);
        if (!pObject)
        {
            spdlog::warn("Workshop sync: could not place {:X}", pBase->formID);
            return;
        }

        SetTransform(pObject, acChange.Position, acChange.Rotation);
        LinkToWorkshop(pObject, apWorkshop);

        m_keyToLocal[key] = pObject->formID;
        m_localToKey[pObject->formID] = key;

        spdlog::info("Workshop sync: placed {:X} as {:X}", pBase->formID, pObject->formID);
        return;
    }

    TESObjectREFR* pObject = FindObject(acChange);
    if (!pObject)
    {
        // Normal for replays, what got scrapped may never have been here
        if (!acChange.Replay)
            spdlog::warn("Workshop sync: object {:X} of player {} not found", acChange.ObjectId, acChange.OwnerId);
        return;
    }

    if (acChange.Kind == WorkshopChange::kMoved)
    {
        SetTransform(pObject, acChange.Position, acChange.Rotation);
        spdlog::info("Workshop sync: moved {:X}", pObject->formID);
        return;
    }

    const bool isTemporary = pObject->IsTemporary();
    const uint32_t formId = pObject->formID;

    pObject->Disable();
    if (isTemporary)
        pObject->Delete();

    spdlog::info("Workshop sync: removed {:X}", formId);

    const auto localItor = m_localToKey.find(formId);
    if (localItor != std::end(m_localToKey))
    {
        m_keyToLocal.erase(localItor->second);
        m_localToKey.erase(localItor);
    }
}

TESObjectREFR* WorkshopService::FindSavedObject(const WorkshopChange& acChange, TESForm* apBase) const noexcept
{
    for (const glm::vec3* cpPosition : {&acChange.Position, &acChange.OriginalPosition})
    {
        TESObjectREFR* pObject = FindAtSpot(apBase, *cpPosition);
        if (pObject && !m_localToKey.count(pObject->formID))
            return pObject;
    }

    return nullptr;
}

TESObjectREFR* WorkshopService::FindObject(const WorkshopChange& acChange) const noexcept
{
    const auto& modSystem = m_world.GetModSystem();

    uint32_t formId = 0;
    if (acChange.ObjectId == 0)
        formId = modSystem.GetGameId(acChange.ObjectServerId);
    else if (const auto itor = m_keyToLocal.find(MakeKey(acChange.OwnerId, acChange.ObjectId)); itor != std::end(m_keyToLocal))
        formId = itor->second;
    else
        // Ours, or built before this session and part of the save both players share
        formId = acChange.ObjectId;

    auto* pObject = Cast<TESObjectREFR>(TESForm::GetById(formId));

    // Temporary ids get reused, the base tells us if this still is the same object
    if (!pObject || !pObject->baseForm || pObject->baseForm->formID != modSystem.GetGameId(acChange.BaseId))
    {
        if (!acChange.Replay)
            return nullptr;

        TESForm* pBase = TESForm::GetById(modSystem.GetGameId(acChange.BaseId));
        return pBase ? FindSavedObject(acChange, pBase) : nullptr;
    }

    return pObject;
}

static TiltedPhoques::Initializer s_workshopHooks(
    []()
    {
        POINTER_FALLOUT4(TWorkshopSink, s_workshopPlaced, 697786);
        POINTER_FALLOUT4(TWorkshopSink, s_workshopMoved, 725213);
        POINTER_FALLOUT4(TWorkshopSink, s_workshopDestroyed, 313584);

        RealWorkshopPlaced = s_workshopPlaced.Get();
        RealWorkshopMoved = s_workshopMoved.Get();
        RealWorkshopDestroyed = s_workshopDestroyed.Get();

        TP_HOOK(&RealWorkshopPlaced, HookWorkshopPlaced);
        TP_HOOK(&RealWorkshopMoved, HookWorkshopMoved);
        TP_HOOK(&RealWorkshopDestroyed, HookWorkshopDestroyed);
    });
