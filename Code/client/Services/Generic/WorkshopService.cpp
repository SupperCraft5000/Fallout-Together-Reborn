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
#include <ExtraData/ExtraDataList.h>

#include <mutex>

namespace
{
constexpr uint32_t kWorkshopItemKeyword = 0x54BA6;
constexpr float kRadToDeg = 57.2957795f;
// Saves round positions a little
constexpr float kSameSpotRadius = 2.f;
// Spreads a burst of incoming pieces over a few frames
constexpr size_t kMaxAppliedPerFrame = 8;
constexpr uint8_t kBendableSplineFormType = 35;
constexpr double kWireSettleSeconds = 0.5;
constexpr double kPowerLogSeconds = 3.0;
constexpr float kWireMoveTolerance = 1.f;

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

// A wire's data is only complete once it settled, so just its id is kept
struct WireEvent
{
    uint32_t WireId;
    bool Removed;
};

struct PowerLink
{
    uint32_t FormId;
    uint32_t LinkType;
};

struct ExtraPowerLinks : BSExtraData
{
    GameArray<PowerLink> links;
};

struct ExtraBendableSplineParams : BSExtraData
{
    float slack;
    float thickness;
    NiPoint3 halfExtents;
    bool detachedEnd;
};

static_assert(offsetof(ExtraPowerLinks, links) == 0x18);
static_assert(offsetof(ExtraBendableSplineParams, slack) == 0x18);

std::mutex s_eventLock;
Vector<WorkshopEvent> s_events;
Vector<WireEvent> s_wireEvents;
// Our own wire work is not a player's edit
bool s_applyingWire = false;

bool IsWire(const TESObjectREFR* apRefr) noexcept
{
    return apRefr && apRefr->baseForm && static_cast<uint8_t>(apRefr->baseForm->formType) == kBendableSplineFormType;
}

ExtraPowerLinks* GetPowerLinks(const TESObjectREFR* apRefr) noexcept
{
    if (!apRefr || !apRefr->extraData)
        return nullptr;

    return static_cast<ExtraPowerLinks*>(apRefr->extraData->GetByType(ExtraDataType::PowerLinks));
}

const ExtraBendableSplineParams* GetSplineParams(const TESObjectREFR* apRefr) noexcept
{
    if (!apRefr || !apRefr->extraData)
        return nullptr;

    return static_cast<const ExtraBendableSplineParams*>(apRefr->extraData->GetByType(ExtraDataType::BendableSplineParams));
}

void QueueWorkshopEvent(WorkshopChange::Action aAction, const WorkshopEventData* apEvent) noexcept
{
    if (!apEvent || !apEvent->pWorkshop || !apEvent->pItem)
        return;

    TESObjectREFR* pWorkshop = apEvent->pWorkshop;
    TESObjectREFR* pItem = apEvent->pItem;

    // A workbench comes from a plugin, freshly built objects never do
    if (pWorkshop->IsTemporary() && !pItem->IsTemporary())
        std::swap(pWorkshop, pItem);

    // Wires have their own path, a plain copy would hang in the air unconnected
    if (!pItem->baseForm || IsWire(pItem))
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

TP_THIS_FUNCTION(TQueueRebuildSpline, void, void, TESObjectREFR*, bool, void*);
TP_THIS_FUNCTION(TDisconnectSpline, void, TESObjectREFR);

TQueueRebuildSpline* RealQueueRebuildSpline = nullptr;
TDisconnectSpline* RealDisconnectSpline = nullptr;

void QueueWireEvent(const TESObjectREFR* apWire, bool aRemoved) noexcept
{
    if (s_applyingWire || !IsWire(apWire) || !apWire->IsTemporary())
        return;

    std::scoped_lock lock(s_eventLock);
    s_wireEvents.push_back({apWire->formID, aRemoved});
}

// The workshop rebuilds a wire whenever it gets strung or an end moves
void TP_MAKE_THISCALL(HookQueueRebuildSpline, void, TESObjectREFR* apWire, bool aCreatePhysics, void* apChildNode)
{
    QueueWireEvent(apWire, false);
    TiltedPhoques::ThisCall(RealQueueRebuildSpline, apThis, apWire, aCreatePhysics, apChildNode);
}

void TP_MAKE_THISCALL(HookDisconnectSpline, TESObjectREFR)
{
    QueueWireEvent(apThis, true);
    TiltedPhoques::ThisCall(RealDisconnectSpline, apThis);
}

bool SetSplineShape(TESObjectREFR* apWire, float aThickness, float aSlack, const glm::vec3& acHalfExtents) noexcept
{
    using TSetSplineInfo = bool(__fastcall)(ExtraDataList*, float*, float*, NiPoint3*, bool*);

    POINTER_FALLOUT4(TSetSplineInfo, s_setSplineInfo, 894306);

    NiPoint3 halfExtents;
    halfExtents.x = acHalfExtents.x;
    halfExtents.y = acHalfExtents.y;
    halfExtents.z = acHalfExtents.z;

    return apWire->extraData && s_setSplineInfo.Get()(apWire->extraData, &aThickness, &aSlack, &halfExtents, nullptr);
}

void ConnectSpline(TESObjectREFR* apA, int32_t aLinkA, TESObjectREFR* apB, int32_t aLinkB, TESObjectREFR* apWire) noexcept
{
    using TConnectSpline = void(__fastcall)(TESObjectREFR*, int32_t, TESObjectREFR*, int32_t, TESObjectREFR*);

    POINTER_FALLOUT4(TConnectSpline, s_connectSpline, 59311);

    s_connectSpline.Get()(apA, aLinkA, apB, aLinkB, apWire);
}

void RebuildSpline(TESObjectREFR* apWire) noexcept
{
    POINTER_FALLOUT4(void*, s_taskQueue, 7491);

    TiltedPhoques::ThisCall(RealQueueRebuildSpline, *s_taskQueue.Get(), apWire, true, nullptr);
}

bool IsPowered(TESObjectREFR* apObject) noexcept
{
    using TIsPowered = bool(__fastcall)(TESObjectREFR*);

    POINTER_FALLOUT4(TIsPowered, s_isPowered, 1546240);

    return s_isPowered.Get()(apObject);
}

// The settlement's power data, a workbench nobody worked at yet has none
BSExtraData* GetWorkshopData(const TESObjectREFR* apWorkshop) noexcept
{
    if (!apWorkshop || !apWorkshop->extraData)
        return nullptr;

    return apWorkshop->extraData->GetByType(ExtraDataType::Workshop);
}

// A settlement nobody here worked at has no power data yet, the game makes it the same way
BSExtraData* EnsureWorkshopData(TESObjectREFR* apWorkshop) noexcept
{
    using TAllocate = void*(__fastcall)(size_t);
    using TConstruct = BSExtraData*(__fastcall)(void*);
    using TAddExtra = void(__fastcall)(ExtraDataList*, BSExtraData*);

    POINTER_FALLOUT4(TAllocate, s_allocateExtra, 898179);
    POINTER_FALLOUT4(TConstruct, s_constructWorkshopData, 1290994);
    POINTER_FALLOUT4(TAddExtra, s_addExtra, 787248);

    if (BSExtraData* pData = GetWorkshopData(apWorkshop))
        return pData;

    if (!apWorkshop || !apWorkshop->extraData)
        return nullptr;

    void* pMemory = s_allocateExtra.Get()(0x58);
    if (!pMemory)
        return nullptr;

    BSExtraData* pData = s_constructWorkshopData.Get()(pMemory);
    s_addExtra.Get()(apWorkshop->extraData, pData);

    spdlog::info("Workshop sync: made power data for workbench {:X}", apWorkshop->formID);
    return pData;
}

// What the workshop does once a wire is strung, the links alone leave it dead
void AddWireToGrid(TESObjectREFR* apWorkshop, TESObjectREFR* apA, TESObjectREFR* apB, TESObjectREFR* apWire) noexcept
{
    using TAddConnection = void(__fastcall)(BSExtraData*, TESObjectREFR*, TESObjectREFR*, TESObjectREFR*);
    using TUpdateItem = void(__fastcall)(TESObjectREFR*, BSExtraData*);
    using TUpdateWorkshop = void(__fastcall)(TESObjectREFR*);

    POINTER_FALLOUT4(TAddConnection, s_addConnection, 523652);
    POINTER_FALLOUT4(TUpdateItem, s_updateItem, 958345);
    POINTER_FALLOUT4(TUpdateWorkshop, s_updatePower, 63358);
    POINTER_FALLOUT4(TUpdateWorkshop, s_updateResources, 730242);

    BSExtraData* pData = EnsureWorkshopData(apWorkshop);
    if (!pData)
    {
        spdlog::warn("Workshop sync: workbench {:X} has no power data", apWorkshop->formID);
        return;
    }

    s_addConnection.Get()(pData, apA, apB, apWire);
    s_updateItem.Get()(apB, pData);
    s_updateItem.Get()(apA, pData);
    s_updatePower.Get()(apWorkshop);
    s_updateResources.Get()(apWorkshop);
}

void RemoveWireFromGrid(TESObjectREFR* apWorkshop, TESObjectREFR* apA, TESObjectREFR* apB, TESObjectREFR* apWire) noexcept
{
    using TRemoveConnection = void(__fastcall)(BSExtraData*, TESObjectREFR*, TESObjectREFR*, TESObjectREFR*, bool);

    POINTER_FALLOUT4(TRemoveConnection, s_removeConnection, 800489);

    if (BSExtraData* pData = GetWorkshopData(apWorkshop); pData && apA && apB)
        s_removeConnection.Get()(pData, apA, apB, apWire, true);
}

// A deleted object left on a grid gets its id reused by something else
void RemoveFromGrid(TESObjectREFR* apWorkshop, TESObjectREFR* apObject) noexcept
{
    using TFindGrid = void*(__fastcall)(BSExtraData*, TESObjectREFR*);
    using TRemoveItem = void(__fastcall)(BSExtraData*, TESObjectREFR*);

    POINTER_FALLOUT4(TFindGrid, s_findGrid, 1529707);
    POINTER_FALLOUT4(TRemoveItem, s_removeItem, 1456203);

    BSExtraData* pData = GetWorkshopData(apWorkshop);
    if (pData && s_findGrid.Get()(pData, apObject))
        s_removeItem.Get()(pData, apObject);
}

// Workbench an object belongs to
TESObjectREFR* GetObjectWorkshop(TESObjectREFR* apObject) noexcept
{
    using ObjectReference = TESObjectREFR;

    // Without it the caller falls back to the last workbench used, building goes on either way
    if (!apObject || !World::Get().ctx().at<PapyrusService>().Get("ObjectReference", "GetLinkedRef"))
        return nullptr;

    PAPYRUS_FUNCTION(TESObjectREFR*, ObjectReference, GetLinkedRef, TESForm*);

    TESForm* pKeyword = TESForm::GetById(kWorkshopItemKeyword);

    return pKeyword ? s_pGetLinkedRef(apObject, pKeyword) : nullptr;
}

// An existing wire between the two, the save of somebody joining can hold it
TESObjectREFR* FindWireBetween(const TESObjectREFR* apA, const TESObjectREFR* apB) noexcept
{
    auto* pLinks = GetPowerLinks(apA);
    if (!pLinks)
        return nullptr;

    for (const PowerLink& link : pLinks->links)
    {
        auto* pWire = Cast<TESObjectREFR>(TESForm::GetById(link.FormId));
        auto* pWireLinks = IsWire(pWire) ? GetPowerLinks(pWire) : nullptr;
        if (!pWireLinks)
            continue;

        for (const PowerLink& wireLink : pWireLinks->links)
        {
            if (wireLink.FormId == apB->formID)
                return pWire;
        }
    }

    return nullptr;
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

void WorkshopService::OnUpdate(const UpdateEvent& acEvent) noexcept
{
    SendLocalChanges();

    if (m_transport.IsOnline())
    {
        ApplyPendingChanges();
        CheckLocalWires(acEvent.Delta);
        LogPower(acEvent.Delta);
    }
}

void WorkshopService::OnDisconnected(const DisconnectedEvent&) noexcept
{
    m_pending.clear();
    m_keyToLocal.clear();
    m_localToKey.clear();
    m_wires.clear();
    m_wireChecks.clear();
    m_powerChecks.clear();
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

        m_lastWorkshopId = event.WorkshopId;

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
    if (acChange.Kind == WorkshopChange::kWired)
    {
        ApplyWire(acChange, apWorkshop);
        return;
    }

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

    if (IsWire(pObject))
    {
        RemoveWire(pObject, apWorkshop);
    }
    else
    {
        RemoveWiresOf(pObject, apWorkshop);
        RemoveFromGrid(apWorkshop, pObject);

        pObject->Disable();
        if (isTemporary)
            pObject->Delete();
    }

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

void WorkshopService::Arm(Vector<DelayedCheck>& aChecks, uint32_t aFormId, double aSeconds) noexcept
{
    for (DelayedCheck& check : aChecks)
    {
        if (check.FormId == aFormId)
        {
            check.Remaining = aSeconds;
            return;
        }
    }

    aChecks.push_back({aFormId, aSeconds});
}

Vector<uint32_t> WorkshopService::TakeDue(Vector<DelayedCheck>& aChecks, double aDelta) noexcept
{
    Vector<uint32_t> due;
    Vector<DelayedCheck> waiting;

    for (DelayedCheck check : aChecks)
    {
        check.Remaining -= aDelta;
        if (check.Remaining > 0.0)
            waiting.push_back(check);
        else
            due.push_back(check.FormId);
    }

    aChecks = std::move(waiting);
    return due;
}

void WorkshopService::CheckLocalWires(double aDelta) noexcept
{
    Vector<WireEvent> events;
    {
        std::scoped_lock lock(s_eventLock);
        events.swap(s_wireEvents);
    }

    for (const WireEvent& event : events)
    {
        if (event.Removed)
        {
            std::erase_if(m_wireChecks, [&event](const DelayedCheck& acCheck) { return acCheck.FormId == event.WireId; });
            SendWireRemoved(event.WireId);
        }
        else
        {
            Arm(m_wireChecks, event.WireId, kWireSettleSeconds);
        }
    }

    for (const uint32_t wireId : TakeDue(m_wireChecks, aDelta))
        SendWire(wireId);
}

bool WorkshopService::FillWireEnd(uint32_t aFormId, uint32_t aLinkType, WorkshopChange::WireEnd& aEnd) const noexcept
{
    const auto* pObject = Cast<TESObjectREFR>(TESForm::GetById(aFormId));
    if (!pObject || !pObject->baseForm || pObject->baseForm->IsTemporary())
        return false;

    const auto& modSystem = m_world.GetModSystem();

    aEnd.LinkType = aLinkType;
    if (!modSystem.GetServerModId(pObject->baseForm->formID, aEnd.BaseId))
        return false;

    if (const auto itor = m_localToKey.find(aFormId); itor != std::end(m_localToKey))
    {
        aEnd.OwnerId = static_cast<uint32_t>(itor->second >> 32);
        aEnd.ObjectId = static_cast<uint32_t>(itor->second);
        return true;
    }

    if (pObject->IsTemporary())
    {
        aEnd.OwnerId = m_transport.GetLocalPlayerId();
        aEnd.ObjectId = aFormId;
        return true;
    }

    return modSystem.GetServerModId(aFormId, aEnd.ObjectServerId);
}

void WorkshopService::SendWire(uint32_t aWireId) noexcept
{
    auto* pWire = Cast<TESObjectREFR>(TESForm::GetById(aWireId));
    auto* pLinks = GetPowerLinks(pWire);
    const auto* pParams = GetSplineParams(pWire);

    // One end is still in the player's hands
    if (!IsWire(pWire) || !pLinks || !pParams || pLinks->links.length != 2 || pParams->detachedEnd)
        return;

    const PowerLink linkA = pLinks->links[0];
    const PowerLink linkB = pLinks->links[1];

    RequestWorkshopChange request;
    WorkshopChange& change = request.Change;
    change.Kind = WorkshopChange::kWired;
    change.Position = pWire->position;
    change.Rotation = pWire->rotation;
    change.OriginalPosition = pWire->position;
    change.HalfExtents = pParams->halfExtents;
    change.Slack = pParams->slack;
    change.Thickness = pParams->thickness;

    if (!FillWireEnd(linkA.FormId, linkA.LinkType, change.EndA) || !FillWireEnd(linkB.FormId, linkB.LinkType, change.EndB))
    {
        spdlog::warn("Workshop sync: wire {:X} has an end nobody else knows", aWireId);
        return;
    }

    auto* pWorkshop = GetObjectWorkshop(Cast<TESObjectREFR>(TESForm::GetById(linkA.FormId)));
    if (!pWorkshop)
        pWorkshop = GetObjectWorkshop(Cast<TESObjectREFR>(TESForm::GetById(linkB.FormId)));
    const uint32_t workshopId = pWorkshop ? pWorkshop->formID : m_lastWorkshopId;

    const auto& modSystem = m_world.GetModSystem();
    if (workshopId == 0 || (workshopId >> 24) == 0xFF || !modSystem.GetServerModId(workshopId, change.WorkshopId) ||
        !modSystem.GetServerModId(pWire->baseForm->formID, change.BaseId))
    {
        spdlog::warn("Workshop sync: no workbench for wire {:X}", aWireId);
        return;
    }

    if (const auto itor = m_localToKey.find(aWireId); itor != std::end(m_localToKey))
    {
        change.OwnerId = static_cast<uint32_t>(itor->second >> 32);
        change.ObjectId = static_cast<uint32_t>(itor->second);
    }
    else
    {
        change.OwnerId = m_transport.GetLocalPlayerId();
        change.ObjectId = aWireId;
    }

    // Rebuilt without any real change, for example when it came into view
    if (const auto itor = m_wires.find(aWireId); itor != std::end(m_wires))
    {
        const WorkshopChange& last = itor->second;
        if (last.EndA == change.EndA && last.EndB == change.EndB && glm::distance(last.Position, change.Position) < kWireMoveTolerance)
            return;
    }

    m_wires[aWireId] = change;
    m_transport.Send(request);

    spdlog::info("Workshop event wire: {:X} between {:X} and {:X}", aWireId, linkA.FormId, linkB.FormId);
}

void WorkshopService::SendWireRemoved(uint32_t aWireId) noexcept
{
    const auto itor = m_wires.find(aWireId);
    if (itor == std::end(m_wires))
        return;

    RequestWorkshopChange request;
    request.Change = itor->second;
    request.Change.Kind = WorkshopChange::kDestroyed;
    request.Change.Replay = false;

    m_transport.Send(request);
    m_wires.erase(itor);

    if (const auto localItor = m_localToKey.find(aWireId); localItor != std::end(m_localToKey))
    {
        m_keyToLocal.erase(localItor->second);
        m_localToKey.erase(localItor);
    }

    spdlog::info("Workshop event wire removed: {:X}", aWireId);
}

TESObjectREFR* WorkshopService::FindWireEnd(const WorkshopChange::WireEnd& acEnd) const noexcept
{
    const auto& modSystem = m_world.GetModSystem();

    uint32_t formId = 0;
    if (acEnd.ObjectId == 0)
        formId = modSystem.GetGameId(acEnd.ObjectServerId);
    else if (const auto itor = m_keyToLocal.find(MakeKey(acEnd.OwnerId, acEnd.ObjectId)); itor != std::end(m_keyToLocal))
        formId = itor->second;
    else
        formId = acEnd.ObjectId;

    auto* pObject = Cast<TESObjectREFR>(TESForm::GetById(formId));
    if (!pObject || !pObject->baseForm || pObject->baseForm->formID != modSystem.GetGameId(acEnd.BaseId))
        return nullptr;

    return pObject;
}

void WorkshopService::ApplyWire(const WorkshopChange& acChange, TESObjectREFR* apWorkshop) noexcept
{
    TESObjectREFR* pEndA = FindWireEnd(acChange.EndA);
    TESObjectREFR* pEndB = FindWireEnd(acChange.EndB);
    if (!pEndA || !pEndB)
    {
        spdlog::warn("Workshop sync: an end of wire {:X} is not here", acChange.ObjectId);
        return;
    }

    const uint64_t key = MakeKey(acChange.OwnerId, acChange.ObjectId);

    // An end moved, the wire is strung again
    if (const auto itor = m_keyToLocal.find(key); itor != std::end(m_keyToLocal))
    {
        const uint32_t oldId = itor->second;
        m_keyToLocal.erase(itor);
        m_localToKey.erase(oldId);
        m_wires.erase(oldId);

        if (auto* pOld = Cast<TESObjectREFR>(TESForm::GetById(oldId)))
            RemoveWire(pOld, apWorkshop);
    }

    TESObjectREFR* pWire = FindWireBetween(pEndA, pEndB);
    if (pWire)
    {
        spdlog::info("Workshop sync: wire is already here as {:X}", pWire->formID);
    }
    else
    {
        TESForm* pBase = TESForm::GetById(m_world.GetModSystem().GetGameId(acChange.BaseId));
        pWire = pBase ? PlaceCopy(apWorkshop, pBase) : nullptr;
        if (!pWire)
        {
            spdlog::warn("Workshop sync: could not make wire {:X}", acChange.ObjectId);
            return;
        }

        SetTransform(pWire, acChange.Position, acChange.Rotation);

        s_applyingWire = true;
        SetSplineShape(pWire, acChange.Thickness, acChange.Slack, acChange.HalfExtents);
        ConnectSpline(pEndA, static_cast<int32_t>(acChange.EndA.LinkType), pEndB, static_cast<int32_t>(acChange.EndB.LinkType), pWire);
        RebuildSpline(pWire);
        AddWireToGrid(apWorkshop, pEndA, pEndB, pWire);
        s_applyingWire = false;

        spdlog::info("Workshop sync: wired {:X} and {:X} with {:X}", pEndA->formID, pEndB->formID, pWire->formID);
    }

    m_keyToLocal[key] = pWire->formID;
    m_localToKey[pWire->formID] = key;
    m_wires[pWire->formID] = acChange;

    Arm(m_powerChecks, pEndA->formID, kPowerLogSeconds);
    Arm(m_powerChecks, pEndB->formID, kPowerLogSeconds);
}

void WorkshopService::RemoveWire(TESObjectREFR* apWire, TESObjectREFR* apWorkshop) noexcept
{
    using TDisconnect = void(__fastcall)(TESObjectREFR*);

    s_applyingWire = true;

    if (auto* pLinks = GetPowerLinks(apWire); pLinks && pLinks->links.length == 2)
    {
        auto* pEndA = Cast<TESObjectREFR>(TESForm::GetById(pLinks->links[0].FormId));
        auto* pEndB = Cast<TESObjectREFR>(TESForm::GetById(pLinks->links[1].FormId));
        RemoveWireFromGrid(apWorkshop, pEndA, pEndB, apWire);
    }

    reinterpret_cast<TDisconnect*>(RealDisconnectSpline)(apWire);
    s_applyingWire = false;

    apWire->Disable();
    apWire->Delete();
}

void WorkshopService::RemoveWiresOf(TESObjectREFR* apObject, TESObjectREFR* apWorkshop) noexcept
{
    auto* pLinks = GetPowerLinks(apObject);
    if (!pLinks)
        return;

    Vector<uint32_t> wires;
    for (const PowerLink& link : pLinks->links)
    {
        if (IsWire(Cast<TESObjectREFR>(TESForm::GetById(link.FormId))))
            wires.push_back(link.FormId);
    }

    for (const uint32_t wireId : wires)
    {
        auto* pWire = Cast<TESObjectREFR>(TESForm::GetById(wireId));
        if (!pWire)
            continue;

        RemoveWire(pWire, apWorkshop);
        m_wires.erase(wireId);

        if (const auto itor = m_localToKey.find(wireId); itor != std::end(m_localToKey))
        {
            m_keyToLocal.erase(itor->second);
            m_localToKey.erase(itor);
        }
    }
}

void WorkshopService::LogPower(double aDelta) noexcept
{
    for (const uint32_t formId : TakeDue(m_powerChecks, aDelta))
    {
        if (auto* pObject = Cast<TESObjectREFR>(TESForm::GetById(formId)))
            spdlog::info("Workshop sync: {:X} powered {}", formId, IsPowered(pObject));
    }
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

        POINTER_FALLOUT4(TQueueRebuildSpline, s_queueRebuildSpline, 198419);
        POINTER_FALLOUT4(TDisconnectSpline, s_disconnectSpline, 750682);

        RealQueueRebuildSpline = s_queueRebuildSpline.Get();
        RealDisconnectSpline = s_disconnectSpline.Get();

        TP_HOOK(&RealWorkshopPlaced, HookWorkshopPlaced);
        TP_HOOK(&RealWorkshopMoved, HookWorkshopMoved);
        TP_HOOK(&RealWorkshopDestroyed, HookWorkshopDestroyed);
        TP_HOOK(&RealQueueRebuildSpline, HookQueueRebuildSpline);
        TP_HOOK(&RealDisconnectSpline, HookDisconnectSpline);
    });
