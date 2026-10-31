#pragma once

#include "Message.h"

#include <Structs/WorkshopChange.h>

struct RequestWorkshopChange final : ClientMessage
{
    static constexpr ClientOpcode Opcode = kRequestWorkshopChange;

    RequestWorkshopChange()
        : ClientMessage(Opcode)
    {
    }

    void SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept override;
    void DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept override;

    bool operator==(const RequestWorkshopChange& acRhs) const noexcept { return GetOpcode() == acRhs.GetOpcode() && Change == acRhs.Change; }

    WorkshopChange Change{};
};
