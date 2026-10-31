// FalloutBot: verbindet sich wie ein Spieler, aber ohne Spiel, und prüft die Antworten des Servers.
// Aufruf: FalloutBot <host:port> <name> <sekunden> [passwort] [bauen]
// Mit "bauen" schickt der Bot nach der Anmeldung feste Siedlungs-Änderungen.
// Mit "spieltest" schickt er echte Formen für die Werkbank von Red Rocket, siehe scripts/test-workshop-ingame.ps1.
// Exit-Code: 0 = angenommen, 2 = abgewiesen, 3 = keine Verbindung oder Zeitüberschreitung
// Was der Encoding-Code sonst über EncodingPch.h bekommt
#include <chrono>
#include <mutex>
#include <optional>
#include <TiltedCore/Stl.hpp>
#include <TiltedCore/Allocator.hpp>
#include <TiltedCore/Buffer.hpp>
#include <TiltedCore/Serialization.hpp>
#include <TiltedCore/ViewBuffer.hpp>
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

#include <Client.hpp>
#include <Packet.hpp>
#include <BuildInfo.h>

#include <Messages/AuthenticationRequest.h>
#include <Messages/AuthenticationResponse.h>
#include <Messages/NotifyPlayerJoined.h>
#include <Messages/NotifyPlayerLeft.h>
#include <Messages/NotifyWorkshopChange.h>
#include <Messages/RequestWorkshopChange.h>
#include <Messages/ServerMessageFactory.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

using TiltedPhoques::Buffer;

namespace
{
// Aus Fallout4.esm: Referenz RedRocketWorkshopREF, ihre Basisform und Position, dazu ein Bodenteil
constexpr uint32_t kRedRocketWorkbench = 0x54BAE;
constexpr uint32_t kRedRocketWorkbenchBase = 0xC1AEB;
constexpr float kRedRocketWorkbenchX = -70384.328125f;
constexpr float kRedRocketWorkbenchY = 79519.953125f;
constexpr float kRedRocketWorkbenchZ = 7360.030273f;
constexpr uint32_t kTestFloor = 0x1C501C;
constexpr uint32_t kTestGenerator = 0x30386;
constexpr uint32_t kTestPylon = 0x15D76F;
constexpr uint32_t kTestWire = 0x1D971;

const char* ResponseName(AuthenticationResponse::ResponseType aType)
{
    using RT = AuthenticationResponse::ResponseType;
    switch (aType)
    {
    case RT::kAccepted: return "accepted";
    case RT::kWrongVersion: return "wrong_version";
    case RT::kModsMismatch: return "mods_mismatch";
    case RT::kClientModsDisallowed: return "client_mods_disallowed";
    case RT::kWrongPassword: return "wrong_password";
    case RT::kServerFull: return "server_full";
    }
    return "unknown";
}

struct Bot final : TiltedPhoques::Client
{
    std::string Name;
    std::string Password;
    int Result = 3;
    bool Finished = false;
    bool Build = false;
    bool GameTest = false;
    uint32_t PlayerId = 0;

    void SendChange(uint8_t aKind, uint32_t aObjectId, float aX)
    {
        RequestWorkshopChange request;
        request.Change.Kind = aKind;
        request.Change.OwnerId = aObjectId ? PlayerId : 0;
        request.Change.ObjectId = aObjectId;
        if (!aObjectId)
            request.Change.ObjectServerId = GameId(0, 0x1234);
        request.Change.Position = glm::vec3(aX, 0.f, 0.f);
        if (aKind == WorkshopChange::kPlaced)
            request.Change.OriginalPosition = request.Change.Position;
        SendMessage(request);
    }

    void SendGameChange(uint8_t aKind, uint32_t aObjectId, uint32_t aBaseId, const glm::vec3& acPosition, bool aReplay)
    {
        RequestWorkshopChange request;
        request.Change.Kind = aKind;
        request.Change.OwnerId = 99;
        request.Change.ObjectId = aObjectId;
        request.Change.WorkshopId = GameId(0, kRedRocketWorkbench);
        request.Change.BaseId = GameId(0, aBaseId);
        request.Change.Position = acPosition;
        request.Change.OriginalPosition = acPosition;
        request.Change.Replay = aReplay;
        SendMessage(request);
    }

