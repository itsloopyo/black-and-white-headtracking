#pragma once

#include <cstdint>

#include <Windows.h>
#include <d3d.h>

// Black & White (2001) engine memory map, as revealed by Ghidra.
//
// runblack.exe loads at base 0x00400000 with ASLR off, so these loaded
// virtual addresses are stable and can be dereferenced directly.
//
// The camera builders (FUN_00819920 / FUN_00819f50) populate g_cameraStruct
// from a (from, to) pair, projection-scale it into g_scaledMatrix, and byte-
// copy that into g_mirrorMatrix:
//   scaled[r,0] = clean[r,0] * sx
//   scaled[r,1] = clean[r,1] * sy
//   scaled[r,2] = clean[r,2]
//
// g_cameraStruct is read by everything that needs the camera: mouse-to-world
// raycasts (MMB grab), projectile spawn directions, AI vision, picking,
// frustum culling, etc. (~358 readers). g_scaledMatrix / g_mirrorMatrix is
// what the CPU vertex transform pipeline uses for actual rendering.

namespace headtracking {

// The engine's view matrices are 4x3 (3 basis rows + 1 translation row),
// stored as 12 contiguous floats. Every save/restore/swap of g_cameraStruct,
// g_scaledMatrix and g_mirrorMatrix copies this many elements.
constexpr int kViewMatrixFloats = 12;

constexpr uintptr_t kCameraStructAddr = 0x00EA1D28;  // float[12], 4x3 clean view matrix
constexpr uintptr_t kCameraPivotAddr  = 0x00EA1DB8;  // float[3], eye in world (B&W is Y-up)
constexpr uintptr_t kScaledMatrixAddr = 0x00EA9E40;  // float[12], projection-scaled mirror
constexpr uintptr_t kMirrorMatrixAddr = 0x00EA1D58;  // float[12], byte copy of scaled
constexpr uintptr_t kScaleXAddr       = 0x00E83A00;  // float, X projection scale ~ tan(FOV/2)
constexpr uintptr_t kScaleYAddr       = 0x00E83A04;  // float, Y projection scale
constexpr uintptr_t kShadowMatrixAddr = 0x00EA9DE0;  // inverse of g_scaledMatrix (shadow projector)
constexpr uintptr_t kCursorXAddr      = 0x00E852C0;  // current game cursor X, client pixels
constexpr uintptr_t kCursorYAddr      = 0x00E852C4;  // current game cursor Y, client pixels

// Engine gate (byte) read before the OS cursor warp in SetCursorPosition: when
// zero the engine suppresses the warp, so the hook must skip it too.
constexpr uintptr_t kCursorWarpGateAddr = 0x00E8C0FA;

// Cursor-delta globals consumed by the engine's gameplay-input/rotation paths.
// FUN_005F89F0 reads kCursorX/Y, clamps the accumulators (kCursorAccumX/Y) to
// ±20, and feeds them into rotation/camera math (also drives sprite trails,
// pan-to-edge behaviour, and probably the right-click-drag rotation engine
// users hit when their cursor reaches the screen edges). Clearing all four
// every cage tick keeps the engine from accumulating enough delta to drive
// any of those behaviours, so the cursor can travel freely past the visible
// edges without forcing camera rotation.
constexpr uintptr_t kCursorDeltaXAddr   = 0x00E852E4;
constexpr uintptr_t kCursorDeltaYAddr   = 0x00E852E8;
constexpr uintptr_t kCursorDeltaZAddr   = 0x00E85300;   // wheel/aux delta
constexpr uintptr_t kCursorAccumXAddr   = 0x00D37CB0;   // ±20-clamped cumulative
constexpr uintptr_t kCursorAccumYAddr   = 0x00D37CB4;

// FUN_0081B370 pixel<->ray constants. For pixel p the engine forms a view-space
// ray ((p.x-halfX)*scaleX/halfX, (halfY-p.y)*scaleY/halfY, fwd) then rotates it
// by g_cameraStruct. Inverting that lets us project the head-rotated forward axis
// back into clean-camera cursor pixels - the offset the cursor cage uses to keep
// the hand inside the visible (rotated) play area while picks still raycast
// through the clean camera.
constexpr uintptr_t kViewForwardAddr  = 0x00E839E0;  // float, view-space forward depth
constexpr uintptr_t kScreenHalfXAddr  = 0x00E839F0;  // float, screen centre X (half width), px
constexpr uintptr_t kScreenHalfYAddr  = 0x00E839F4;  // float, screen centre Y (half height), px
constexpr uintptr_t kProjScaleXAddr   = 0x00C3812C;  // float, horizontal ray scale (~tan(fovH/2))
constexpr uintptr_t kProjScaleYAddr   = 0x00C38130;  // float, vertical ray scale (~tan(fovV/2))

// void __fastcall(float* out, float* in): out = inverse(in).
constexpr uintptr_t kFn_Invert_Addr   = 0x007FB290;

// Global IDirect3DDevice7* used by the engine. Found at the tail of
// FUN_00819F50 (cutscene camera builder) which loads it and calls
// SetTransform(D3DTS_VIEW). 465 xrefs across the binary - this is THE device,
// used by both camera builders and LH3DWater::Render among ~80 others.
constexpr uintptr_t kD3DDeviceGlobalAddr = 0x00ECA638;

inline IDirect3DDevice7* GetD3DDevice() {
    return *reinterpret_cast<IDirect3DDevice7**>(kD3DDeviceGlobalAddr);
}

}  // namespace headtracking
