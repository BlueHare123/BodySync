// BodySync <-> driver wire protocol (little-endian, x64 Windows). Pure functions: encode/decode with strict validation.
#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace bs::ipc {
constexpr uint32_t kMagic = 0x4E595342;       // 'BSYN'
constexpr uint16_t kVersion = 1;
constexpr uint32_t kMaxPayload = 4096;
constexpr const char* kPipeName = R"(\\.\pipe\BodySync.Driver.v1)";
enum class Type : uint16_t { Hello = 1, HelloAck = 2, Poses = 3, Config = 4, Ping = 5, Pong = 6, Status = 7, Bye = 8 };

#pragma pack(push, 1)
struct Header { uint32_t magic; uint16_t version; uint16_t type; uint32_t size; uint32_t seq; };
struct HelloMsg { uint32_t pid; uint32_t protocol; char name[32]; };
struct HelloAckMsg { uint32_t driverVersion; uint32_t trackerCount; uint32_t enabledMask; char runtime[32]; };
struct PoseWire { uint8_t id; uint8_t flags; uint16_t pad; double pos[3]; double rot[4]; float vel[3]; float angVel[3]; };   // rot = w,x,y,z
struct PosesHeader { uint64_t timestampUs; float timeOffsetSec; uint8_t count; uint8_t pad[3]; };
struct ConfigMsg { uint32_t enabledMask; };
struct StatusMsg { uint32_t flags; uint32_t lastError; uint64_t framesReceived; uint64_t posesPublished; uint32_t activeMask; uint32_t reserved; };
#pragma pack(pop)
static_assert(sizeof(Header) == 16 && sizeof(PoseWire) == 84 && sizeof(PosesHeader) == 16, "wire layout");
enum PoseFlags : uint8_t { kActive = 1, kValid = 2 };
enum StatusFlags : uint32_t { kClientConnected = 1, kDevicesRegistered = 2, kReceivingPoses = 4 };
enum class Result { Ok, Incomplete, BadMagic, BadVersion, BadSize, BadPayload };
const char* resultName(Result r);

struct Pose8 { uint8_t id = 0; bool active = false, valid = false; double pos[3]{}, rot[4]{1, 0, 0, 0}; float vel[3]{}, angVel[3]{}; };
struct PoseFrame { uint64_t timestampUs = 0; float timeOffsetSec = 0; std::vector<Pose8> poses; };
struct Message { Header h{}; std::vector<uint8_t> payload; };

std::vector<uint8_t> encode(Type t, uint32_t seq, const void* payload, uint32_t size);
std::vector<uint8_t> encodeHello(uint32_t seq, uint32_t pid, const std::string& name);
std::vector<uint8_t> encodeHelloAck(uint32_t seq, uint32_t driverVer, uint32_t count, uint32_t mask, const std::string& runtime);
std::vector<uint8_t> encodePoses(uint32_t seq, const PoseFrame& f);
std::vector<uint8_t> encodeConfig(uint32_t seq, uint32_t enabledMask);
std::vector<uint8_t> encodeStatus(uint32_t seq, const StatusMsg& s);
// Extracts one message from the front of buf. Ok => consumed bytes in *used. Incomplete => need more data.
// Any other result means the stream is corrupt: the connection must be dropped.
Result tryExtract(const uint8_t* buf, size_t len, Message& out, size_t* used);
Result decodePoses(const Message& m, PoseFrame& out);          // validates count, finiteness, unit quaternions
Result decodeHello(const Message& m, HelloMsg& out);
Result decodeHelloAck(const Message& m, HelloAckMsg& out);
Result decodeConfig(const Message& m, ConfigMsg& out);
Result decodeStatus(const Message& m, StatusMsg& out);
}  // namespace bs::ipc
