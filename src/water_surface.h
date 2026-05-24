#pragma once

namespace headtracking {

// Draws the head-tracking-compatible procedural ocean.
//
// B&W's native sea is a CPU-projected pre-transformed (D3DFVF_XYZRHW) sheet
// that cannot survive a head-rotated view matrix, so the camera hook
// suppresses it and calls this instead. The surface is a camera-centred,
// wave-displaced grid emitted as XYZRHW triangles projected through the
// engine's (already head-rotated) g_scaledMatrix, so it tracks the view for
// free. Save/restore of the D3D render state it touches is handled internally.
void RenderHeadTrackedWater();

}  // namespace headtracking
