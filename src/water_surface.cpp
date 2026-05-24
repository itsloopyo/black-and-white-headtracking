#include "water_surface.h"

#include <Windows.h>
#include <d3d.h>
#include <chrono>
#include <cmath>

#include "engine_addresses.h"
#include "water_shading.h"
#include "debug_log.h"

namespace headtracking {

using namespace water;

namespace {

// Seconds since the first water-render call. Used as the wave time.
inline float WaveTime() {
    static const auto s_start = std::chrono::steady_clock::now();
    return std::chrono::duration<float>(
        std::chrono::steady_clock::now() - s_start).count();
}

// Pre-transformed vertex for D3D7 XYZRHW. (sx, sy) are screen-space pixel
// coords, sz is depth in [0, 1] (z-buffer value), rhw is 1/w used for
// perspective-correct interpolation. This matches the engine's own vert
// format - the engine never uses D3D's fixed-function transform.
struct WaterVertexRHW {
    float sx, sy, sz, rhw;
    DWORD diffuse;
};
constexpr DWORD kWaterFVF_RHW = D3DFVF_XYZRHW | D3DFVF_DIFFUSE;

}  // namespace

void RenderHeadTrackedWater() {
    IDirect3DDevice7* device = GetD3DDevice();
    if (!device) return;

    // Viewport for NDC -> pixel mapping.
    D3DVIEWPORT7 vp{};
    if (FAILED(device->GetViewport(&vp))) return;
    const float halfW = vp.dwWidth  * 0.5f;
    const float halfH = vp.dwHeight * 0.5f;
    const float screen_cx = vp.dwX + halfW;
    const float screen_cy = vp.dwY + halfH;

    // Use the engine's scaled matrix (clean view * (sx,sy,1) per column).
    // ApplyHeadRotationToRenderMatrix has already rewritten this with
    // head rotation when tracking is active, so we get head-rotated
    // projection for free. When tracking is off, this is the engine's
    // clean projection.
    const float* scaled = reinterpret_cast<const float*>(kScaledMatrixAddr);
    const float* pivot  = reinterpret_cast<const float*>(kCameraPivotAddr);

    static int s_logCount = 0;
    if (s_logCount < 3) {
        HT_LOG("[water-rhw] vp=%lux%lu pivot=(%.1f, %.1f, %.1f)",
               (unsigned long)vp.dwWidth, (unsigned long)vp.dwHeight,
               pivot[0], pivot[1], pivot[2]);
        ++s_logCount;
    }

    // Tessellated grid centered on the camera's XZ. Each grid vert is
    // wave-displaced in world Y, transformed into view space, and carries
    // a per-vertex normal computed from the wave gradient. Triangles are
    // then clipped to the near plane on CPU; shading is applied at emit.
    const float eyeX = pivot[0], eyeY = pivot[1], eyeZ = pivot[2];
    const float h = kWaterHalfExtent;
    constexpr int N = kWaterGridCells;
    constexpr int kVertCount = (N + 1) * (N + 1);

    // Per-grid-vertex packet. We carry the world position, surface
    // normal, grid index, and raw wave Y alongside the view-space
    // coords so the clipped/emitted vertices can be properly shaded
    // (Fresnel + specular + foam + per-vertex noise).
    struct ViewVert {
        float vxs, vys, view_z;
        float wx, wy, wz;
        float nrm_x, nrm_y, nrm_z;
        float fcol, frow;       // grid index (used for noise stipple)
        float wave_dy;          // raw wave-only Y displacement (for foam)
        DWORD diffuse;          // shaded colour, computed once per vertex
    };
    static ViewVert vverts[kVertCount];

    const float t_now = WaveTime();
    const float step = (2.0f * h) / static_cast<float>(N);
    const float inv_step = 1.0f / step;
    WaveFrame wf[kNumWaves];
    PrecomputeWaves(t_now, wf);

    // ShadeWater is a pure function of a vertex's normal/world-pos/grid-index
    // plus the (frame-constant) eye and time. Each grid vertex is shared by up
    // to 6 triangles, so shading at emit recomputed the identical colour up to
    // 6 times per vertex. Compute it once here (and once per clipped vertex in
    // interp_at_z); emit just copies it. Same expression, same operand order,
    // same eye/time -> bit-identical diffuse to the per-emit path.
    auto compute_diffuse = [eyeX, eyeY, eyeZ, t_now](const ViewVert& v) -> DWORD {
        const float dx = eyeX - v.wx;
        const float dy = eyeY - v.wy;
        const float dz = eyeZ - v.wz;
        const float viewLen = std::sqrt(dx*dx + dy*dy + dz*dz);
        return ShadeWater(v.nrm_x, v.nrm_y, v.nrm_z,
                          dx, dy, dz, viewLen,
                          v.wave_dy, v.fcol, v.frow, t_now);
    };
    int vi = 0;
    for (int row = 0; row <= N; ++row) {
        const float wz_base = eyeZ - h + step * static_cast<float>(row);
        for (int col = 0; col <= N; ++col) {
            const float wx_base = eyeX - h + step * static_cast<float>(col);

            // Wave field is now keyed off the grid index, not world
            // position, so the animation is purely time-driven and
            // doesn't visually accelerate when the player drags the
            // camera (see WaveSpec comment).
            float dy, dy_dcol, dy_drow;
            SampleWaves(wf, static_cast<float>(col), static_cast<float>(row),
                        dy, dy_dcol, dy_drow);
            const float wy = kSeaLevelY + dy;

            // Convert grid-space derivatives to world-space so the
            // surface normal is in the same frame as everything we
            // light against. World x = col*step + offset, so dy/dx
            // = (dy/dcol) / step (same for z/row).
            float nx   = -dy_dcol * inv_step;
            float ny   =  1.0f;
            float nz_n = -dy_drow * inv_step;

            // Per-vertex hash-noise jitter on (nx, nz). Each vertex
            // gets a different small kick, so the spec/Fresnel
            // calculation produces wildly different output between
            // adjacent verts and the interpolated triangle surface
            // reads as broken-up sparkly water rather than a smooth
            // mirror gradient. The kick is animated in time so the
            // sparkle pattern shimmers rather than being static.
            const float fc = static_cast<float>(col);
            const float fr = static_cast<float>(row);
            const float h1 = std::sin(fc * 12.9898f + fr * 78.233f + t_now * 0.31f);
            const float h2 = std::sin(fc * 39.346f - fr * 11.135f + t_now * 0.43f);
            nx   += h1 * kNormalJitter;
            nz_n += h2 * kNormalJitter;

            const float inv_len = 1.0f / std::sqrt(nx*nx + ny*ny + nz_n*nz_n);
            nx *= inv_len; ny *= inv_len; nz_n *= inv_len;

            // scaled is row-major 4x3 with basis vectors as COLUMNS.
            ViewVert& v = vverts[vi++];
            v.vxs    = wx_base*scaled[0] + wy*scaled[3] + wz_base*scaled[6] + scaled[9];
            v.vys    = wx_base*scaled[1] + wy*scaled[4] + wz_base*scaled[7] + scaled[10];
            v.view_z = wx_base*scaled[2] + wy*scaled[5] + wz_base*scaled[8] + scaled[11];
            v.wx = wx_base; v.wy = wy; v.wz = wz_base;
            v.nrm_x = nx; v.nrm_y = ny; v.nrm_z = nz_n;
            v.fcol = static_cast<float>(col);
            v.frow = static_cast<float>(row);
            v.wave_dy = dy;
            v.diffuse = compute_diffuse(v);
        }
    }

    // Triangle-clip each grid cell against the near plane (view.z = nz).
    // A clipped triangle emits 0, 3, or 6 verts (1 or 2 output triangles).
    // Worst case per cell = 12 verts; over the whole grid = N*N*12.
    constexpr float kNearViewZ = 1.0f;
    static WaterVertexRHW emit[N * N * 12];
    int eo = 0;

    auto emit_vert = [&](const ViewVert& v) {
        const float inv_z = 1.0f / v.view_z;
        emit[eo].sx = screen_cx + v.vxs * inv_z * halfW;
        emit[eo].sy = screen_cy - v.vys * inv_z * halfH;
        emit[eo].sz = DepthFromViewZ(v.view_z);
        emit[eo].rhw = inv_z;
        emit[eo].diffuse = v.diffuse;
        ++eo;
    };
    auto interp_at_z = [&compute_diffuse](const ViewVert& a, const ViewVert& b,
                                          float nz, ViewVert& out) {
        const float t = (nz - a.view_z) / (b.view_z - a.view_z);
        out.vxs    = a.vxs    + t * (b.vxs    - a.vxs);
        out.vys    = a.vys    + t * (b.vys    - a.vys);
        out.view_z = nz;
        out.wx     = a.wx + t * (b.wx - a.wx);
        out.wy     = a.wy + t * (b.wy - a.wy);
        out.wz     = a.wz + t * (b.wz - a.wz);
        out.fcol   = a.fcol + t * (b.fcol - a.fcol);
        out.frow   = a.frow + t * (b.frow - a.frow);
        out.wave_dy = a.wave_dy + t * (b.wave_dy - a.wave_dy);
        // Linearly interpolate (and renormalise) the surface normal. The
        // clipped vertices live near the near plane and are mostly off-
        // screen, so this is good enough; an exact re-sample would be
        // overkill.
        float nx = a.nrm_x + t * (b.nrm_x - a.nrm_x);
        float ny = a.nrm_y + t * (b.nrm_y - a.nrm_y);
        float nz_n = a.nrm_z + t * (b.nrm_z - a.nrm_z);
        const float inv_n = 1.0f / std::sqrt(nx*nx + ny*ny + nz_n*nz_n);
        out.nrm_x = nx * inv_n;
        out.nrm_y = ny * inv_n;
        out.nrm_z = nz_n * inv_n;
        out.diffuse = compute_diffuse(out);
    };
    auto clip_tri = [&](const ViewVert& A, const ViewVert& B, const ViewVert& C) {
        const bool aIn = A.view_z > kNearViewZ;
        const bool bIn = B.view_z > kNearViewZ;
        const bool cIn = C.view_z > kNearViewZ;
        const int n = (aIn?1:0) + (bIn?1:0) + (cIn?1:0);
        if (n == 0) return;
        if (n == 3) {
            emit_vert(A); emit_vert(B); emit_vert(C);
            return;
        }
        if (n == 1) {
            const ViewVert *v_in, *v_o1, *v_o2;
            if      (aIn) { v_in = &A; v_o1 = &B; v_o2 = &C; }
            else if (bIn) { v_in = &B; v_o1 = &C; v_o2 = &A; }
            else          { v_in = &C; v_o1 = &A; v_o2 = &B; }
            ViewVert ip1, ip2;
            interp_at_z(*v_in, *v_o1, kNearViewZ, ip1);
            interp_at_z(*v_in, *v_o2, kNearViewZ, ip2);
            emit_vert(*v_in); emit_vert(ip1); emit_vert(ip2);
            return;
        }
        const ViewVert *v_o, *v_in1, *v_in2;
        if      (!aIn) { v_o = &A; v_in1 = &B; v_in2 = &C; }
        else if (!bIn) { v_o = &B; v_in1 = &C; v_in2 = &A; }
        else           { v_o = &C; v_in1 = &A; v_in2 = &B; }
        ViewVert ip1, ip2;
        interp_at_z(*v_in1, *v_o, kNearViewZ, ip1);
        interp_at_z(*v_in2, *v_o, kNearViewZ, ip2);
        emit_vert(*v_in1); emit_vert(*v_in2); emit_vert(ip2);
        emit_vert(*v_in1); emit_vert(ip2);    emit_vert(ip1);
    };

    for (int row = 0; row < N; ++row) {
        for (int col = 0; col < N; ++col) {
            const int ia = row * (N + 1) + col;
            const int ib = ia + 1;
            const int ic = ia + (N + 1);
            const int id = ic + 1;
            clip_tri(vverts[ia], vverts[ic], vverts[ib]);
            clip_tri(vverts[ib], vverts[ic], vverts[id]);
        }
    }
    if (eo == 0) return;

    // Capture every render state we're about to clobber so we can
    // restore the engine's exact pipeline configuration afterwards.
    // Leaking state (especially the texture binding on stage 0 and
    // FOG/CULL/BLEND) was causing B&W to render later geometry like
    // the cursor as solid black squares - the cursor expects its own
    // texture+modulate setup and our SetTexture(0, nullptr) made it
    // sample NULL and modulate to zero.
    struct SavedState {
        DWORD lighting, zEnable, zWrite, zFunc, cull, fill, blend, fog;
        DWORD colorOp;
        IDirectDrawSurface7* tex0;
    } saved{};
    device->GetRenderState(D3DRENDERSTATE_LIGHTING,         &saved.lighting);
    device->GetRenderState(D3DRENDERSTATE_ZENABLE,          &saved.zEnable);
    device->GetRenderState(D3DRENDERSTATE_ZWRITEENABLE,     &saved.zWrite);
    device->GetRenderState(D3DRENDERSTATE_ZFUNC,            &saved.zFunc);
    device->GetRenderState(D3DRENDERSTATE_CULLMODE,         &saved.cull);
    device->GetRenderState(D3DRENDERSTATE_FILLMODE,         &saved.fill);
    device->GetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, &saved.blend);
    device->GetRenderState(D3DRENDERSTATE_FOGENABLE,        &saved.fog);
    device->GetTextureStageState(0, D3DTSS_COLOROP,         &saved.colorOp);
    device->GetTexture(0, &saved.tex0);  // adds a reference we must release

