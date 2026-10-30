#include <random>
#include "bodysync/filters.h"
#include "bodysync/ipc_protocol.h"
#include "test_helpers.h"
using namespace bs;

TEST(vec3_basic) {
  Vec3 a{1, 2, 3}, b{-2, 0.5, 4};
  CHECK_NEAR(dot(a, b), -2 + 1 + 12, 1e-12);
  Vec3 c = cross(a, b);
  CHECK_NEAR(dot(c, a), 0, 1e-12); CHECK_NEAR(dot(c, b), 0, 1e-12);
  CHECK_NEAR(Vec3(3, 4, 0).length(), 5, 1e-12);
  CHECK_NEAR(Vec3(0, 0, 0).normalized({0, 1, 0}).y, 1, 1e-12);                 // degenerate -> fallback
  CHECK(!Vec3(NAN, 0, 0).isFinite());
  Vec3 p = constrainDistance({0, 0, 0}, {5, 0, 0}, 2); CHECK_NEAR(p.x, 2, 1e-12);
  CHECK_NEAR(rejectFrom({1, 1, 0}, {0, 1, 0}).y, 0, 1e-12);
}
TEST(quat_rotation_and_composition) {
  Quat q = axisAngle({0, 1, 0}, kPi / 2);
  Vec3 f = q.forward();                                   // facing -Z turned 90deg left -> -X
  CHECK_NEAR(f.x, -1, 1e-12); CHECK_NEAR(f.z, 0, 1e-12);
  Quat r = axisAngle({1, 0, 0}, 0.3) * axisAngle({0, 0, 1}, -0.7);
  Vec3 v{0.3, -1.2, 2};
  Vec3 w = r.conj().rotate(r.rotate(v)); CHECK_NEAR((w - v).length(), 0, 1e-12);
  CHECK_NEAR(r.norm(), 1, 1e-12);
  CHECK_NEAR(Quat(0, 0, 0, 0).normalized().w, 1, 1e-12);          // invalid -> identity
  CHECK_NEAR(Quat(NAN, 0, 0, 1).normalized().w, 1, 1e-12);
}
TEST(quat_slerp_properties) {
  Quat a = axisAngle({0, 1, 0}, 0.2), b = axisAngle({0, 1, 0}, 1.4);
  Quat m = slerp(a, b, 0.5); CHECK_NEAR(angleBetween(m, axisAngle({0, 1, 0}, 0.8)), 0, 1e-9);
  CHECK_NEAR(angleBetween(slerp(a, b, 0), a), 0, 1e-9); CHECK_NEAR(angleBetween(slerp(a, b, 1), b), 0, 1e-9);
  Quat nb{-b.w, -b.x, -b.y, -b.z};                                 // same rotation, opposite sign: must take the short way
  CHECK_NEAR(angleBetween(slerp(a, nb, 0.5), m), 0, 1e-9);
  // constant angular speed
  double a1 = angleBetween(a, slerp(a, b, 0.25)), a2 = angleBetween(slerp(a, b, 0.25), slerp(a, b, 0.5));
  CHECK_NEAR(a1, a2, 1e-9);
  CHECK_NEAR(angleBetween(slerp(a, a, 0.3), a), 0, 1e-9);          // identical inputs
  // never goes through Euler gimbal: 179deg yaw to -179deg yaw crosses 180, not 358
  Quat y1 = yawQuat(deg2rad(179)), y2 = yawQuat(deg2rad(-179));
  CHECK_NEAR(rad2deg(angleBetween(y1, y2)), 2, 1e-6);
  CHECK_NEAR(std::fabs(rad2deg(yawOf(slerp(y1, y2, 0.5)))), 180, 1e-6);
}
TEST(basis_and_frames) {
  Quat q = frameFromUpFwd({0, 1, 0}, {0, 0, -1});                  // identity pose
  CHECK_NEAR(angleBetween(q, Quat{}), 0, 1e-9);
  Quat l = frameFromUpFwd({0, 0, -1}, {0, 1, 0});                  // lying on back, head to -Z, face up
  CHECK_NEAR((l.up() - Vec3{0, 0, -1}).length(), 0, 1e-9); CHECK_NEAR((l.forward() - Vec3{0, 1, 0}).length(), 0, 1e-9);
  CHECK_NEAR(dot(l.right(), l.up()), 0, 1e-9);
  Quat d = frameFromUpFwd({0, 1, 0}, {0, 1, 0});                   // parallel -> still a valid rotation
  CHECK_NEAR(d.norm(), 1, 1e-9); CHECK(d.isFinite());
  for (int i = 0; i < 200; ++i) {                                  // round trip through basis for random rotations
    std::mt19937 g(i); std::uniform_real_distribution<double> u(-1, 1);
    Quat r = Quat{u(g), u(g), u(g), u(g)}.normalized();
    Quat r2 = fromBasis(r.right(), r.up(), r.rotate({0, 0, 1}));
    CHECK_NEAR(angleBetween(r, r2), 0, 1e-7);
  }
}
TEST(coordinate_conversion_openvr_matrix) {
  // OpenVR HmdMatrix34_t: rotation 90deg about +Y (turn left) + translation (1,2,3)
  float m[3][4] = {{0, 0, 1, 1}, {0, 1, 0, 2}, {-1, 0, 0, 3}};
  Pose p = poseFromMat34(m);
  CHECK_NEAR(p.p.x, 1, 1e-6); CHECK_NEAR(p.p.y, 2, 1e-6); CHECK_NEAR(p.p.z, 3, 1e-6);
  CHECK_NEAR((p.q.forward() - Vec3{-1, 0, 0}).length(), 0, 1e-6);
  CHECK_NEAR(rad2deg(yawOf(p.q)), 90, 1e-4);
  float back[3][4]; poseToMat34(p, back);
  for (int r = 0; r < 3; ++r) for (int c = 0; c < 4; ++c) CHECK_NEAR(back[r][c], m[r][c], 1e-6);
  Pose a{{1, 0, 0}, yawQuat(kPi / 2)}, b{{0, 0, -1}, Quat{}};
  Pose ab = a * b;                                                  // child (0,0,-1) forward of parent facing -X
  CHECK_NEAR((ab.p - Vec3{0, 0, 0}).length(), 0, 1e-9);
  Pose id = a * a.inverse(); CHECK_NEAR(id.p.length(), 0, 1e-9); CHECK_NEAR(angleBetween(id.q, Quat{}), 0, 1e-9);
  CHECK_NEAR(wrapPi(2.5 * kPi), 0.5 * kPi, 1e-9);   // wraps into [-pi, pi)
  CHECK_NEAR(wrapPi(0.5), 0.5, 1e-12); CHECK_NEAR(wrapPi(-4), 2 * kPi - 4, 1e-12);
}
TEST(two_bone_ik_constraints) {
  double l1 = 0.43, l2 = 0.43;
  for (int i = 0; i < 500; ++i) {
    std::mt19937 g(i); std::uniform_real_distribution<double> u(-1, 1);
    Vec3 root{u(g), u(g), u(g)}, end = root + Vec3{u(g), u(g), u(g)} * 1.2, pole{u(g), u(g), u(g)};
    Vec3 e = end;
    Vec3 mid = twoBoneIK(root, e, l1, l2, pole, deg2rad(145));
    CHECK(mid.isFinite() && e.isFinite());
    CHECK_NEAR(distance(root, mid), l1, 1e-9); CHECK_NEAR(distance(mid, e), l2, 1e-9);   // segment lengths preserved exactly
    CHECK(distance(root, e) <= (l1 + l2) * 0.9999 + 1e-9);
  }
  Vec3 e{0, -0.5, 0.3}; Vec3 mid = twoBoneIK({0, 0, 0}, e, l1, l2, {0, 0, -1}, deg2rad(145));
  CHECK(mid.z < -0.05);                                              // knee bulges toward the pole (forward = -Z)
  Vec3 far{0, -5, 0}; twoBoneIK({0, 0, 0}, far, l1, l2, {0, 0, -1}, deg2rad(145)); CHECK_NEAR(far.length(), (l1 + l2) * 0.9999, 1e-9);   // out of reach is clamped
  Vec3 near{0, -0.0001, 0}; twoBoneIK({0, 0, 0}, near, l1, l2, {0, 0, -1}, deg2rad(100));
  CHECK_NEAR(near.length(), chordForBend(l1, l2, deg2rad(100)), 1e-9);   // bend limit enforced as a minimum reach
  Vec3 same{0, 0, 0}; Vec3 m2 = twoBoneIK({0, 0, 0}, same, l1, l2, {0, 0, 0}, deg2rad(145)); CHECK(m2.isFinite());   // degenerate input stays finite
}
TEST(filters_smooth_and_converge) {
  FilterParams fp = filterFromSmoothing(0.8); OneEuroVec3 f; std::mt19937 g(1); std::normal_distribution<double> n(0, 0.001);
  double in2 = 0, out2 = 0; Vec3 last;
  for (int i = 0; i < 900; ++i) { Vec3 x{1 + n(g), 2 + n(g), 3 + n(g)}; last = f.apply(x, 1.0 / 90, fp); if (i > 200) { in2 += 3e-6; out2 += dot(last - Vec3{1, 2, 3}, last - Vec3{1, 2, 3}); } }
  CHECK_MSG(out2 < in2 * 0.3, "noise %g -> %g", in2, out2);                                           // noise strongly reduced
  CHECK_NEAR(last.x, 1, 0.005);
  // fast motion must not lag far behind (adaptive cutoff)
  OneEuroVec3 f2; Vec3 o; for (int i = 0; i < 90; ++i) o = f2.apply({i * 0.02, 0, 0}, 1.0 / 90, filterFromSmoothing(0.8));
  CHECK(std::fabs(o.x - 89 * 0.02) < 0.15);
  OneEuroVec3 f3; f3.apply({0, 0, 0}, 0.011, fp); Vec3 j = f3.apply({5, 0, 0}, 1.0, fp); CHECK_NEAR(j.x, 5, 1e-12);   // long gap resets
  OneEuroQuat fq; Quat qo; for (int i = 0; i < 300; ++i) qo = fq.apply(axisAngle({0, 1, 0}, 0.5 + 0.02 * std::sin(i * 3.0)), 1.0 / 90, fp);
  CHECK_NEAR(qo.norm(), 1, 1e-9); CHECK(angleBetween(qo, axisAngle({0, 1, 0}, 0.5)) < 0.02);
  CHECK(filterFromSmoothing(1).minCutoff < filterFromSmoothing(0).minCutoff);
}

