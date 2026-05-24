// View-matrix injection for Black & White (2001). See engine_addresses.h
// for the engine memory map this hook reads and rewrites.
//
// Head-tracking strategy (matches the AGENTS.md doctrine):
//   Hook the tail of both camera builders (FUN_00819920 / FUN_00819f50).
//   Leave g_cameraStruct untouched - game logic gets clean camera, so MMB
//   grab raycasts the world point the user is actually pointing at,
//   projectile aim is unchanged, etc. Then rebuild g_scaledMatrix and
//   g_mirrorMatrix from a head-rotated copy of g_cameraStruct so the player
//   sees the rotated view.
//
// SetTransform(D3DTS_VIEW) at the end of both engine functions is dead -
// B&W does CPU-side T&L with D3DFVF_XYZRHW verts. The downstream matrix
// is the only one that affects rendering.

#include "camera_hook.h"

#include <Windows.h>
#include <intrin.h>
#include <atomic>
#include <cmath>

#include "cameraunlock/hooks/hook_manager.h"
#include "engine_addresses.h"
#include "water_surface.h"
#include "plugin.h"
#include "debug_log.h"

namespace headtracking {

namespace {

// Hook targets (engine functions we detour). The data globals these touch
// live in engine_addresses.h.
constexpr uintptr_t kFn_19920_Addr    = 0x00819920;  // per-frame gameplay camera builder
constexpr uintptr_t kFn_19f50_Addr    = 0x00819F50;  // scripted/cutscene builder (also takes a rotmat)
constexpr uintptr_t kFn_Water_Addr    = 0x00879930;  // LH3DWater::Render
constexpr uintptr_t kFn_WorldRender_Addr = 0x0054DA80;  // top-level frame render dispatcher
constexpr uintptr_t kFn_ScreenToWorld_Addr = 0x0081B370;  // screen-point -> world ray (reads g_cameraStruct)
constexpr uintptr_t kFn_ObjectScreenPick_Addr = 0x00519960;  // object screen-pick helper
constexpr uintptr_t kFn_InputCursorClamp_Addr = 0x007E49A0;  // input cursor clamp
constexpr uintptr_t kFn_SetCursorPosition_Addr = 0x007E4E40;  // OS cursor warp
constexpr uintptr_t kFn_CitadelGlow_Addr = 0x007954A0;  // citadel/temple-room glow + lightmap draw
constexpr uintptr_t kFn_CursorEffect_Addr = 0x00576F20;  // cursor-edge effect / rotation-strength gate processor
constexpr uintptr_t kFn_PickOrchestrator_Addr = 0x005E42E0;  // per-frame "what's under the cursor": snapshots kCursorX/Y -> DAT_ea1ac8/cc + calls S2W

// True while g_cameraStruct holds the head-rotated copy (during the world
// render and post-world HUD sandwiches). Declared early so the DrawPrimitive
// capture below can record it. Defined here, used by the sandwich hooks too.
std::atomic<bool> g_inSandwich{false};
// Clean (un-rotated) camera matrix saved by the world-render sandwich at its
// entry, restored at exit. Valid (= the clean camera) while inSandwich.
float g_cleanCameraSave[kViewMatrixFloats] = {0};

// Particles, shadow projectors and other CPU-projected primitives read
// g_cameraStruct directly to compute screen positions. With only the
// scaled-matrix path rotated, those primitives stay anchored to the
// clean (unrotated) view -> they look screen-locked / counter-moving
// when the head turns. Fix: sandwich the world-render dispatcher
// (FUN_005E42E0) with a rotated copy of g_cameraStruct so every render
// path sees the rotated camera; restore on exit so game logic, MMB
// raycast, AI vision etc. continue to see the clean matrix.
//
// Water remains a special case: its per-vertex ray cast (FUN_0081b370)
// combined with the rotated projection produces a roll-induced warp.
// Inside Hook_Water we swap BOTH g_cameraStruct AND g_scaledMatrix
// back to clean so the water plane renders horizontal in world space.
std::atomic<bool> g_haveHook{false};
float g_rotatedMatrix[kViewMatrixFloats] = {0};

// Set while inside the citadel/temple-room glow gate (FUN_007954A0) so the
// corona draw can suppress only those glows under head tracking. See the TODO
// at Hook_CitadelGlow for why they are disabled rather than reprojected.
std::atomic<bool> g_inCitadelGlow{false};

// Eye-to-target distance (zoom level), captured from the camera builders.
std::atomic<float> g_focalDistance{0.0f};
std::atomic<int> g_virtualCursorX{0};
std::atomic<int> g_virtualCursorY{0};
std::atomic<bool> g_virtualCursorActive{false};
// The engine's input object: caches the `this` pointer captured by either
// Hook_InputCursorClamp or Hook_SetCursorPosition so the cage thread can write
// the unclamped cursor position straight into its mirrors at +0xbc/+0xc4/+0xd0
// without waiting for the engine to call one of the hooked entry points.
std::atomic<uintptr_t> g_inputObject{0};

// Head-turn pixel offset of the rendered (head-rotated) view centre within the
// CLEAN camera's cursor space. The cursor cage subtracts this from kCursorX/Y to
// get the on-screen hand position and clamps THAT to the screen rect, so the
// hand stays in the visible play area under head rotation. Refreshed by
// UpdateCursorBoxOffset each camera build.
std::atomic<float> g_cursorBoxOffsetX{0.0f};
std::atomic<float> g_cursorBoxOffsetY{0.0f};

// Pixel shift the pick orchestrator (FUN_005E42E0) applies to kCursorX/Y before
// snapshotting them into DAT_00ea1ac8/cc and calling FUN_0081B370. The shift is
// the NDC delta of the head-rotated forward axis projected through the clean
// camera's pixel space, which - to first order, for any near-centre world point
// - equals the screen-space offset between rotated_proj(W) and clean_proj(W).
// The pick downstream compares projected object positions (via the head-rotated
// g_scaledMatrix) against DAT_00ea1ac8/cc; without this shift the cursor stays
// at the pre-rotation pixel while every visible world point has moved by this
// delta, so the comparison misses. Refreshed by ComputePickCursorShift each
// camera build; consumed by Hook_PickOrchestrator.
std::atomic<float> g_pickShiftX{0.0f};
std::atomic<float> g_pickShiftY{0.0f};

// World-space eye translation from positional tracking, in the BODY frame
// (derived from the clean camera basis, so it does not rotate with the head).
// Written each frame by ApplyHeadRotationToRenderMatrix; the name-box S2W hook
// shifts g_cameraPivot by this same vector so its unproject matches the render.
float g_posEyeShiftWorld[3] = {0};

void CopyViewMatrix(float* dst, const float* src) {
    for (int i = 0; i < kViewMatrixFloats; ++i) dst[i] = src[i];
}

// Stash the live matrix into `save`, then overwrite it with `replacement`.
// Pair with CopyViewMatrix(live, save) to restore.
void StashAndReplaceViewMatrix(float* live, const float* replacement, float* save) {
    for (int i = 0; i < kViewMatrixFloats; ++i) {
        save[i] = live[i];
        live[i] = replacement[i];
    }
}

// Project a clean/rotated 4x3 view basis into the engine's render matrices:
// scale columns by (sx, sy, 1) and write both g_scaledMatrix and its byte
// copy g_mirrorMatrix. This is the same projection the engine's camera
// builder applies; we re-run it whenever we substitute a basis for rendering.
void WriteScaledAndMirror(const float* basis) {
    const float sx = *reinterpret_cast<const float*>(kScaleXAddr);
    const float sy = *reinterpret_cast<const float*>(kScaleYAddr);
    float* scaled = reinterpret_cast<float*>(kScaledMatrixAddr);
    float* mirror = reinterpret_cast<float*>(kMirrorMatrixAddr);
    for (int row = 0; row < 4; ++row) {
        const float a = basis[row * 3 + 0] * sx;
        const float b = basis[row * 3 + 1] * sy;
        const float c = basis[row * 3 + 2];
        scaled[row * 3 + 0] = a; scaled[row * 3 + 1] = b; scaled[row * 3 + 2] = c;
        mirror[row * 3 + 0] = a; mirror[row * 3 + 1] = b; mirror[row * 3 + 2] = c;
    }
}

struct Mat3 { float r[3][3]; };

Mat3 BuildYprRowVector(float yaw, float pitch, float roll) {
    const float cy = std::cos(yaw),   sy = std::sin(yaw);
    const float cp = std::cos(pitch), sp = std::sin(pitch);
    const float cr = std::cos(roll),  sr = std::sin(roll);
    return {{
        { cy*cr - sy*sp*sr,  cy*sr + sy*sp*cr,  -sy*cp },
        { -cp*sr,            cp*cr,              sp    },
        { sy*cr + cy*sp*sr,  sy*sr - cy*sp*cr,   cy*cp }
    }};
}

Mat3 BuildPitchRollRowVector(float pitch, float roll) {
    const float cp = std::cos(pitch), sp = std::sin(pitch);
    const float cr = std::cos(roll),  sr = std::sin(roll);
    return {{
        { cr,     sr,     0  },
        { -cp*sr, cp*cr,  sp },
        { sp*sr,  -sp*cr, cp }
    }};
}

// Pixel offset between kCursorX/Y (clean) and the on-screen hand position
// (rotated). Per-axis tan-of-input, clamped: pure yaw only shifts X, pure pitch
// only shifts Y. This matches the visible shift under world-Y (horizon-locked)
// rotation, where the rendered horizon doesn't tilt so the cage shouldn't tilt
// either. Under camera-local rotation the rendered horizon DOES tilt with yaw
// but the cage stays rectangular - that mode trades geometric fidelity for the
// no-arc cursor behaviour the user prefers.
void UpdateCursorBoxOffset(float yaw_r, float pitch_r) {
    constexpr float kMaxAngleRad = 1.047f;  // 60°
    const float yaw_c   = std::max(-kMaxAngleRad, std::min(kMaxAngleRad, yaw_r));
    const float pitch_c = std::max(-kMaxAngleRad, std::min(kMaxAngleRad, pitch_r));
    const float fwd   = *reinterpret_cast<const float*>(kViewForwardAddr);
    const float kx    = *reinterpret_cast<const float*>(kProjScaleXAddr);
    const float ky    = *reinterpret_cast<const float*>(kProjScaleYAddr);
    const float halfX = *reinterpret_cast<const float*>(kScreenHalfXAddr);
    const float halfY = *reinterpret_cast<const float*>(kScreenHalfYAddr);
    g_cursorBoxOffsetX.store( std::tan(yaw_c)   * (fwd / kx) * halfX, std::memory_order_release);
    g_cursorBoxOffsetY.store(-std::tan(pitch_c) * (fwd / ky) * halfY, std::memory_order_release);
}

// Compute the pixel shift to apply to kCursorX/Y inside the pick orchestrator
// (FUN_005E42E0) so that the cursor lines up with what is drawn under it through
// the head-rotated view.
//
// Math: project the head-rotated forward axis through the CLEAN projection.
// For a world point W on the camera's optical axis at depth d, clean_proj(W) is
// the screen centre and rotated_proj(W) lands at (ndc_x, ndc_y) computed below.
// To first order, every other near-centre world point shifts by the same NDC
// delta. The pick compares against an unshifted cursor while seeing world
// points through the rotated projection; subtracting this delta from the
// cursor aligns them.
//
// We use the spherical decomposition (per AGENTS.md reticle-projection notes,
// not the naive per-axis-tangent formula) so roll combines correctly: roll is
// applied to the direction vector before perspective-divide, not to NDC.
void ComputePickCursorShift(float yaw_r, float pitch_r, float roll_r) {
    constexpr float kMaxAngleRad = 1.047f;  // 60°, matches UpdateCursorBoxOffset clamp
    const float y_r = std::max(-kMaxAngleRad, std::min(kMaxAngleRad, yaw_r));
    const float p_r = std::max(-kMaxAngleRad, std::min(kMaxAngleRad, pitch_r));

    const float sy = std::sin(y_r), cy = std::cos(y_r);
    const float sp = std::sin(p_r), cp = std::cos(p_r);
    const float sr = std::sin(roll_r), cr = std::cos(roll_r);

    // Rotated forward axis in clean camera coords.
    const float ax = -sy;
    const float ay =  sp * cy;
    const float az =  cp * cy;

    // Roll applied in direction space (NOT screen space) per AGENTS.md.
    const float rx = ax * cr - ay * sr;
    const float ry = ax * sr + ay * cr;

    if (az < 1e-3f) {
        g_pickShiftX.store(0.0f, std::memory_order_release);
        g_pickShiftY.store(0.0f, std::memory_order_release);
        return;
    }

    const float kx = *reinterpret_cast<const float*>(kProjScaleXAddr);
    const float ky = *reinterpret_cast<const float*>(kProjScaleYAddr);
    const float halfX = *reinterpret_cast<const float*>(kScreenHalfXAddr);
    const float halfY = *reinterpret_cast<const float*>(kScreenHalfYAddr);

    const float ndc_x =  rx / az / kx;
    const float ndc_y = -ry / az / ky;

    // ndc_x is in D3D NDC (X grows rightward, same as pixels) so the screen
    // shift in pixels is ndc_x * halfX directly. ndc_y is in AGENTS.md's
    // "+Y is up in NDC" convention but our pixels are Y-down, so we flip:
    // pixel_shift_y = -ndc_y * halfY. Both reflect where a world point lands
    // in clean pixel space after head rotation, which is exactly the cursor
    // shift needed to keep raw_cursor aligned with the visible target.
    g_pickShiftX.store( ndc_x * halfX, std::memory_order_release);
    g_pickShiftY.store(-ndc_y * halfY, std::memory_order_release);
}

void ApplyHeadRotationToRenderMatrix() {
    float yaw = 0.0f, pitch = 0.0f, roll = 0.0f;
    const bool tracking_active = GetPlugin().GetCurrentRotationRadians(yaw, pitch, roll);

    const float* clean = reinterpret_cast<const float*>(kCameraStructAddr);

    if (!tracking_active) {
        g_cursorBoxOffsetX.store(0.0f, std::memory_order_release);
        g_cursorBoxOffsetY.store(0.0f, std::memory_order_release);
        g_pickShiftX.store(0.0f, std::memory_order_release);
        g_pickShiftY.store(0.0f, std::memory_order_release);
        // Mirror this frame's clean into g_rotatedMatrix so the world-
        // render sandwich and the water hook never see stale rotated
        // data from a previous frame (which would draw the entire
        // engine scene with a mismatched view matrix -> black screen
        // during UDP packet drops, fast mouse rotation, fast head
        // movement). Don't touch scaled - the engine already wrote it
        // from clean this frame.
        CopyViewMatrix(g_rotatedMatrix, clean);
        g_posEyeShiftWorld[0] = g_posEyeShiftWorld[1] = g_posEyeShiftWorld[2] = 0.0f;
        return;
    }

    ComputePickCursorShift(yaw, pitch, roll);

    // B&W's view matrix uses row-vector convention (D3D standard): p_view =
    // p_world * V. In that convention `rotated = V * R` rotates IN VIEW SPACE
    // (camera-local), while left-multiplying V by a world-Y rotation rotates
    // IN WORLD SPACE (horizon-locked yaw). The two branches below pick which
    // composition to use.
    const bool world_yaw = GetPlugin().IsWorldSpaceYaw();
    float base[12];
    Mat3 R;
    if (world_yaw) {
        // True horizon-locked yaw: pre-multiply clean's upper 3x3 by row-form
        // Ry(-yaw) so the rotation lives in world space, then compose pitch/roll
        // in view space via the post-multiplied R below. Translation row is
        // recomputed from the unchanged eye so the camera stays put.
        const float c = std::cos(-yaw), s = std::sin(-yaw);
        for (int j = 0; j < 3; ++j) {
            base[0 * 3 + j] =  c * clean[0 * 3 + j] - s * clean[2 * 3 + j];
            base[1 * 3 + j] =      clean[1 * 3 + j];
            base[2 * 3 + j] =  s * clean[0 * 3 + j] + c * clean[2 * 3 + j];
        }
        const float* eye = reinterpret_cast<const float*>(kCameraPivotAddr);
        for (int j = 0; j < 3; ++j) {
            base[3 * 3 + j] = -(eye[0] * base[0 * 3 + j]
                              + eye[1] * base[1 * 3 + j]
                              + eye[2] * base[2 * 3 + j]);
        }
        R = BuildPitchRollRowVector(pitch, -roll);
    } else {
        // Camera-local yaw: bake yaw into the post-multiplied R alongside pitch
        // and roll. All three rotations live in view space, so when the camera
        // is pitched the rendered horizon tilts as the head yaws.
        CopyViewMatrix(base, clean);
        R = BuildYprRowVector(-yaw, pitch, -roll);
    }

    // Always rebuild g_rotatedMatrix from the CURRENT clean. Earlier code
    // early-returned when rotation was zero, leaving g_rotatedMatrix stale
    // from previous frames. The water hook reads g_rotatedMatrix every
    // frame so it must always reflect the current camera; the math costs
    // ~36 multiplies, not worth optimising out.
    float rotated[12];
    for (int row = 0; row < 4; ++row) {
        const float x = base[row * 3 + 0];
        const float y = base[row * 3 + 1];
        const float z = base[row * 3 + 2];
        rotated[row * 3 + 0] = x * R.r[0][0] + y * R.r[1][0] + z * R.r[2][0];
        rotated[row * 3 + 1] = x * R.r[0][1] + y * R.r[1][1] + z * R.r[2][1];
        rotated[row * 3 + 2] = x * R.r[0][2] + y * R.r[1][2] + z * R.r[2][2];
    }

    // Positional (6DOF) tracking moves the eye, applied as the final step.
    // Crucially the lean must live in the BODY frame, not the head-rotated
    // frame: if the offset rides the rotated basis, leaning in and then turning
    // the head swings the eye through an arc that adds to the angular motion,
    // so rotation feels amplified. Instead, resolve the view-axis offset into a
    // world vector through the CLEAN (un-head-rotated) basis once, giving a
    // body-fixed eye shift, then bake it into the rotated translation row as
    // -eye_shift . rotatedBasis. Head rotation then simply pivots about the
    // leaned eye - rotation speed is identical whether leaning or not.
    // (At neutral head pose this equals the raw view-space offset, so the lean
    // direction is unchanged; it just stops coupling into rotation.)
    float ox = 0.0f, oy = 0.0f, oz = 0.0f;
    if (GetPlugin().GetCurrentPositionOffset(ox, oy, oz)) {
        const float wx = clean[0] * ox + clean[1] * oy + clean[2] * oz;
        const float wy = clean[3] * ox + clean[4] * oy + clean[5] * oz;
        const float wz = clean[6] * ox + clean[7] * oy + clean[8] * oz;
        g_posEyeShiftWorld[0] = wx;
        g_posEyeShiftWorld[1] = wy;
        g_posEyeShiftWorld[2] = wz;
        for (int j = 0; j < 3; ++j) {
            rotated[3 * 3 + j] += wx * rotated[0 * 3 + j]
                                + wy * rotated[1 * 3 + j]
                                + wz * rotated[2 * 3 + j];
        }
    } else {
        g_posEyeShiftWorld[0] = g_posEyeShiftWorld[1] = g_posEyeShiftWorld[2] = 0.0f;
    }

    // Baseline (Option A): write both render-side matrices identically.
    // g_cameraStruct is left clean - the engine's game logic, MMB raycast,
    // shadow projection, particle systems all read it.
    WriteScaledAndMirror(rotated);

    CopyViewMatrix(g_rotatedMatrix, rotated);

    g_cursorBoxOffsetX.store(0.0f, std::memory_order_release);
    g_cursorBoxOffsetY.store(0.0f, std::memory_order_release);

    // 0xEA9DE0 is the engine's inverse of g_scaledMatrix, used by the shadow
    // projector (FUN_0081FFF0) to bring bones from view-rotated-scaled space
    // back to world space before flattening onto the ground plane. The
    // engine computed it from the CLEAN scaledMatrix earlier in the camera
    // builder (FUN_00819920 / FUN_00819F50, both call FUN_07FB290 right
    // before returning to us). Now that we have overwritten g_scaledMatrix
    // with the rotated version, re-invoke the engine's inverter to refresh
    // 0xEA9DE0 = inv(rotated scaledMatrix). Without this the shadow
    // projector multiplies bones by inv(clean) while characters are
    // rendered through (clean * R), so shadows drift on head rotation.
    typedef void (__fastcall *Fn_Invert_t)(float* out, float* in);
    reinterpret_cast<Fn_Invert_t>(kFn_Invert_Addr)(
        reinterpret_cast<float*>(kShadowMatrixAddr),
        reinterpret_cast<float*>(kScaledMatrixAddr));

    g_haveHook.store(true, std::memory_order_release);
}

typedef void (__fastcall *Fn_19920_t)(float *from, float *to);
typedef void (__fastcall *Fn_19f50_t)(float *from, float *to, float *rotMat);

Fn_19920_t g_orig_19920 = nullptr;
Fn_19f50_t g_orig_19f50 = nullptr;

void CaptureFocalDistance(const float* from, const float* to) {
    if (!from || !to) return;
    const float dx = to[0] - from[0];
    const float dy = to[1] - from[1];
    const float dz = to[2] - from[2];
    g_focalDistance.store(std::sqrt(dx * dx + dy * dy + dz * dz),
                          std::memory_order_release);
}

// FUN_0068B740 is the HUD bottom-right 3D element's mini-camera builder: it
// saves the gameplay camera state then calls FUN_00819920 with fixed positional
// args to set up an isolated camera for the HUD collar / other 3D HUD elements.
// Applying head rotation to THAT build rotates the HUD camera with the player's
// head, so the 3D collar drifts. Detect calls into FUN_00819920 originating from
// inside FUN_0068B740 by return address and skip the head-rotation apply (and
// focal-distance capture, which is for gameplay zoom) for them. The HUD restore
// at the end of FUN_0068B7E0 goes through FUN_0068b7d0 with a different return
// address, so the gameplay camera comes back head-rotated for the next frame.
constexpr uintptr_t kHudCameraBuilderStart = 0x0068B740;
constexpr uintptr_t kHudCameraBuilderEnd   = 0x0068B7D0;

void __fastcall Hook_19920(float *from, float *to) {
    g_orig_19920(from, to);
    const uintptr_t ra = reinterpret_cast<uintptr_t>(_ReturnAddress());
    if (ra >= kHudCameraBuilderStart && ra < kHudCameraBuilderEnd) {
        return;
    }
    CaptureFocalDistance(from, to);
    ApplyHeadRotationToRenderMatrix();
}

void __fastcall Hook_19f50(float *from, float *to, float *rotMat) {
    g_orig_19f50(from, to, rotMat);
    CaptureFocalDistance(from, to);
    ApplyHeadRotationToRenderMatrix();
}

typedef void (__fastcall *Fn_Water_t)(void *self);
Fn_Water_t g_orig_water = nullptr;

// Camera-struct rotated copy stash for the world-render sandwich.
// FUN_005E42E0 is called once per scene render. We swap rotated in on
// entry and clean back on exit. ApplyHead has already written the
// rotated render matrix; we just need to align g_cameraStruct with it
// so CPU-projected primitives (particles, shadows, markers) line up.
typedef void (__fastcall *Fn_WorldRender_t)(int param_1);
Fn_WorldRender_t g_orig_worldRender = nullptr;

void __fastcall Hook_WorldRender(int param_1, void* /*edx*/) {
    if (!g_haveHook.load(std::memory_order_acquire)) {
        g_orig_worldRender(param_1);
        return;
    }
    float* cs = reinterpret_cast<float*>(kCameraStructAddr);
    StashAndReplaceViewMatrix(cs, g_rotatedMatrix, g_cleanCameraSave);
    g_inSandwich.store(true, std::memory_order_release);
    g_orig_worldRender(param_1);
    g_inSandwich.store(false, std::memory_order_release);
    CopyViewMatrix(cs, g_cleanCameraSave);
}

// FUN_0081B370 is the engine's screen-point -> world-ray helper. It
// reads g_cameraStruct's basis to convert (mouseX, mouseY) into a world
// direction. Two prominent callers are the god-hand cursor pickup and
// LH3DWater per-vertex rays. Both want the ray cast through the CLEAN
// camera so the hand sits on the terrain pixel under the cursor and
// the water plane matches its projection. Inside the FUN_0054DA80
// sandwich g_cameraStruct is rotated, so any caller inside this scope
// would otherwise see a head-rotated raycast. Override that here.
typedef void (__fastcall *Fn_S2W_t)(int *param_1, float *param_2, float param_3);
Fn_S2W_t g_orig_s2w = nullptr;

typedef void (__thiscall *Fn_SetCursorPosition_t)(int self, int* xy);
Fn_SetCursorPosition_t g_orig_setCursorPosition = nullptr;

bool ReadVirtualCursor(int& x, int& y) {
    if (!g_virtualCursorActive.load(std::memory_order_acquire)) return false;
    x = g_virtualCursorX.load(std::memory_order_acquire);
    y = g_virtualCursorY.load(std::memory_order_acquire);
    return true;
}

void WriteVirtualCursorToInputObject(int self, int x, int y) {
    *reinterpret_cast<int*>(self + 0xbc) = x;
    *reinterpret_cast<int*>(self + 0xc0) = y;
    *reinterpret_cast<int*>(self + 0xc4) = x;
    *reinterpret_cast<int*>(self + 0xc8) = y;
    *reinterpret_cast<int*>(self + 0xd0) = x;
    *reinterpret_cast<int*>(self + 0xd4) = y;
    *reinterpret_cast<int*>(kCursorXAddr) = x;
    *reinterpret_cast<int*>(kCursorYAddr) = y;
}

void __fastcall Hook_SetCursorPosition(int self, void* /*edx*/, int* xy) {
    g_inputObject.store(static_cast<uintptr_t>(self), std::memory_order_release);
    int vx = 0;
    int vy = 0;
    if (GetPlugin().IsEnabled() && ReadVirtualCursor(vx, vy)) {
        WriteVirtualCursorToInputObject(self, vx, vy);
        if (*reinterpret_cast<unsigned char*>(kCursorWarpGateAddr) == 0) return;
        int virtualXy[2] = { vx, vy };
        g_orig_setCursorPosition(self, virtualXy);
        WriteVirtualCursorToInputObject(self, vx, vy);
        return;
    }
    g_orig_setCursorPosition(self, xy);
}

typedef void (__thiscall *Fn_InputCursorClamp_t)(int self, int x, int y);
Fn_InputCursorClamp_t g_orig_inputCursorClamp = nullptr;

void __fastcall Hook_InputCursorClamp(int self, void* /*edx*/, int x, int y) {
    g_inputObject.store(static_cast<uintptr_t>(self), std::memory_order_release);
    g_orig_inputCursorClamp(self, x, y);
    int vx = 0;
    int vy = 0;
    if (!GetPlugin().IsEnabled() || !ReadVirtualCursor(vx, vy)) return;
    WriteVirtualCursorToInputObject(self, vx, vy);
}

typedef void (__cdecl *Fn_ObjectScreenPick_t)(int param_1, float *param_2, float param_3);
Fn_ObjectScreenPick_t g_orig_objectScreenPick = nullptr;

// Return addresses of the two FUN_0081B370 calls inside the name-box setup
// FUN_008334C0. The box's screen anchor is computed against the head-rotated
// projection, but unprojected here; forcing the clean camera (as we do for MMB
// grab/water) makes the screen->world->screen round trip mismatch, so the box
// swims as the head turns. Unproject these through the rotated camera instead
// so the round trip closes and the box stays pinned where intended.
constexpr uintptr_t kNameBoxS2W_A = 0x0083372E;
constexpr uintptr_t kNameBoxS2W_B = 0x0083390B;

// Call the original screen->world ray with the pixel input shifted by the
// head-turn offset IFF the input matches the engine cursor globals (= a
// cursor-driven pick). The engine cursor holds the VISIBLE position; the
// original FUN_0081B370 expects the CLEAN (extended) coord, so we add the
// offset back for the duration of the call and restore on return. Non-cursor
// callers (e.g. the name-box unproject) leave the input alone.
void CallOrigS2WShifted(int *param_1, float *param_2, float param_3) {
    const int curX = *reinterpret_cast<int*>(kCursorXAddr);
    const int curY = *reinterpret_cast<int*>(kCursorYAddr);
    const int savedX = param_1[0];
    const int savedY = param_1[1];
    if (savedX == curX && savedY == curY) {
        param_1[0] = savedX + static_cast<int>(std::lround(
            g_cursorBoxOffsetX.load(std::memory_order_acquire)));
        param_1[1] = savedY + static_cast<int>(std::lround(
            g_cursorBoxOffsetY.load(std::memory_order_acquire)));
    }
    g_orig_s2w(param_1, param_2, param_3);
    param_1[0] = savedX;
    param_1[1] = savedY;
}

void __fastcall Hook_ScreenToWorld(int *param_1, float *param_2, float param_3) {
    const uintptr_t ra = reinterpret_cast<uintptr_t>(_ReturnAddress());
    if ((ra == kNameBoxS2W_A || ra == kNameBoxS2W_B)
        && g_haveHook.load(std::memory_order_acquire)) {
        float* cs = reinterpret_cast<float*>(kCameraStructAddr);
        float* pivot = reinterpret_cast<float*>(kCameraPivotAddr);
        float saveCs[kViewMatrixFloats];
        float savePivot[3] = { pivot[0], pivot[1], pivot[2] };
        StashAndReplaceViewMatrix(cs, g_rotatedMatrix, saveCs);

        // FUN_0081B370 reconstructs the world point as eye + basis*viewray
        // (g_cameraPivot + g_cameraStruct rows). It never reads the matrix
        // translation row, so the positional eye shift baked into the render is
        // invisible here: the box would anchor to the un-shifted eye while the
        // unit, rendered through the shifted view, has moved -> the box drifts
        // off the name by the lean amount. The render moved the eye to
        // eye - g_posEyeShiftWorld (body frame), so shift g_cameraPivot by the
        // same vector and the unproject lands back on the unit.
        pivot[0] -= g_posEyeShiftWorld[0];
        pivot[1] -= g_posEyeShiftWorld[1];
        pivot[2] -= g_posEyeShiftWorld[2];

        CallOrigS2WShifted(param_1, param_2, param_3);
        pivot[0] = savePivot[0]; pivot[1] = savePivot[1]; pivot[2] = savePivot[2];
        CopyViewMatrix(cs, saveCs);
        return;
    }
    if (!g_inSandwich.load(std::memory_order_acquire)) {
        CallOrigS2WShifted(param_1, param_2, param_3);
        return;
    }
    float* cs = reinterpret_cast<float*>(kCameraStructAddr);
    float saveCs[kViewMatrixFloats];
    StashAndReplaceViewMatrix(cs, g_cleanCameraSave, saveCs);
    CallOrigS2WShifted(param_1, param_2, param_3);
    CopyViewMatrix(cs, saveCs);
}

constexpr uintptr_t kCollarObjectPickReturn = 0x00466B90;
constexpr uintptr_t kCitadelScrollPickReturn = 0x0070A2DC;

static bool IsCitadelChamberPick(uintptr_t ra) {
    return ra == kCollarObjectPickReturn || ra == kCitadelScrollPickReturn;
}

// FUN_00519960 (object screen pick) compares a projected world point against
// DAT_00ea1ac8/cc (the cursor snapshot). For most pick paths the global
// Hook_PickOrchestrator shift is sufficient - the snapshot inherits the shift
// and the rotated projection matches.
//
// The citadel chamber picks (collars at ra=0x00466B90, chamber mission scrolls
// at ra=0x0070A2DC) need a different treatment: they project through a path
// that does NOT track the head-rotated render (likely an inner-chamber camera
// sub-view used to render the citadel's orbital view of itself). For those
// callers, restore the clean projection matrices AND undo the snap shift for
// the duration of FUN_00519960 so cursor and projection both live in the
// clean-camera frame the chamber rendering uses.
void __cdecl Hook_ObjectScreenPick(int param_1, float *param_2, float param_3) {
    const uintptr_t ra = reinterpret_cast<uintptr_t>(_ReturnAddress());
    if (!IsCitadelChamberPick(ra) || !g_inSandwich.load(std::memory_order_acquire)) {
        g_orig_objectScreenPick(param_1, param_2, param_3);
        return;
    }

    // Force the clean projection matrices and undo the cursor-shift on the
    // snap, so both cursor and projection live in the clean-camera frame the
    // chamber rendering uses. The shift values to subtract are exactly what
    // Hook_PickOrchestrator added to kCursorX/Y, which then flowed into the
    // snap.
    float* cs = reinterpret_cast<float*>(kCameraStructAddr);
    float* scaled = reinterpret_cast<float*>(kScaledMatrixAddr);
    float* mirror = reinterpret_cast<float*>(kMirrorMatrixAddr);
    int* snap_x_p = reinterpret_cast<int*>(0x00EA1AC8);
    int* snap_y_p = reinterpret_cast<int*>(0x00EA1ACC);
    float saveCs[kViewMatrixFloats];
    float saveScaled[kViewMatrixFloats];
    float saveMirror[kViewMatrixFloats];
    const int saved_snap_x = *snap_x_p;
    const int saved_snap_y = *snap_y_p;
    const int dx = static_cast<int>(std::lround(
        g_pickShiftX.load(std::memory_order_acquire)));
    const int dy = static_cast<int>(std::lround(
        g_pickShiftY.load(std::memory_order_acquire)));

    CopyViewMatrix(saveScaled, scaled);
    CopyViewMatrix(saveMirror, mirror);
    StashAndReplaceViewMatrix(cs, g_cleanCameraSave, saveCs);
    WriteScaledAndMirror(g_cleanCameraSave);
    *snap_x_p = saved_snap_x - dx;
    *snap_y_p = saved_snap_y - dy;

    g_orig_objectScreenPick(param_1, param_2, param_3);

    *snap_x_p = saved_snap_x;
    *snap_y_p = saved_snap_y;
    CopyViewMatrix(cs, saveCs);
    CopyViewMatrix(scaled, saveScaled);
    CopyViewMatrix(mirror, saveMirror);
}

void __fastcall Hook_Water(void *self, void* /*edx*/) {
    // Engine's 2D water render is suppressed (it's a CPU-projected
    // pre-transformed sheet that can't survive head rotation). When
    // tracking is enabled, draw our own flat 3D water quad here so the
    // sea is in the right Z-order: after terrain, before HUD.
    // Tracking off -> let the engine's water render normally.
    if (!GetPlugin().IsEnabled()) {
        g_orig_water(self);
        return;
    }
    // Always render water when tracking is enabled - even when the head
    // rotation is identity, the camera itself may have moved this frame
    // and the water must sit under the current camera position.
    RenderHeadTrackedWater();
}

typedef void (__thiscall *Fn_CitadelGlow_t)(int self, char param);
Fn_CitadelGlow_t g_orig_citadelGlow = nullptr;

// FUN_007954A0 is the citadel/temple-room glow + lightmap draw (Challenge,
// Creature, Credits, GameOptions, SaveGame, World, Universe rooms).
//
// TODO(glows): the chandelier/light glows are screen-locked under head tracking
// and not yet fixed. They are oriented-corona billboards (FUN_0083c990) whose
// WORLD positions transform through the glow renderer's matrix at
// (*(this+0x18))+0x14, which the engine leaves un-head-rotated, so they do not
// follow the room (which renders through the head-rotated g_scaledMatrix).
// Substituting a head-rotated world->view matrix there does rotate them, but the
// glow frame is Z-flipped (yaw+pitch read inverted) AND the corona's own facing
// compounds roll, so a plain matrix swap could not be made to track on all axes
// across several attempts. A correct fix likely needs per-corona screen
// reprojection (project each world glow position through the head-rotated view,
// like reticle compensation) rather than a single matrix. Until then, suppress
// the glows under tracking via Hook_Corona below; the lightmap/volume-light
// sub-path (FUN_0083d4b0 -> g_scaledMatrix) tracks fine and is left drawing.
void __fastcall Hook_CitadelGlow(int self, void* /*edx*/, char param) {
    g_inCitadelGlow.store(true, std::memory_order_release);
    g_orig_citadelGlow(self, param);
    g_inCitadelGlow.store(false, std::memory_order_release);
}

// FUN_0083c990 draws an oriented-corona billboard. Inside the citadel glow gate,
// with tracking active, skip it so the screen-locked glows are not drawn. Outside
// the gate it draws normally, so other coronas are untouched.
typedef void (__thiscall *Fn_Corona_t)(int self);
Fn_Corona_t g_orig_corona = nullptr;

void __fastcall Hook_Corona(int self, void* /*edx*/) {
    if (GetPlugin().IsEnabled()
        && g_haveHook.load(std::memory_order_acquire)
        && g_inCitadelGlow.load(std::memory_order_acquire)) {
        return;
    }
    g_orig_corona(self);
}

// FUN_00576F20: cursor-edge effect / rotation-strength gate processor.
//   - Reads cursor X/Y from stack params, checks a 20-pixel "safe-zone" margin
//     against screen dimensions (DAT_00e85058).
//   - Gated by the rotation-strength field *(float*)(self+0x98) > 0; when that
//     field is positive it does animation/sprite work for the cursor including
//     - per evidence in the wild - the LMB-drag rotation-handle indication.
//   - Three callers pass different LAB_xxx sprite tables: FUN_005c83d0 (camera
//     update wrapper), FUN_0071f3d0 and FUN_00517d93 (cursor-sprite handlers).
// Forcing self+0x98 to zero at entry takes the early-return path, so the entire
// rotation-strength-driven body never runs and the engine can't promote the
// cursor to a rotation handle or trigger LMB rotation. The final fade-to-zero
// branch keeps the field at zero on subsequent calls.
typedef void (__thiscall *Fn_CursorEffect_t)(int self, int p1, int p2);
Fn_CursorEffect_t g_orig_cursorEffect = nullptr;

void __fastcall Hook_CursorEffect(int self, void* /*edx*/, int p1, int p2) {
    if (g_virtualCursorActive.load(std::memory_order_acquire)) {
        *reinterpret_cast<float*>(self + 0x98) = 0.0f;
    }
    g_orig_cursorEffect(self, p1, p2);
}

// FUN_005E42E0 is the per-frame "what is the cursor over" orchestrator:
//   DAT_00ea1acc = kCursorY;
//   DAT_00ea1ac8 = kCursorX;                  <- pick logic reads these two
//   FUN_0081B370(0);                          <- screen-to-world ray
//   DAT_00ea1b14 = kCursorY;
//   DAT_00ea1b10 = kCursorX;
// All downstream picks (citadel collar, creature mouseover, mission scroll
// mouseover) compare DAT_00ea1ac8/cc against projected object world positions.
// Inside the world-render sandwich the projection uses the head-rotated matrix,
// but kCursorX/Y is the raw input pixel - which sits at clean_proj(target) only,
// not rotated_proj(target). Shift the cursor by the rotated-forward NDC delta
// for the duration of this function so the snapshots inherit the shift and the
// pick alignment is restored. Restore on exit so other readers (cursor sprite,
// HUD, the cage) see the raw cursor again.
//
// Calling convention: FUN_005E42E0 is __thiscall. The caller does
// LEA ECX,[EBP+0x205a20] before the CALL, and the prologue immediately stores
// ECX to [ESP+0x18] for later use. The hook MUST preserve ECX through to the
// original or `this` becomes garbage and the function crashes the first time
// it dereferences it.
typedef int (__thiscall *Fn_PickOrchestrator_t)(int self);
Fn_PickOrchestrator_t g_orig_pickOrchestrator = nullptr;

int __fastcall Hook_PickOrchestrator(int self, void* /*edx*/) {
    if (!g_haveHook.load(std::memory_order_acquire)) {
        return g_orig_pickOrchestrator(self);
    }
    const int saved_x = *reinterpret_cast<int*>(kCursorXAddr);
    const int saved_y = *reinterpret_cast<int*>(kCursorYAddr);
    const int dx = static_cast<int>(std::lround(
        g_pickShiftX.load(std::memory_order_acquire)));
    const int dy = static_cast<int>(std::lround(
        g_pickShiftY.load(std::memory_order_acquire)));
    *reinterpret_cast<int*>(kCursorXAddr) = saved_x + dx;
    *reinterpret_cast<int*>(kCursorYAddr) = saved_y + dy;
    const int rv = g_orig_pickOrchestrator(self);
    *reinterpret_cast<int*>(kCursorXAddr) = saved_x;
    *reinterpret_cast<int*>(kCursorYAddr) = saved_y;
    return rv;
}

bool InstallOneHook(uintptr_t addr, void* detour, void** outOriginal, const char* tag) {
    using namespace cameraunlock::hooks;
    auto& mgr = HookManager::Instance();
    void* target = reinterpret_cast<void*>(addr);
    if (mgr.CreateHook(target, detour, outOriginal) != HookStatus::Ok) {
        HT_LOG("[hook] CreateHook(%s @ 0x%08X) failed", tag, static_cast<unsigned>(addr));
        return false;
    }
    if (mgr.EnableHook(target) != HookStatus::Ok) {
        HT_LOG("[hook] EnableHook(%s @ 0x%08X) failed", tag, static_cast<unsigned>(addr));
        return false;
    }
    HT_LOG("[hook] %s @ 0x%08X hooked", tag, static_cast<unsigned>(addr));
    return true;
}

}  // namespace

float GetFocalDistance() { return g_focalDistance.load(std::memory_order_acquire); }
void SetVirtualCursor(bool active, int x, int y) {
    g_virtualCursorX.store(x, std::memory_order_release);
    g_virtualCursorY.store(y, std::memory_order_release);
    g_virtualCursorActive.store(active, std::memory_order_release);
    if (active) {
        *reinterpret_cast<int*>(kCursorXAddr) = x;
        *reinterpret_cast<int*>(kCursorYAddr) = y;
        // Also push into the engine's input object mirrors if we've captured
        // its `this` pointer from a prior input-hook fire, so engine paths that
        // read those instead of the global keep our unclamped value.
        const uintptr_t io = g_inputObject.load(std::memory_order_acquire);
        if (io) {
            *reinterpret_cast<int*>(io + 0xbc) = x;
            *reinterpret_cast<int*>(io + 0xc0) = y;
            *reinterpret_cast<int*>(io + 0xc4) = x;
            *reinterpret_cast<int*>(io + 0xc8) = y;
            *reinterpret_cast<int*>(io + 0xd0) = x;
            *reinterpret_cast<int*>(io + 0xd4) = y;
        }
        // Kill the engine's cursor-delta + accumulator globals so the
        // pan-to-edge / forced-rotation paths in FUN_005F89F0 and friends never
        // see non-zero motion. Without these zeroed, FUN_005F89F0 accumulates
        // ±20-clamped deltas into kCursorAccumX/Y and uses them as a rotation
        // strength even when the user's mouse motion is being captured by our
        // virtual cursor and shouldn't drive in-game camera rotation.
        *reinterpret_cast<int*>(kCursorDeltaXAddr) = 0;
        *reinterpret_cast<int*>(kCursorDeltaYAddr) = 0;
        *reinterpret_cast<int*>(kCursorDeltaZAddr) = 0;
        *reinterpret_cast<int*>(kCursorAccumXAddr) = 0;
        *reinterpret_cast<int*>(kCursorAccumYAddr) = 0;
    }
}

void GetCursorBoxOffset(int& offsetX, int& offsetY) {
    offsetX = static_cast<int>(std::lround(g_cursorBoxOffsetX.load(std::memory_order_acquire)));
    offsetY = static_cast<int>(std::lround(g_cursorBoxOffsetY.load(std::memory_order_acquire)));
}

CameraHook::~CameraHook() { Uninstall(); }

bool CameraHook::Install() {
    using namespace cameraunlock::hooks;
    auto& mgr = HookManager::Instance();
    if (auto s = mgr.Initialize();
        s != HookStatus::Ok && s != HookStatus::ErrorAlreadyInitialized) {
        HT_LOG("[hook] MinHook init failed: %s", HookStatusToString(s));
        return false;
    }

    if (!GetModuleHandleA("runblack.exe")) {
        HT_LOG("[hook] runblack.exe not loaded - aborting install");
        return false;
    }

    bool any = false;
    any |= InstallOneHook(kFn_19920_Addr, reinterpret_cast<void*>(&Hook_19920),
                          reinterpret_cast<void**>(&g_orig_19920), "FUN_00819920");
    any |= InstallOneHook(kFn_19f50_Addr, reinterpret_cast<void*>(&Hook_19f50),
                          reinterpret_cast<void**>(&g_orig_19f50), "FUN_00819f50");
    InstallOneHook(kFn_WorldRender_Addr, reinterpret_cast<void*>(&Hook_WorldRender),
                   reinterpret_cast<void**>(&g_orig_worldRender), "FUN_0054DA80 frameRender");
    InstallOneHook(kFn_Water_Addr, reinterpret_cast<void*>(&Hook_Water),
                   reinterpret_cast<void**>(&g_orig_water), "LH3DWater::Render");
    InstallOneHook(kFn_ScreenToWorld_Addr, reinterpret_cast<void*>(&Hook_ScreenToWorld),
                   reinterpret_cast<void**>(&g_orig_s2w), "FUN_0081B370 screenToWorld");
    InstallOneHook(kFn_ObjectScreenPick_Addr, reinterpret_cast<void*>(&Hook_ObjectScreenPick),
                   reinterpret_cast<void**>(&g_orig_objectScreenPick), "FUN_00519960 objectScreenPick");
    InstallOneHook(kFn_InputCursorClamp_Addr, reinterpret_cast<void*>(&Hook_InputCursorClamp),
                   reinterpret_cast<void**>(&g_orig_inputCursorClamp), "FUN_007E49A0 inputCursorClamp");
    InstallOneHook(kFn_SetCursorPosition_Addr, reinterpret_cast<void*>(&Hook_SetCursorPosition),
                   reinterpret_cast<void**>(&g_orig_setCursorPosition), "FUN_007E4E40 setCursorPosition");
    InstallOneHook(kFn_CitadelGlow_Addr, reinterpret_cast<void*>(&Hook_CitadelGlow),
                   reinterpret_cast<void**>(&g_orig_citadelGlow), "FUN_007954A0 citadelGlow");
    InstallOneHook(kFn_CursorEffect_Addr, reinterpret_cast<void*>(&Hook_CursorEffect),
                   reinterpret_cast<void**>(&g_orig_cursorEffect), "FUN_00576F20 cursorEffect");
    InstallOneHook(0x0083C990, reinterpret_cast<void*>(&Hook_Corona),
                   reinterpret_cast<void**>(&g_orig_corona), "FUN_0083C990 corona");
    InstallOneHook(kFn_PickOrchestrator_Addr, reinterpret_cast<void*>(&Hook_PickOrchestrator),
                   reinterpret_cast<void**>(&g_orig_pickOrchestrator), "FUN_005E42E0 pickOrchestrator");
    if (!any) {
        HT_LOG("[hook] no engine functions hooked - aborting");
        return false;
    }
    return true;
}

void CameraHook::Uninstall() {
    cameraunlock::hooks::HookManager::Instance().DisableAllHooks();
}

}  // namespace headtracking
