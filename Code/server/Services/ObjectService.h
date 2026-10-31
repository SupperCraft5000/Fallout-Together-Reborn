#pragma once

#include <Events/PacketEvent.h>
#include <Structs/WorkshopChange.h>

struct World;
struct PlayerLeaveCellEvent;
struct PlayerJoinEvent;
struct ActivateRequest;
struct LockChangeRequest;
struct RequestWorkshopChange;
struct AssignObjectsRequest;
struct ScriptAnimationRequest;

/**
 * @brief Manages (interactive) objects and relays interactions with said objects.
 */
class ObjectService
{
public:
    ObjectService(World& aWorld, entt::dispatcher& aDispatcher);

private:
    void OnPlayerLeaveCellEvent(const PlayerLeaveCellEvent& acEvent) noexcept;
    void OnAssignObjectsRequest(const PacketEvent<AssignObjectsRequest>&) noexcept;
    void OnActivate(const PacketEvent<ActivateRequest>&) const noexcept;
    void OnLockChange(const PacketEvent<LockChangeRequest>&) const noexcept;
    void OnWorkshopChange(const PacketEvent<RequestWorkshopChange>&) noexcept;
    void OnPlayerJoin(const PlayerJoinEvent&) const noexcept;
    void RememberWorkshopChange(const WorkshopChange& acChange) noexcept;
    void OnScriptAnimationRequest(const PacketEvent<ScriptAnimationRequest>&) noexcept;

    World& m_world;

    // One entry per object, gone when the server stops. Saves hold everything older.
    Vector<WorkshopChange> m_workshopChanges{};

    entt::scoped_connection m_leaveCellConnection;
    entt::scoped_connection m_assignObjectConnection;
    entt::scoped_connection m_activateConnection;
    entt::scoped_connection m_lockChangeConnection;
    entt::scoped_connection m_workshopChangeConnection;
    entt::scoped_connection m_playerJoinConnection;
    entt::scoped_connection m_scriptAnimationConnection;
};
