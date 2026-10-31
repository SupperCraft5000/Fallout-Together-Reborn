#pragma once

#include <Structs/WorkshopChange.h>

struct World;
struct TransportService;
struct TESObjectREFR;
struct TESForm;

struct UpdateEvent;
struct DisconnectedEvent;
struct NotifyWorkshopChange;

/**
 * @brief Mirrors what players place, move and scrap in a settlement.
 *
 * The server keeps the edits of its run and replays them to anybody joining.
 */
struct WorkshopService
{
    WorkshopService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransport) noexcept;
    ~WorkshopService() noexcept = default;

    TP_NOCOPYMOVE(WorkshopService);

private:
    struct DelayedCheck
    {
        uint32_t FormId;
        double Remaining;
    };

    static void Arm(Vector<DelayedCheck>& aChecks, uint32_t aFormId, double aSeconds) noexcept;
    static Vector<uint32_t> TakeDue(Vector<DelayedCheck>& aChecks, double aDelta) noexcept;

    void OnUpdate(const UpdateEvent&) noexcept;
    void OnDisconnected(const DisconnectedEvent&) noexcept;
    void OnNotifyWorkshopChange(const NotifyWorkshopChange&) noexcept;

    void SendLocalChanges() noexcept;
    void ApplyPendingChanges() noexcept;
    void ApplyChange(const WorkshopChange& acChange, TESObjectREFR* apWorkshop) noexcept;
    TESObjectREFR* FindObject(const WorkshopChange& acChange) const noexcept;
    TESObjectREFR* FindSavedObject(const WorkshopChange& acChange, TESForm* apBase) const noexcept;

    void CheckLocalWires(double aDelta) noexcept;
    void SendWire(uint32_t aWireId) noexcept;
    void SendWireRemoved(uint32_t aWireId) noexcept;
    void ApplyWire(const WorkshopChange& acChange, TESObjectREFR* apWorkshop) noexcept;
    void RemoveWire(TESObjectREFR* apWire) noexcept;
    void RemoveWiresOf(TESObjectREFR* apObject) noexcept;
    void LogPower(double aDelta) noexcept;
    bool FillWireEnd(uint32_t aFormId, uint32_t aLinkType, WorkshopChange::WireEnd& aEnd) const noexcept;
    TESObjectREFR* FindWireEnd(const WorkshopChange::WireEnd& acEnd) const noexcept;

    World& m_world;
    TransportService& m_transport;

    // Applied once the settlement is loaded here, in the order they came in
    Vector<WorkshopChange> m_pending{};
    // (owner << 32 | owner's form id) to the copy made here, and back
    Map<uint64_t, uint32_t> m_keyToLocal{};
    Map<uint32_t, uint64_t> m_localToKey{};
    // Wires by form id here, as last sent or applied
    Map<uint32_t, WorkshopChange> m_wires{};
    // Wires being dragged change a lot, they are looked at once they settled
    Vector<DelayedCheck> m_wireChecks{};
    // Objects whose power state goes to the log once the grid caught up
    Vector<DelayedCheck> m_powerChecks{};
    uint32_t m_lastWorkshopId{};

    entt::scoped_connection m_updateConnection;
    entt::scoped_connection m_disconnectedConnection;
    entt::scoped_connection m_notifyConnection;
};
