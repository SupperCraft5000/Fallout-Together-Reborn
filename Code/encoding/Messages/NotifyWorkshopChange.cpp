#include <Messages/NotifyWorkshopChange.h>

void NotifyWorkshopChange::SerializeRaw(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Change.Serialize(aWriter);
}

void NotifyWorkshopChange::DeserializeRaw(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    ServerMessage::DeserializeRaw(aReader);

    Change.Deserialize(aReader);
}
