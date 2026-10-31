#include <Structs/WorkshopChange.h>
#include <TiltedCore/Serialization.hpp>

using TiltedPhoques::Serialization;

bool WorkshopChange::WireEnd::operator==(const WireEnd& acRhs) const noexcept
{
    return OwnerId == acRhs.OwnerId && ObjectId == acRhs.ObjectId && ObjectServerId == acRhs.ObjectServerId && BaseId == acRhs.BaseId &&
           LinkType == acRhs.LinkType;
}

void WorkshopChange::WireEnd::Serialize(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    Serialization::WriteVarInt(aWriter, OwnerId);
    Serialization::WriteVarInt(aWriter, ObjectId);
    ObjectServerId.Serialize(aWriter);
    BaseId.Serialize(aWriter);
    Serialization::WriteVarInt(aWriter, LinkType);
}

void WorkshopChange::WireEnd::Deserialize(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    OwnerId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    ObjectId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    ObjectServerId.Deserialize(aReader);
    BaseId.Deserialize(aReader);
    LinkType = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
}

bool WorkshopChange::operator==(const WorkshopChange& acRhs) const noexcept
{
    return Kind == acRhs.Kind && OwnerId == acRhs.OwnerId && ObjectId == acRhs.ObjectId && ObjectServerId == acRhs.ObjectServerId &&
           WorkshopId == acRhs.WorkshopId && BaseId == acRhs.BaseId && Position == acRhs.Position && Rotation == acRhs.Rotation &&
           OriginalPosition == acRhs.OriginalPosition && Replay == acRhs.Replay && EndA == acRhs.EndA && EndB == acRhs.EndB &&
           HalfExtents == acRhs.HalfExtents && Slack == acRhs.Slack && Thickness == acRhs.Thickness;
}

bool WorkshopChange::operator!=(const WorkshopChange& acRhs) const noexcept
{
    return !this->operator==(acRhs);
}

void WorkshopChange::Serialize(TiltedPhoques::Buffer::Writer& aWriter) const noexcept
{
    aWriter.WriteBits(Kind, 8);
    Serialization::WriteVarInt(aWriter, OwnerId);
    Serialization::WriteVarInt(aWriter, ObjectId);
    ObjectServerId.Serialize(aWriter);
    WorkshopId.Serialize(aWriter);
    BaseId.Serialize(aWriter);

    // Full precision, snapped pieces show gaps otherwise
    for (int i = 0; i < 3; ++i)
        Serialization::WriteFloat(aWriter, Position[i]);
    for (int i = 0; i < 3; ++i)
        Serialization::WriteFloat(aWriter, Rotation[i]);
    for (int i = 0; i < 3; ++i)
        Serialization::WriteFloat(aWriter, OriginalPosition[i]);

    Serialization::WriteBool(aWriter, Replay);

    if (Kind != kWired)
        return;

    EndA.Serialize(aWriter);
    EndB.Serialize(aWriter);
    for (int i = 0; i < 3; ++i)
        Serialization::WriteFloat(aWriter, HalfExtents[i]);
    Serialization::WriteFloat(aWriter, Slack);
    Serialization::WriteFloat(aWriter, Thickness);
}

void WorkshopChange::Deserialize(TiltedPhoques::Buffer::Reader& aReader) noexcept
{
    uint64_t kind = 0;
    aReader.ReadBits(kind, 8);
    Kind = kind & 0xFF;

    OwnerId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    ObjectId = Serialization::ReadVarInt(aReader) & 0xFFFFFFFF;
    ObjectServerId.Deserialize(aReader);
    WorkshopId.Deserialize(aReader);
    BaseId.Deserialize(aReader);

    for (int i = 0; i < 3; ++i)
        Position[i] = Serialization::ReadFloat(aReader);
    for (int i = 0; i < 3; ++i)
        Rotation[i] = Serialization::ReadFloat(aReader);
    for (int i = 0; i < 3; ++i)
        OriginalPosition[i] = Serialization::ReadFloat(aReader);

    Replay = Serialization::ReadBool(aReader);

    if (Kind != kWired)
        return;

    EndA.Deserialize(aReader);
    EndB.Deserialize(aReader);
    for (int i = 0; i < 3; ++i)
        HalfExtents[i] = Serialization::ReadFloat(aReader);
    Slack = Serialization::ReadFloat(aReader);
    Thickness = Serialization::ReadFloat(aReader);
}
