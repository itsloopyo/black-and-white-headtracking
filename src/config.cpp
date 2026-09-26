#include "config.h"

#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "legacy_config/legacy_config.h"

#include "cameraunlock/config/head_tracking_config_table.h"
#include "cameraunlock/input/key_bindings.h"
#include "cameraunlock/tracking/tracking_mode.h"

namespace headtracking {

namespace {

using cameraunlock::config::DroppedValue;
using cameraunlock::config::ImportResult;
using cameraunlock::config::LegacyInput;
using cameraunlock::config::LegacyPoseShaping;
using cameraunlock::config::PoseShapingValue;
using cameraunlock::input::KeyBinding;
using cameraunlock::input::KeyModifiers;

// A legacy hotkey code and the Ctrl+Shift chord v0.2.1 always registered beside it, as one key
// list: the code's binding when it is a key code, then the chord.
std::string KeyList(int vk, char letter, const char* key, std::vector<DroppedValue>& dropped) {
    cameraunlock::config::LegacyVirtualKeyToBindings(vk, "Hotkeys", key, dropped);
    std::vector<KeyBinding> bindings;
    if (vk >= 0x01 && vk <= 0xFE) bindings.push_back({KeyModifiers::kNone, vk});
    bindings.push_back({KeyModifiers::kCtrl | KeyModifiers::kShift, letter});
    return cameraunlock::input::FormatKeyBindings(bindings);
}

ImportResult Import(const LegacyInput& input, Config& out) {
    // v0.2.1 opened the file by the ANSI path GetModuleFileNameA gave it, which is the owner's
    // ANSI form of the same path.
    legacy::Config c;
    const legacy::ReadStatus read = legacy::Read(input.ansi_path.c_str(), c);

    std::vector<DroppedValue> dropped;
    std::vector<PoseShapingValue> shaping;

    out.udp_port = c.port;
    out.enable_on_startup = c.enabled_on_startup;
    out.world_space_yaw = c.world_space_yaw;

    // [Position] Enabled chose only the startup mode: the cycle key reached every mode either
    // way.
    const cameraunlock::TrackingModeChannels mode = cameraunlock::EncodeTrackingMode(
        c.pos_enabled ? cameraunlock::TrackingMode::RotationAndPosition
                      : cameraunlock::TrackingMode::RotationOnly);
    out.rotation_enabled = mode.rotation_enabled;
    out.position_enabled = mode.position_enabled;

    out.local_smoothing = c.local_smoothing;
    out.position.local_smoothing = c.local_smoothing;
    out.remote_smoothing = c.remote_smoothing;
    out.position.remote_smoothing = c.remote_smoothing;

    // The old file had one vertical limit, which the old runtime applied both ways.
    out.position.limit_x = c.pos_limit_x;
    out.position.limit_y = c.pos_limit_y;
    out.position.limit_y_down = c.pos_limit_y;
    out.position.limit_z = c.pos_limit_z;
    out.position.limit_z_back = c.pos_limit_z_back;

    out.pos_zoom_reference = c.pos_zoom_reference;
    out.pos_zoom_scale_max = c.pos_zoom_scale_max;
    out.log_position_trace = c.log_position_trace;

    // Every sensitivity, inversion and deadzone shipped at identity, so nothing folds and a
    // value the player changed is dropped. The unit scale shipped at the engine units per metre
    // the camera path now applies itself.
    LegacyPoseShaping(c.sens_yaw, 1.0f, "Sensitivity", "Yaw", shaping, dropped);
    LegacyPoseShaping(c.sens_pitch, 1.0f, "Sensitivity", "Pitch", shaping, dropped);
    LegacyPoseShaping(c.sens_roll, 1.0f, "Sensitivity", "Roll", shaping, dropped);
    LegacyPoseShaping(c.invert_yaw, false, "Sensitivity", "InvertYaw", shaping, dropped);
    LegacyPoseShaping(c.invert_pitch, false, "Sensitivity", "InvertPitch", shaping, dropped);
    LegacyPoseShaping(c.invert_roll, false, "Sensitivity", "InvertRoll", shaping, dropped);
    LegacyPoseShaping(c.deadzone_yaw, 0.0f, "Deadzone", "Yaw", shaping, dropped);
    LegacyPoseShaping(c.deadzone_pitch, 0.0f, "Deadzone", "Pitch", shaping, dropped);
    LegacyPoseShaping(c.deadzone_roll, 0.0f, "Deadzone", "Roll", shaping, dropped);
    LegacyPoseShaping(c.pos_world_scale, kWorldUnitsPerMetre, "Position", "WorldScale", shaping, dropped);
    LegacyPoseShaping(c.pos_sens_x, 1.0f, "Position", "SensX", shaping, dropped);
    LegacyPoseShaping(c.pos_sens_y, 1.0f, "Position", "SensY", shaping, dropped);
    LegacyPoseShaping(c.pos_sens_z, 1.0f, "Position", "SensZ", shaping, dropped);
    LegacyPoseShaping(c.pos_invert_x, false, "Position", "InvertX", shaping, dropped);
    LegacyPoseShaping(c.pos_invert_y, false, "Position", "InvertY", shaping, dropped);
    LegacyPoseShaping(c.pos_invert_z, false, "Position", "InvertZ", shaping, dropped);

    out.toggle_key_name = KeyList(c.toggle_vk, 'Y', "Toggle", dropped);
    out.cycle_tracking_mode_key_name = KeyList(c.mode_cycle_vk, 'G', "ModeCycle", dropped);
    out.yaw_mode_key_name = KeyList(c.yaw_mode_vk, 'H', "YawMode", dropped);

    return read == legacy::ReadStatus::Absent ? ImportResult::Absent(std::move(dropped), std::move(shaping))
                                              : ImportResult::Imported(std::move(dropped), std::move(shaping));
}

}  // namespace

cameraunlock::config::ConfigTable<Config> MakeConfigTable() {
    using cameraunlock::config::schema::Concept;
    cameraunlock::config::ConfigTable<Config> table = cameraunlock::config::HeadTrackingConfigTable<Config>(
        {Concept::UdpPort, Concept::EnableOnStartup, Concept::WorldSpaceYaw, Concept::RotationEnabled,
         Concept::LocalSmoothing, Concept::RemoteSmoothing, Concept::PositionEnabled, Concept::PositionLimitX,
         Concept::PositionLimitY, Concept::PositionLimitYDown, Concept::PositionLimitZ, Concept::PositionLimitZBack,
         Concept::ToggleKey, Concept::CycleTrackingModeKey, Concept::YawModeKey});
    table.Select(Concept::WorldSpaceYaw).Writable()
        .Select(Concept::RotationEnabled).Writable()
        .Select(Concept::PositionEnabled).Writable();
    constexpr double kLargestFloat = std::numeric_limits<float>::max();
    table.Local("Position", "ZoomReference", &Config::pos_zoom_reference, cameraunlock::config::FloatCodec(),
                "The zoom at which leaning is not scaled, as the camera's distance to what it looks at\n"
                "in the game's units. At other zooms leaning is scaled by the ratio of the two distances,\n"
                "within ZoomScaleMax. 0 uses the first zoom the game shows. PositionTrace logs the distance.")
        .Range(0.0, kLargestFloat);
    table.Local("Position", "ZoomScaleMax", &Config::pos_zoom_scale_max, cameraunlock::config::FloatCodec(),
                "The most the zoom scaling may multiply or divide leaning by. 1 turns it off.\n"
                "Lower it if leaning moves the view too far when zoomed out.")
        .Range(1.0, kLargestFloat);
    table.Local("Logging", "PositionTrace", &Config::log_position_trace, cameraunlock::config::BoolCodec(),
                "true: write the tracked head position, the view's offset and the zoom to HeadTracking.log\n"
                "once a second, for the first minute of positional tracking.");
    return table;
}

cameraunlock::config::LegacyImport<Config> MakeLegacyImport() {
    return {&Import, legacy::ReadKeys()};
}

cameraunlock::config::ConfigOwnerOptions<Config> MakeConfigOwnerOptions(const std::wstring& folder,
                                                                        cameraunlock::config::DefaultsFile defaults) {
    const auto wide = [](const char* name) { return std::wstring(name, name + std::char_traits<char>::length(name)); };
    cameraunlock::config::ConfigOwnerOptions<Config> options;
    options.path = folder + wide(kConfigFileName);
    options.legacy_path = folder + wide(kLegacyConfigFileName);
    options.table = MakeConfigTable();
    options.import = MakeLegacyImport();
    options.header.display_name = kConfigDisplayName;
    options.defaults = std::move(defaults);
    return options;
}

}  // namespace headtracking
