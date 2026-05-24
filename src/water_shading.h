#pragma once

// Pure (D3D-free) wave field and per-vertex shading math for the procedural
// ocean. Split out of water_surface.cpp so it can be golden-locked by a unit
// test without dragging in DirectDraw / the engine memory map. The render
// plumbing (vertex emit, clipping, device state) stays in water_surface.cpp.

#include <cmath>
#include <cstdint>

namespace headtracking {
namespace water {

// B&W is Y-up. Confirmed empirically from the camera matrix: forward axis
// is ~-Y when looking down at the island, eye sits at Y ~3054 over the
// terrain. Sea level is Y = 0 in the engine, but we lift the visible
// surface slightly so it consistently wins z-test against the ugly
// aliased seabed mesh that B&W normally hides under its own water.
constexpr float kSeaLevelY = 1.0f;

// Half-extent of the camera-centered water grid. Big enough to reach past
// any reasonable horizon at gameplay pitches.
constexpr float kWaterHalfExtent = 8000.0f;

// Grid tessellation. Bumped from the original 16 to give per-vertex wave
// displacement and per-vertex lighting enough spatial sampling that
// medium-wavelength waves (200-800 world units) read as smooth ocean
// swell rather than a blocky polygon hash.
constexpr int kWaterGridCells = 32;

// Sum-of-sines wave field, evaluated in GRID-INDEX space rather than
// world-space. Each wave contributes A * sin(k * (dir . grid_pos) -
// omega * t + phi), where grid_pos is the (col, row) index of the
// vertex on the camera-centred tessellation grid.
//
// Why grid-index instead of world position: with world-locked waves,
// the wave value at any screen position changes both with time AND
// with camera motion (because dragging the camera shifts which world
// point each screen pixel maps to). The user perceives the resulting
// rate change as the wave animation "speeding up when I drag".
// Locking the wave field to the grid makes the animation purely
// time-driven - dragging the camera doesn't shift any vertex's
// grid index, so the wave at each on-screen position evolves at the
// same rate regardless of motion. For B&W's god-camera (player isn't
// physically inside the world) this reads as ambient water that's
// always doing its thing; you can't anchor a "fixed wave" to a world
// point you've never visited anyway.
struct WaveSpec {
    float dir_x, dir_z;     // unit-length direction in grid space
    float cells_per_wave;   // wavelength expressed in grid cells
    float amplitude;        // world-space Y displacement
    float cells_per_second; // phase travel speed in grid cells / sec
    float phase;            // static phase offset (radians)
};
// Tuned for gentle ocean swell: low Y displacement (~2.8u peak) so wave
// crests don't poke up through low-lying coastline terrain, with a mix of
// frequencies so the surface normals stay busy. Lambda in "cells" with a
// 32-cell grid means 18-cell wavelength = big swell, 2-3 cell = wind ripple.
// The height budget is weighted toward the short wavelengths because slope
// (what the lighting sees) is amplitude*k, so short waves buy visible chop
// for almost no height.
constexpr WaveSpec kWaves[] = {
    //  dir_x   dir_z   lambda  amp     speed  phase
    {  1.000f,  0.000f, 20.0f,  1.00f,  1.4f,  0.0f },
    {  0.000f,  1.000f, 14.0f,  0.70f,  1.1f,  1.7f },
    {  0.707f,  0.707f,  8.0f,  0.45f,  1.7f,  3.2f },
    { -0.707f,  0.707f,  5.0f,  0.25f,  2.1f,  4.5f },
    {  0.500f, -0.866f,  3.0f,  0.15f,  2.6f,  0.9f },
    { -0.866f,  0.500f,  2.0f,  0.10f,  3.0f,  2.3f },
    // Sub-2-cell-wavelength waves: each vertex hits a different phase
    // of these, so adjacent cells get visibly different normals and
    // the per-vertex shading reads as surface "texture" rather than a
    // smooth interpolated gradient. Amplitudes are tiny - only the
    // slope (and therefore the normal) matters for the textured look.
    {  0.382f,  0.924f,  1.7f,  0.08f,  3.4f,  1.1f },
    { -0.924f, -0.382f,  1.3f,  0.05f,  4.0f,  2.7f },
    {  0.643f, -0.766f,  1.05f, 0.03f,  4.5f,  3.9f },
};
constexpr int kNumWaves = sizeof(kWaves) / sizeof(kWaves[0]);

// World-space sun direction (vert -> sun), normalised. Roughly overhead
// with a slight tilt so Blinn-Phong sun glints show up across most
// camera angles. Could later be sourced from B&W's own lighting state.
constexpr float kSunX = 0.357f;
constexpr float kSunY = 0.857f;
constexpr float kSunZ = 0.371f;

// Deep water / sky-reflection colours, in linear-ish 0..1 RGB. Fresnel
// (Schlick) blends between them per-vertex based on the angle between
// the view direction and the surface normal: looking straight down at
// the water samples deep, looking near-grazing samples sky.
// Both pulled down from full bright so the water doesn't look like
// noon daylight in scenes the engine has shaded for dusk / night.
constexpr float kDeepR = 0.04f, kDeepG = 0.13f, kDeepB = 0.22f;
constexpr float kSkyR  = 0.30f, kSkyG  = 0.46f, kSkyB  = 0.58f;
// F0 bumped well above the physically-correct 0.02 so even near-vertical
// view directions get a meaningful chunk of sky reflection mixed into
// the diffuse colour. Without this, water directly below the camera is
// almost pure deep-colour and reads as a flat mirror once any other
// surface detail breaks up.
constexpr float kFresnelF0 = 0.10f;
constexpr float kSpecStrength = 1.2f;
// Per-vertex normal-perturbation amount. Each grid vert gets a small
// hash-driven kick added to (nx, nz) before normalisation. Kept small:
// because the kick is incoherent between adjacent verts, large values made
// the lighting differ wildly across a triangle, and at range (where one
// triangle covers a big chunk of screen) that interpolated into giant
// blue/white diamonds. A gentle kick still breaks up the smooth gradient
// up close without blowing out at distance.
constexpr float kNormalJitter = 0.05f;
// Whitecap foam threshold: when the geometric wave height exceeds this
// (in world units, where total wave amplitude is ~2.8u), we tint the
// vertex toward white. Only the tallest crests foam, so it stays sparse.
constexpr float kFoamThreshold = 1.4f;
constexpr float kFoamStrength  = 0.35f;
// Per-vertex chromatic noise. Small high-frequency variation in
// brightness so neighbouring verts don't interpolate as a perfect
// smooth gradient - reads as faint surface stipple up close.
constexpr float kNoiseStrength = 0.03f;

constexpr float kTwoPi = 6.2831853f;

// Per-frame precompute of each wave's grid-space constants. Everything
// in here is invariant across the whole vertex grid for a given time t,
// so it is computed once per frame in RenderHeadTrackedWater rather than
// re-derived for all (N+1)^2 vertices inside SampleWaves.
struct WaveFrame {
    float kdx, kdz;       // k*dir_x, k*dir_z  (phase gradient in grid space)
    float amp;            // amplitude
    float amp_kdx, amp_kdz; // amplitude*k*dir_x, amplitude*k*dir_z (deriv weights)
    float phase_const;    // -omega*t + static phase
};

inline void PrecomputeWaves(float t, WaveFrame (&out)[kNumWaves]) {
    for (int i = 0; i < kNumWaves; ++i) {
        const WaveSpec& w = kWaves[i];
        const float k = kTwoPi / w.cells_per_wave;
        const float omega = k * w.cells_per_second;
        out[i].kdx = k * w.dir_x;
        out[i].kdz = k * w.dir_z;
        out[i].amp = w.amplitude;
        out[i].amp_kdx = w.amplitude * k * w.dir_x;
        out[i].amp_kdz = w.amplitude * k * w.dir_z;
        out[i].phase_const = -omega * t + w.phase;
    }
}

// Sample the wave field at grid index (fcol, frow) using the per-frame
// precomputed wave constants. Returns the Y displacement plus the partial
// derivatives with respect to the grid axes (these get scaled into
// world-space derivatives by the caller, dividing by the world-units-per-
// cell step, so the surface normal ends up properly world-aligned for
// lighting).
inline void SampleWaves(const WaveFrame (&wf)[kNumWaves],
                        float fcol, float frow,
                        float& out_dy,
                        float& out_dy_dcol, float& out_dy_drow) {
    float dy = 0.0f, dc = 0.0f, dr = 0.0f;
    for (int i = 0; i < kNumWaves; ++i) {
        const WaveFrame& w = wf[i];
        const float phase = w.kdx * fcol + w.kdz * frow + w.phase_const;
        const float s = std::sin(phase);
        const float c = std::cos(phase);
        dy += w.amp * s;
        dc += w.amp_kdx * c;
        dr += w.amp_kdz * c;
    }
    out_dy = dy; out_dy_dcol = dc; out_dy_drow = dr;
}

// Per-vertex shader. Mixes deep / sky via Schlick Fresnel, adds a
// Blinn-Phong sun specular, a foam crest at high wave amplitude, and a
// small per-vertex chromatic stipple so adjacent verts don't interpolate
// across the triangle as a perfectly smooth gradient.
//   nx/ny/nz: world-space surface normal
//   dx/dy/dz: unnormalised world-space (eye - vert)
//   viewLen:  |eye - vert|
//   wave_dy:  geometric wave Y displacement at this vert (for foam)
//   fcol/frow: grid index of this vert (for stipple noise)
//   t:         current wave time
inline uint32_t ShadeWater(float nx, float ny, float nz,
                           float dx, float dy, float dz, float viewLen,
                           float wave_dy, float fcol, float frow, float t) {
    const float invVL = 1.0f / viewLen;
    const float vx = dx * invVL;
    const float vy = dy * invVL;
    const float vz = dz * invVL;

    float ndotv = nx*vx + ny*vy + nz*vz;
    if (ndotv < 0.0f) ndotv = 0.0f;
    if (ndotv > 1.0f) ndotv = 1.0f;

    const float oneMinus = 1.0f - ndotv;
    const float om2 = oneMinus * oneMinus;
    const float om5 = om2 * om2 * oneMinus;
    const float fresnel = kFresnelF0 + (1.0f - kFresnelF0) * om5;

    float r = kDeepR + (kSkyR - kDeepR) * fresnel;
    float g = kDeepG + (kSkyG - kDeepG) * fresnel;
    float b = kDeepB + (kSkyB - kDeepB) * fresnel;

    // Blinn-Phong half-vector specular against the sun.
    float hx = vx + kSunX, hy = vy + kSunY, hz = vz + kSunZ;
    const float invH = 1.0f / std::sqrt(hx*hx + hy*hy + hz*hz);
    hx *= invH; hy *= invH; hz *= invH;
    float ndoth = nx*hx + ny*hy + nz*hz;
    if (ndoth < 0.0f) ndoth = 0.0f;
    // pow(ndoth, 48) via repeated squaring. A broad-ish sun sheen that
    // spreads across neighbouring verts and slides along crests, rather
    // than a razor cone that spikes a single vertex to white (which
    // Gouraud-interpolated into white diamonds at range).
    float s2  = ndoth * ndoth;
    float s4  = s2 * s2;
    float s8  = s4 * s4;
    float s16 = s8 * s8;
    float s32 = s16 * s16;
    const float spec = (s32 * s16) * kSpecStrength;
    r += spec; g += spec; b += spec;

    // Foam crests on the bigger swells. Soft threshold around the
    // sum-of-sines peak so it isn't a hard line.
    if (wave_dy > kFoamThreshold) {
        const float foam = (wave_dy - kFoamThreshold) * kFoamStrength;
        r += foam; g += foam; b += foam;
    }

    // High-frequency per-vertex stipple. Two unrelated sin terms keyed
    // off the grid index + time give a hash-like noise that's stable
    // for adjacent triangles to interpolate cleanly across.
    const float noise = (std::sin(fcol * 12.9898f + frow * 78.233f + t * 0.41f)
                       + std::sin(fcol * 39.346f - frow * 11.135f + t * 0.73f)
                       + std::sin(fcol *  7.531f + frow * 23.751f - t * 0.19f))
                      * (kNoiseStrength / 3.0f);
    r += noise; g += noise; b += noise;

    if (r > 1.0f) r = 1.0f; else if (r < 0.0f) r = 0.0f;
    if (g > 1.0f) g = 1.0f; else if (g < 0.0f) g = 0.0f;
    if (b > 1.0f) b = 1.0f; else if (b < 0.0f) b = 0.0f;

    const uint32_t R = static_cast<uint32_t>(r * 255.0f);
    const uint32_t G = static_cast<uint32_t>(g * 255.0f);
    const uint32_t B = static_cast<uint32_t>(b * 255.0f);
    return 0xFF000000u | (R << 16) | (G << 8) | B;
}

// Per-vertex depth derived from view.z using D3D's standard hyperbolic
// formula (depth = 1 - zNear/view.z with zNear ~ 1 and zFar ~ infinity).
// This is what D3D's own projection pipeline would compute for XYZ verts
// going through SetTransform(PROJECTION). The engine's XYZRHW verts
// almost certainly use the same monotonic-in-view.z scheme, so writing
// matching depth values keeps z-test sorting consistent: close objects
// (terrain above water) get smaller depths than the water below them
// and correctly occlude it; distant objects (sky) get larger depths
// than water in the same direction and are correctly hidden.
//
// A fixed depth (the previous hack) caused the "drowning mountains"
// bug: water with depth=0.99 beat any engine-rendered mountain whose
// hyperbolic depth happened to exceed 0.99 - which varies with view
// angle, so the apparent sea level shifted as the head rotated.
inline float DepthFromViewZ(float view_z) {
    // 1 - 1/view.z, clamped to [0, 1]. Asymptotes to 1 as view.z -> inf.
    if (view_z <= 1.0f) return 0.0f;
    const float d = 1.0f - 1.0f / view_z;
    return (d > 0.9999f) ? 0.9999f : d;
}

}  // namespace water
}  // namespace headtracking
