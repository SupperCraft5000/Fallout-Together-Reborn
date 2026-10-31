#pragma once

#include <Structs/WorkshopChange.h>

struct World;
struct TransportService;
struct TESObjectREFR;

struct UpdateEvent;
struct DisconnectedEvent;
struct NotifyWorkshopChange;

/**
 * @brief Mirrors what players place, move and scrap in a settlement.
 *
 * Nothing is stored on the server, somebody joining later does not get earlier edits.
 */
struct WorkshopService
{
    WorkshopService(World& aWorld, entt::dispatcher& aDispatcher, TransportService& aTransport) noexcept;
    ~WorkshopService() noexcept = default;

    TP_NOCOPYMOVE(WorkshopService);

private:
    void OnUpdate(const UpdateEvent&) noexcept;
    void OnDisconnected(const DisconnectedEvent&) noexcept;
    void OnNotifyWorkshopChange(const NotifyWorkshopChange&) noexcept;

    void SendLocalChanges() noexcept;
    void ApplyPendingChanges() noexcept;
    void ApplyChange(const WorkshopChange& acChange, TESObjectREFR* apWorkshop) noexcept;
    TESObjectREFR* FindObject(const WorkshopChange& acChange) const noexcept;

    World& m_world;
    TransportService& m_transport;

    // Applied once the settlement is loaded here, in the order they came in
    Vector<WorkshopChange> m_pending{};
    // (owner << 32 | owner's form id) to the copy made here, and back
    Map<uint64_t, uint32_t> m_keyToLocal{};
    Map<uint32_t, uint64_t> m_localToKey{};

    entt::scoped_connection m_updateConnection;
    entt::scoped_connection m_disconnectedConnection;
    entt::scoped_connection m_notifyConnection;
};