    // Prüft im Spiel: bauen, verschieben, entfernen, und ob Nachholen vorhandene Objekte erkennt
    void SendGameTest()
    {
        const glm::vec3 workbench(kRedRocketWorkbenchX, kRedRocketWorkbenchY, kRedRocketWorkbenchZ);
        const glm::vec3 spotA = workbench + glm::vec3(300.f, 0.f, 0.f);
        const glm::vec3 spotB = workbench + glm::vec3(0.f, 300.f, 0.f);

        SendGameChange(WorkshopChange::kPlaced, 0xFF00A001, kTestFloor, spotA, false);
        SendGameChange(WorkshopChange::kMoved, 0xFF00A001, kTestFloor, spotA + glm::vec3(100.f, 0.f, 0.f), false);
        SendGameChange(WorkshopChange::kPlaced, 0xFF00A002, kTestFloor, spotB, true);
        // Die Werkbank selbst steht schon da, sie darf nicht noch einmal entstehen
        SendGameChange(WorkshopChange::kPlaced, 0xFF00A003, kRedRocketWorkbenchBase, workbench, true);
        SendGameChange(WorkshopChange::kDestroyed, 0xFF00A002, kTestFloor, spotB, false);

        // Generator und Mast mit Kabel, das Kabel kommt wie beim Bauen nach seinen Enden
        const glm::vec3 generator = workbench + glm::vec3(-300.f, 0.f, 0.f);
        const glm::vec3 pylon = workbench + glm::vec3(-300.f, 300.f, 0.f);
        SendGameChange(WorkshopChange::kPlaced, 0xFF00A004, kTestGenerator, generator, false);
        SendGameChange(WorkshopChange::kPlaced, 0xFF00A005, kTestPylon, pylon, false);

        RequestWorkshopChange wire;
        wire.Change.Kind = WorkshopChange::kWired;
        wire.Change.OwnerId = 99;
        wire.Change.ObjectId = 0xFF00A006;
        wire.Change.WorkshopId = GameId(0, kRedRocketWorkbench);
        wire.Change.BaseId = GameId(0, kTestWire);
        wire.Change.Position = (generator + pylon) * 0.5f;
        wire.Change.OriginalPosition = wire.Change.Position;
        wire.Change.HalfExtents = glm::vec3(0.f, 150.f, 0.f);
        wire.Change.Slack = 0.05f;
        wire.Change.Thickness = 1.5f;
        wire.Change.EndA.OwnerId = 99;
        wire.Change.EndA.ObjectId = 0xFF00A004;
        wire.Change.EndA.BaseId = GameId(0, kTestGenerator);
        wire.Change.EndB.OwnerId = 99;
        wire.Change.EndB.ObjectId = 0xFF00A005;
        wire.Change.EndB.BaseId = GameId(0, kTestPylon);
        SendMessage(wire);
    }

    // A wird verschoben, B verschrottet, C stammt aus einem Plugin
    void SendBuildScript()
    {
        SendChange(WorkshopChange::kPlaced, 0xFF000001, 1.f);
        SendChange(WorkshopChange::kMoved, 0xFF000001, 2.f);
        SendChange(WorkshopChange::kPlaced, 0xFF000002, 3.f);
        SendChange(WorkshopChange::kDestroyed, 0xFF000002, 3.f);
        SendChange(WorkshopChange::kMoved, 0, 4.f);
    }

    void SendMessage(const ClientMessage& acMessage)
    {
        Buffer buffer(1 << 16);
        Buffer::Writer writer(&buffer);
        writer.WriteBits(0, 8); // erstes Byte reserviert, wie im echten Client
        acMessage.Serialize(writer);
        TiltedPhoques::PacketView packet(reinterpret_cast<char*>(buffer.GetWriteData()), static_cast<uint32_t>(writer.Size()));
        Send(&packet);
    }

