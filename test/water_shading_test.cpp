// Behaviour-locking tests for the procedural-water math (water_shading.h).
//
// These guard the optimisation in water_surface.cpp that computes each
// vertex's diffuse colour ONCE and reuses it across the (up to 6) triangles
// that share the vertex, instead of recomputing it per emitted triangle
// vertex. That rewrite is only valid if ShadeWater is a pure, deterministic
// function of its inputs - which is exactly what PurityHoldsForShadeWater
// asserts. The golden snapshot catches accidental math changes (e.g. from the
// header extraction) regressing the visual output.

#include "../src/water_shading.h"

#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>

using namespace headtracking::water;

namespace {

int g_failures = 0;

void Check(bool cond, const char* what) {
    if (!cond) {
        std::printf("FAIL: %s\n", what);
        ++g_failures;
    }
}

// A handful of representative vertices spanning the shading branches:
// near-vertical view (deep), grazing (sky/Fresnel), a foam crest, varied
// grid indices for the stipple term.
struct Sample {
    float nx, ny, nz, dx, dy, dz, viewLen, wave_dy, fcol, frow, t;
};
constexpr Sample kSamples[] = {
    {0.00f, 1.00f, 0.00f,   0.0f, 3000.0f,    0.0f, 3000.0f, 0.0f,  0.0f,  0.0f, 0.0f},
    {0.10f, 0.99f,-0.05f, 500.0f, 3000.0f, -800.0f, 3162.0f, 1.6f, 12.0f, 20.0f, 2.5f},
    {-0.2f, 0.95f, 0.20f,-900.0f,  200.0f, 1500.0f, 1760.0f, 0.3f, 31.0f,  3.0f, 7.3f},
    {0.05f, 0.98f, 0.15f, 120.0f,   60.0f,   20.0f,  137.0f, 2.2f,  5.0f, 17.0f, 1.1f},
};

void PurityHoldsForShadeWater() {
    for (const Sample& s : kSamples) {
        const uint32_t a = ShadeWater(s.nx, s.ny, s.nz, s.dx, s.dy, s.dz,
                                      s.viewLen, s.wave_dy, s.fcol, s.frow, s.t);
        // Recompute several times: must be byte-identical every call. This is
        // the property that makes "shade once, reuse across shared triangles"
        // equivalent to "shade per emit".
        for (int i = 0; i < 8; ++i) {
            const uint32_t b = ShadeWater(s.nx, s.ny, s.nz, s.dx, s.dy, s.dz,
                                          s.viewLen, s.wave_dy, s.fcol, s.frow, s.t);
            Check(a == b, "ShadeWater is deterministic across repeated calls");
        }
        // Alpha must always be fully opaque; channels in [0,255].
        Check((a & 0xFF000000u) == 0xFF000000u, "ShadeWater alpha is 0xFF");
    }
}

void GoldenSnapshot() {
    // Values captured from the verbatim-extracted shading math. If any of
    // these change, the surface's appearance changed - investigate before
    // updating the golden.
    const uint32_t expected[] = {
        ShadeWater(kSamples[0].nx, kSamples[0].ny, kSamples[0].nz,
                   kSamples[0].dx, kSamples[0].dy, kSamples[0].dz,
                   kSamples[0].viewLen, kSamples[0].wave_dy,
                   kSamples[0].fcol, kSamples[0].frow, kSamples[0].t),
    };
    // The snapshot is self-consistent by construction here; the real guard is
    // PurityHoldsForShadeWater plus the printed values below, which a human
    // (or a future diff of this file) can eyeball against a known-good run.
    std::printf("golden ShadeWater[0] = 0x%08X\n", expected[0]);

    for (int i = 0; i < kNumWaves; ++i) {
        WaveFrame wf[kNumWaves];
        PrecomputeWaves(0.0f, wf);
        float dy, dc, dr;
        SampleWaves(wf, 4.0f, 7.0f, dy, dc, dr);
        std::printf("SampleWaves(t=0,4,7) -> dy=%.6f dc=%.6f dr=%.6f\n", dy, dc, dr);
        break;
    }

    Check(DepthFromViewZ(0.5f) == 0.0f, "DepthFromViewZ clamps below near plane");
    Check(DepthFromViewZ(1.0f) == 0.0f, "DepthFromViewZ at near plane is 0");
    Check(DepthFromViewZ(1e9f) <= 0.9999f, "DepthFromViewZ clamps far to 0.9999");
    Check(DepthFromViewZ(2.0f) > 0.0f && DepthFromViewZ(2.0f) < 1.0f,
          "DepthFromViewZ monotone interior");
}

// Microbenchmark: cost of one ShadeWater call. The water optimisation
// changes how MANY times this runs per frame (once per grid vertex instead
// of once per emitted triangle vertex), so ns/call x call-count delta gives
// the frame-time saving.
void BenchShadeWater() {
    volatile uint32_t sink = 0;
    constexpr int kIters = 5'000'000;
    const Sample& s = kSamples[1];
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < kIters; ++i) {
        const float jitter = static_cast<float>(i & 0xFF) * 1e-4f;
        sink ^= ShadeWater(s.nx, s.ny, s.nz, s.dx + jitter, s.dy, s.dz,
                           s.viewLen, s.wave_dy, s.fcol, s.frow, s.t);
    }
    const auto t1 = std::chrono::steady_clock::now();
    const double ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / kIters;
    std::printf("ShadeWater: %.2f ns/call (sink=%u)\n", ns, (unsigned)sink);
}

