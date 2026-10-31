#pragma once

#include "Message.h"

#include <Structs/WorkshopChange.h>

struct NotifyWorkshopChange final : ServerMessage
{
    static constexpr ServerOpcode Opcode = kNotifyWorkshopChange;

    NotifyWorkshopChange()
        : ServerMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const NotifyWorkshopChange& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && Change == acRhs.Change; }

    WorkshopChange Change{};
};