    void OnConnected() override
    {
        std::printf("[%s] verbunden, sende Anmeldung (Version %s)\n", Name.c_str(), BUILD_COMMIT);

        AuthenticationRequest request{};
        request.Version = BUILD_COMMIT;
        request.Username = Name.c_str();
        request.Token = Password.c_str();
        request.Level = 1;

        // Offizielle Masterdateien in Standard-Ladereihenfolge
        const char* masters[] = {"Fallout4.esm", "DLCRobot.esm", "DLCworkshop01.esm", "DLCCoast.esm",
                                 "DLCworkshop02.esm", "DLCworkshop03.esm", "DLCNukaWorld.esm", "DLCUltraHighResolution.esm"};
        uint16_t id = 0;
        for (const char* m : masters)
        {
            auto& entry = request.UserMods.ModList.emplace_back();
            entry.Filename = m;
            entry.Id = id++;
            entry.IsLite = false;
        }

        SendMessage(request);
    }

    void OnConsume(const void* apData, uint32_t aSize) override
    {
        ServerMessageFactory factory;
        TiltedPhoques::ViewBuffer buf((uint8_t*)apData, aSize);
        Buffer::Reader reader(&buf);
        auto pMessage = factory.Extract(reader);
        if (!pMessage)
        {
            std::printf("[%s] FEHLER: Paket nicht lesbar (%u Bytes)\n", Name.c_str(), aSize);
            return;
        }

        const auto opcode = pMessage->GetOpcode();
        if (opcode == AuthenticationResponse::Opcode)
        {
            auto* pResponse = static_cast<AuthenticationResponse*>(pMessage.get());
            std::printf("[%s] Anmeldung: %s (Spieler-ID %u, Server-Version %s)\n", Name.c_str(), ResponseName(pResponse->Type),
                        pResponse->PlayerId, pResponse->Version.c_str());
            Result = pResponse->Type == AuthenticationResponse::ResponseType::kAccepted ? 0 : 2;
            if (Result != 0)
                Finished = true;
            PlayerId = pResponse->PlayerId;
            if (Result == 0 && Build)
                SendBuildScript();
            if (Result == 0 && GameTest)
                SendGameTest();
        }
        else if (opcode == NotifyWorkshopChange::Opcode)
        {
            const auto& change = static_cast<NotifyWorkshopChange*>(pMessage.get())->Change;
            std::printf("[%s] Bauteil %X Art %u Replay %d x %g Ursprung %g\n", Name.c_str(), change.ObjectId, static_cast<unsigned>(change.Kind),
                        change.Replay ? 1 : 0, change.Position.x, change.OriginalPosition.x);
        }
        else if (opcode == NotifyPlayerJoined::Opcode)
        {
            auto* pJoined = static_cast<NotifyPlayerJoined*>(pMessage.get());
            std::printf("[%s] Mitspieler beigetreten: %s (ID %u)\n", Name.c_str(), pJoined->Username.c_str(), pJoined->PlayerId);
        }
        else if (opcode == NotifyPlayerLeft::Opcode)
        {
            std::printf("[%s] Mitspieler gegangen\n", Name.c_str());
        }
        else
        {
            std::printf("[%s] Nachricht %u\n", Name.c_str(), static_cast<unsigned>(opcode));
        }
    }

    void OnDisconnected(EDisconnectReason aReason) override
    {
        std::printf("[%s] getrennt (Grund %d)\n", Name.c_str(), static_cast<int>(aReason));
        Finished = true;
    }

    void OnUpdate() override {}
};
} // namespace

int main(int argc, char** argv)
{
    if (argc < 4)
    {
        std::printf("Aufruf: FalloutBot <host:port> <name> <sekunden> [passwort]\n");
        return 1;
    }

    Bot bot;
    bot.Name = argv[2];
    bot.Password = argc > 4 ? argv[4] : "";
    bot.Build = argc > 5 && std::string(argv[5]) == "bauen";
    bot.GameTest = argc > 5 && std::string(argv[5]) == "spieltest";
    const auto seconds = std::atoi(argv[3]);

    if (!bot.ConnectByIp(argv[1]))
    {
        std::printf("[%s] Verbindungsaufbau zu %s fehlgeschlagen\n", bot.Name.c_str(), argv[1]);
        return 3;
    }

    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    while (!bot.Finished && std::chrono::steady_clock::now() < end)
    {
        bot.Update();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    bot.Close();

    std::printf("[%s] Ergebnis: %d\n", bot.Name.c_str(), bot.Result);
    return bot.Result;
}