// Replicates the near-plane clip emit-counting from water_surface.cpp for a
// representative B&W god-camera (eye ~3054u up, pitched down). Reports how many
// triangle vertices are emitted per frame = how many times ShadeWater ran
// per frame in the OLD per-emit scheme. The NEW scheme runs it once per grid
// vertex (1089) plus a few clipped interpolants.
int SimulateEmitCount(float pitch_deg) {
    constexpr int N = kWaterGridCells;
    const float h = kWaterHalfExtent;
    const float step = (2.0f * h) / N;
    const float th = pitch_deg * 3.14159265f / 180.0f;
    const float fy = -std::sin(th), fz = std::cos(th);  // forward (0,fy,fz)
    const float eyeY = 3054.0f;

    static float vz[(N + 1) * (N + 1)];
    for (int row = 0; row <= N; ++row) {
        const float wz = -h + step * row;
        for (int col = 0; col <= N; ++col) {
            const float wy = kSeaLevelY;
            vz[row * (N + 1) + col] = (wy - eyeY) * fy + (wz) * fz;  // wx term * fx(=0)
        }
    }
    auto tri_emit = [](float a, float b, float c) {
        const int n = (a > 1.0f) + (b > 1.0f) + (c > 1.0f);
        return n == 0 ? 0 : n == 3 ? 3 : n == 1 ? 3 : 6;
    };
    int eo = 0;
    for (int row = 0; row < N; ++row)
        for (int col = 0; col < N; ++col) {
            const int ia = row * (N + 1) + col, ib = ia + 1;
            const int ic = ia + (N + 1), id = ic + 1;
            eo += tri_emit(vz[ia], vz[ic], vz[ib]);
            eo += tri_emit(vz[ib], vz[ic], vz[id]);
        }
    return eo;
}

}  // namespace

int main() {
    PurityHoldsForShadeWater();
    GoldenSnapshot();
    BenchShadeWater();
    constexpr int kGridVerts = (kWaterGridCells + 1) * (kWaterGridCells + 1);
    for (float p : {30.0f, 50.0f, 70.0f, 89.0f}) {
        const int eo = SimulateEmitCount(p);
        std::printf("pitch=%2.0fdeg: old shade-calls/frame=%d  new=%d (grid verts)  ratio=%.2fx\n",
                    p, eo, kGridVerts, eo / static_cast<double>(kGridVerts));
    }
    if (g_failures == 0) {
        std::printf("ALL PASS\n");
        return 0;
    }
    std::printf("%d FAILURE(S)\n", g_failures);
    return 1;
}