    // Set just the render states we need for an unlit, untextured,
    // depth-tested water surface.
    device->SetRenderState(D3DRENDERSTATE_LIGHTING,         FALSE);
    device->SetRenderState(D3DRENDERSTATE_ZENABLE,          D3DZB_TRUE);
    device->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE,     TRUE);
    device->SetRenderState(D3DRENDERSTATE_ZFUNC,            D3DCMP_LESSEQUAL);
    device->SetRenderState(D3DRENDERSTATE_CULLMODE,         D3DCULL_NONE);
    device->SetRenderState(D3DRENDERSTATE_FILLMODE,         D3DFILL_SOLID);
    device->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, FALSE);
    device->SetRenderState(D3DRENDERSTATE_FOGENABLE,        FALSE);
    // Disable texturing on stage 0 so the engine's previously-bound
    // texture doesn't get sampled into our diffuse-only verts. Setting
    // COLOROP = DISABLE is cleaner than nulling the texture binding
    // because it doesn't disturb the binding itself.
    device->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_DISABLE);

    device->DrawPrimitive(D3DPT_TRIANGLELIST, kWaterFVF_RHW, emit, eo, 0);

    // Restore.
    device->SetRenderState(D3DRENDERSTATE_LIGHTING,         saved.lighting);
    device->SetRenderState(D3DRENDERSTATE_ZENABLE,          saved.zEnable);
    device->SetRenderState(D3DRENDERSTATE_ZWRITEENABLE,     saved.zWrite);
    device->SetRenderState(D3DRENDERSTATE_ZFUNC,            saved.zFunc);
    device->SetRenderState(D3DRENDERSTATE_CULLMODE,         saved.cull);
    device->SetRenderState(D3DRENDERSTATE_FILLMODE,         saved.fill);
    device->SetRenderState(D3DRENDERSTATE_ALPHABLENDENABLE, saved.blend);
    device->SetRenderState(D3DRENDERSTATE_FOGENABLE,        saved.fog);
    device->SetTextureStageState(0, D3DTSS_COLOROP,         saved.colorOp);
    device->SetTexture(0, saved.tex0);
    if (saved.tex0) saved.tex0->Release();
}

}  // namespace headtracking
