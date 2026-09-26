// Compiled into the config oracle library only, with `cameraunlock` and `headtracking` renamed,
// so "config.h" here is v0.2.1's (oracle/src/config.h).
#include "config.h"
#include "oracle_adapter.h"

namespace bw_oracle_view {

std::string OraclePath() { return headtracking::Config::IniPath(); }

OracleConfig RunOracle() {
    const headtracking::Config c = headtracking::Config::LoadOrCreateDefault();
    OracleConfig o{};
    o.port = c.port;
    o.enabled_on_startup = c.enabled_on_startup;
    o.sens_yaw = c.sens_yaw;
    o.sens_pitch = c.sens_pitch;
    o.sens_roll = c.sens_roll;
    o.invert_yaw = c.invert_yaw;
    o.invert_pitch = c.invert_pitch;
    o.invert_roll = c.invert_roll;
    o.local_smoothing = c.local_smoothing;
    o.remote_smoothing = c.remote_smoothing;
    o.deadzone_yaw = c.deadzone_yaw;
    o.deadzone_pitch = c.deadzone_pitch;
    o.deadzone_roll = c.deadzone_roll;
    o.pos_enabled = c.pos_enabled;
    o.pos_sens_x = c.pos_sens_x;
    o.pos_sens_y = c.pos_sens_y;
    o.pos_sens_z = c.pos_sens_z;
    o.pos_invert_x = c.pos_invert_x;
    o.pos_invert_y = c.pos_invert_y;
    o.pos_invert_z = c.pos_invert_z;
    o.pos_limit_x = c.pos_limit_x;
    o.pos_limit_y = c.pos_limit_y;
    o.pos_limit_z = c.pos_limit_z;
    o.pos_limit_z_back = c.pos_limit_z_back;
    o.pos_world_scale = c.pos_world_scale;
    o.pos_zoom_reference = c.pos_zoom_reference;
    o.pos_zoom_scale_max = c.pos_zoom_scale_max;
    o.toggle_vk = c.toggle_vk;
    o.yaw_mode_vk = c.yaw_mode_vk;
    o.mode_cycle_vk = c.mode_cycle_vk;
    o.debounce_ms = c.debounce_ms;
    o.world_space_yaw = c.world_space_yaw;
    o.log_position_trace = c.log_position_trace;
    return o;
}

}  // namespace bw_oracle_view
