#pragma once

#include <Structs/GameId.h>

using TiltedPhoques::Buffer;

//! One settlement edit, forwarded as is by the server.
struct WorkshopChange
{
    enum Action : uint8_t
    {
        kPlaced = 0,
        kMoved,
        kDestroyed,
        kWired
    };

    //! One end of a power wire, named like the changed object itself.
    struct WireEnd
    {
        bool operator==(const WireEnd& acRhs) const noexcept;

        void Serialize(TiltedPhoques::Buffer::Writer& aWriter) const noexcept;
        void Deserialize(TiltedPhoques::Buffer::Reader& aReader) noexcept;

        uint32_t OwnerId{};
        uint32_t ObjectId{};
        GameId ObjectServerId{};
        GameId BaseId{};
        uint32_t LinkType{};
    };

    WorkshopChange() = default;
    ~WorkshopChange() = default;

    bool operator==(const WorkshopChange& acRhs) const noexcept;
    bool operator!=(const WorkshopChange& acRhs) const noexcept;

    void Serialize(TiltedPhoques::Buffer::Writer& aWriter) const noexcept;
    void Deserialize(TiltedPhoques::Buffer::Reader& aReader) noexcept;

    uint8_t Kind{};
    // Objects built during play have a form id only their builder knows, they are
    // named by that player and id. Objects of a plugin use ObjectServerId, ObjectId is then 0.
    uint32_t OwnerId{};
    uint32_t ObjectId{};
    GameId ObjectServerId{};
    GameId WorkshopId{};
    GameId BaseId{};
    glm::vec3 Position{};
    glm::vec3 Rotation{};
    // Where it was first placed, a save can hold it there
    glm::vec3 OriginalPosition{};
    // Sent from the server's memory to somebody joining
    bool Replay{};
    // Only sent for kWired
    WireEnd EndA{};
    WireEnd EndB{};
    glm::vec3 HalfExtents{};
    float Slack{};
    float Thickness{};
};
