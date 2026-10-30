#include "bodysync/ipc_protocol.h"
#include <cmath>

namespace bs::ipc {
const char* resultName(Result r) {
  switch (r) { case Result::Ok: return "ok"; case Result::Incomplete: return "incomplete"; case Result::BadMagic: return "bad magic"; case Result::BadVersion: return "bad version"; case Result::BadSize: return "bad size"; default: return "bad payload"; }
}
std::vector<uint8_t> encode(Type t, uint32_t seq, const void* payload, uint32_t size) {
  Header h{kMagic, kVersion, (uint16_t)t, size, seq};
  std::vector<uint8_t> v(sizeof h + size);
  std::memcpy(v.data(), &h, sizeof h);
  if (size) std::memcpy(v.data() + sizeof h, payload, size);
  return v;
}
static void copyStr(char* dst, size_t n, const std::string& s) { std::memset(dst, 0, n); std::memcpy(dst, s.data(), std::min(s.size(), n - 1)); }
std::vector<uint8_t> encodeHello(uint32_t seq, uint32_t pid, const std::string& name) { HelloMsg m{}; m.pid = pid; m.protocol = kVersion; copyStr(m.name, sizeof m.name, name); return encode(Type::Hello, seq, &m, sizeof m); }
std::vector<uint8_t> encodeHelloAck(uint32_t seq, uint32_t dv, uint32_t n, uint32_t mask, const std::string& rt) { HelloAckMsg m{}; m.driverVersion = dv; m.trackerCount = n; m.enabledMask = mask; copyStr(m.runtime, sizeof m.runtime, rt); return encode(Type::HelloAck, seq, &m, sizeof m); }
std::vector<uint8_t> encodeConfig(uint32_t seq, uint32_t mask) { ConfigMsg m{mask}; return encode(Type::Config, seq, &m, sizeof m); }
std::vector<uint8_t> encodeStatus(uint32_t seq, const StatusMsg& s) { return encode(Type::Status, seq, &s, sizeof s); }
std::vector<uint8_t> encodePoses(uint32_t seq, const PoseFrame& f) {
  size_t n = std::min<size_t>(f.poses.size(), 16);
  std::vector<uint8_t> pl(sizeof(PosesHeader) + n * sizeof(PoseWire));
  PosesHeader ph{}; ph.timestampUs = f.timestampUs; ph.timeOffsetSec = f.timeOffsetSec; ph.count = (uint8_t)n;
  std::memcpy(pl.data(), &ph, sizeof ph);
  for (size_t i = 0; i < n; ++i) {
    const Pose8& p = f.poses[i]; PoseWire w{};
    w.id = p.id; w.flags = (p.active ? kActive : 0) | (p.valid ? kValid : 0);
    std::memcpy(w.pos, p.pos, sizeof w.pos); std::memcpy(w.rot, p.rot, sizeof w.rot); std::memcpy(w.vel, p.vel, sizeof w.vel); std::memcpy(w.angVel, p.angVel, sizeof w.angVel);
    std::memcpy(pl.data() + sizeof ph + i * sizeof w, &w, sizeof w);
  }
  return encode(Type::Poses, seq, pl.data(), (uint32_t)pl.size());
}
Result tryExtract(const uint8_t* buf, size_t len, Message& out, size_t* used) {
  if (len < sizeof(Header)) return Result::Incomplete;
  Header h; std::memcpy(&h, buf, sizeof h);
  if (h.magic != kMagic) return Result::BadMagic;
  if (h.version != kVersion) return Result::BadVersion;
  if (h.size > kMaxPayload || h.type < 1 || h.type > (uint16_t)Type::Bye) return Result::BadSize;
  if (len < sizeof h + h.size) return Result::Incomplete;
  out.h = h; out.payload.assign(buf + sizeof h, buf + sizeof h + h.size);
  *used = sizeof h + h.size;
  return Result::Ok;
}
template <class T> static Result fixed(const Message& m, T& out) {
  if (m.payload.size() != sizeof(T)) return Result::BadPayload;
  std::memcpy(&out, m.payload.data(), sizeof(T)); return Result::Ok;
}
Result decodeHello(const Message& m, HelloMsg& o) { Result r = fixed(m, o); if (r == Result::Ok) o.name[sizeof o.name - 1] = 0; return r; }
Result decodeHelloAck(const Message& m, HelloAckMsg& o) { Result r = fixed(m, o); if (r == Result::Ok) o.runtime[sizeof o.runtime - 1] = 0; return r; }
Result decodeConfig(const Message& m, ConfigMsg& o) { return fixed(m, o); }
Result decodeStatus(const Message& m, StatusMsg& o) { return fixed(m, o); }
Result decodePoses(const Message& m, PoseFrame& out) {
  if (m.payload.size() < sizeof(PosesHeader)) return Result::BadPayload;
  PosesHeader ph; std::memcpy(&ph, m.payload.data(), sizeof ph);
  if (ph.count > 16 || m.payload.size() != sizeof ph + ph.count * sizeof(PoseWire) || !std::isfinite(ph.timeOffsetSec)) return Result::BadPayload;
  out.timestampUs = ph.timestampUs; out.timeOffsetSec = ph.timeOffsetSec; out.poses.clear();
  for (unsigned i = 0; i < ph.count; ++i) {
    PoseWire w; std::memcpy(&w, m.payload.data() + sizeof ph + i * sizeof w, sizeof w);
    Pose8 p; p.id = w.id; p.active = w.flags & kActive; p.valid = w.flags & kValid;
    if (w.id >= 8) return Result::BadPayload;
    std::memcpy(p.pos, w.pos, sizeof p.pos); std::memcpy(p.rot, w.rot, sizeof p.rot); std::memcpy(p.vel, w.vel, sizeof p.vel); std::memcpy(p.angVel, w.angVel, sizeof p.angVel);
    if (p.valid) {   // a valid pose must be finite with a unit quaternion and a sane position
      double n2 = 0; for (double r : p.rot) { if (!std::isfinite(r)) return Result::BadPayload; n2 += r * r; }
      for (double c : p.pos) if (!std::isfinite(c) || std::fabs(c) > 1000) return Result::BadPayload;
      for (float c : p.vel) if (!std::isfinite(c)) return Result::BadPayload;
      for (float c : p.angVel) if (!std::isfinite(c)) return Result::BadPayload;
      if (std::fabs(n2 - 1.0) > 0.01) return Result::BadPayload;
    }
    out.poses.push_back(p);
  }
  return Result::Ok;
}
}  // namespace bs::ipc