// ---- IPC protocol ----
TEST(ipc_protocol_roundtrip) {
  using namespace bs::ipc;
  PoseFrame f; f.timestampUs = 123456789; f.timeOffsetSec = -0.01f;
  for (int i = 0; i < 8; ++i) { Pose8 p; p.id = (uint8_t)i; p.active = true; p.valid = i != 3; p.pos[0] = i; p.pos[1] = 1.5; p.pos[2] = -2; p.rot[0] = 1; p.vel[0] = 0.5f; f.poses.push_back(p); }
  auto bytes = encodePoses(7, f);
  Message m; size_t used = 0;
  CHECK(tryExtract(bytes.data(), bytes.size(), m, &used) == Result::Ok); CHECK(used == bytes.size()); CHECK(m.h.seq == 7);
  PoseFrame g; CHECK(decodePoses(m, g) == Result::Ok);
  CHECK(g.poses.size() == 8); CHECK(g.timestampUs == 123456789); CHECK(!g.poses[3].valid); CHECK_NEAR(g.poses[5].pos[0], 5, 0);
  auto h = encodeHello(1, 4242, "BodySync"); CHECK(tryExtract(h.data(), h.size(), m, &used) == Result::Ok);
  HelloMsg hm; CHECK(decodeHello(m, hm) == Result::Ok); CHECK(hm.pid == 4242); CHECK(std::string(hm.name) == "BodySync");
  auto a = encodeHelloAck(2, 100, 8, 0xFF, "SteamVR"); CHECK(tryExtract(a.data(), a.size(), m, &used) == Result::Ok);
  HelloAckMsg am; CHECK(decodeHelloAck(m, am) == Result::Ok); CHECK(am.trackerCount == 8 && std::string(am.runtime) == "SteamVR");
  auto c = encodeConfig(3, 0x55); tryExtract(c.data(), c.size(), m, &used); ConfigMsg cm; CHECK(decodeConfig(m, cm) == Result::Ok && cm.enabledMask == 0x55);
  StatusMsg sm{}; sm.framesReceived = 99; auto s = encodeStatus(4, sm); tryExtract(s.data(), s.size(), m, &used); StatusMsg sm2; CHECK(decodeStatus(m, sm2) == Result::Ok && sm2.framesReceived == 99);
}
TEST(ipc_protocol_rejects_malformed) {
  using namespace bs::ipc;
  PoseFrame f; Pose8 p; p.valid = true; p.active = true; f.poses.push_back(p);
  auto good = encodePoses(1, f); Message m; size_t used = 0;
  for (size_t cut = 0; cut < good.size(); ++cut) CHECK(tryExtract(good.data(), cut, m, &used) == Result::Incomplete);   // every truncation = incomplete, never UB
  auto bad = good; bad[0] ^= 0xFF; CHECK(tryExtract(bad.data(), bad.size(), m, &used) == Result::BadMagic);
  bad = good; bad[4] = 9; CHECK(tryExtract(bad.data(), bad.size(), m, &used) == Result::BadVersion);
  bad = good; bad[8] = 0xFF; bad[9] = 0xFF; bad[10] = 0xFF; bad[11] = 0x7F; CHECK(tryExtract(bad.data(), bad.size(), m, &used) == Result::BadSize);
  bad = good; bad[6] = 99; CHECK(tryExtract(bad.data(), bad.size(), m, &used) == Result::BadSize);                        // unknown type
  PoseFrame o;
  Pose8 q = p; q.pos[0] = NAN; f.poses = {q}; auto b1 = encodePoses(1, f); tryExtract(b1.data(), b1.size(), m, &used); CHECK(decodePoses(m, o) == Result::BadPayload);
  q = p; q.rot[0] = 0; q.rot[1] = 5; f.poses = {q}; auto b2 = encodePoses(1, f); tryExtract(b2.data(), b2.size(), m, &used); CHECK(decodePoses(m, o) == Result::BadPayload);   // not unit
  q = p; q.id = 9; f.poses = {q}; auto b3 = encodePoses(1, f); tryExtract(b3.data(), b3.size(), m, &used); CHECK(decodePoses(m, o) == Result::BadPayload);
  q = p; q.valid = false; q.pos[0] = NAN; q.rot[0] = 0; f.poses = {q}; auto b4 = encodePoses(1, f); tryExtract(b4.data(), b4.size(), m, &used); CHECK(decodePoses(m, o) == Result::Ok);   // invalid pose may carry junk
  Message wrong; wrong.h.type = (uint16_t)Type::Poses; wrong.payload = {1, 2, 3}; CHECK(decodePoses(wrong, o) == Result::BadPayload);
  std::mt19937 g(5); int ok = 0;                                      // random garbage never crashes and never yields a decoded pose frame with junk
  for (int i = 0; i < 20000; ++i) { std::vector<uint8_t> r(g() % 200); for (auto& b : r) b = (uint8_t)g(); if (tryExtract(r.data(), r.size(), m, &used) == Result::Ok) { PoseFrame x; if (decodePoses(m, x) == Result::Ok) ++ok; } }
  CHECK(ok == 0);
}
