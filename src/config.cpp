#include "config.h"

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
using cameraunlock::config::LegacyClampToRange;
using cameraunlock::config::LegacyFollowsDefaultsIni;
using cameraunlock::config::LegacyInput;
using cameraunlock::config::LegacyPoseShaping;
using cameraunlock::config::PoseShapingValue;
using cameraunlock::config::schema::Concept;
using cameraunlock::input::KeyBinding;
using cameraunlock::input::KeyModifiers;

// A legacy hotkey code and the Ctrl+Shift chord v0.2.1 always registered beside it, as one key
// list: the code's binding (none for a code no hotkey can hold, N1 and N3), then the chord.
std::string KeyList(int vk, char letter, const char* key, std::vector<DroppedValue>& dropped) {
    const std::string code = cameraunlock::config::LegacyVirtualKeyToBindings(vk, "Hotkeys", key, dropped);
    const std::string chord = cameraunlock::input::FormatKeyBindings(
        std::vector<KeyBinding>{{KeyModifiers::kCtrl | KeyModifiers::kShift, letter}});
    return code.empty() ? chord : code + ", " + chord;
}

ImportResult Import(const LegacyInput& input, Config& out) {
    // v0.2.1 opened the file by the ANSI path GetModuleFileNameA gave it, which is the owner's
    // ANSI form of the same path.
    legacy::Config c;
    const legacy::ReadStatus read = legacy::Read(input.ansi_path.c_str(), c);
    const legacy::Config shipped;

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

    // The old file had one vertical limit, which the old runtime applied both ways. The reader
    // replaced a limit that was not finite or was below 0 with its default and took any other; one
    // above the rows' 10 imports as 10 (N4).
    using LimitY = cameraunlock::config::schema::ConceptTraits<Concept::PositionLimitY>;
    using LimitYDown = cameraunlock::config::schema::ConceptTraits<Concept::PositionLimitYDown>;
    static_assert(LimitY::kMin == LimitYDown::kMin && LimitY::kMax == LimitYDown::kMax,
                  "LimitY fills both vertical rows, so they take one range");
    out.position.limit_x = LegacyClampToRange<Concept::PositionLimitX>(c.pos_limit_x, "Position", "LimitX", dropped);
    out.position.limit_y = LegacyClampToRange<Concept::PositionLimitY>(c.pos_limit_y, "Position", "LimitY", dropped);
    out.position.limit_y_down = out.position.limit_y;
    out.position.limit_z = LegacyClampToRange<Concept::PositionLimitZ>(c.pos_limit_z, "Position", "LimitZ", dropped);
    out.position.limit_z_back =
        LegacyClampToRange<Concept::PositionLimitZBack>(c.pos_limit_z_back, "Position", "LimitZBack", dropped);

    out.log_position_trace = c.log_position_trace;

    // Every sensitivity, inversion and deadzone shipped at identity, so nothing folds and a
    // value the player changed is dropped. The unit scale and its zoom scaling shipped at the
    // values the camera path now applies itself.
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
    // 0 was the first gameplay zoom, which the camera path now always locks to.
    LegacyPoseShaping(c.pos_zoom_reference, 0.0f, "Position", "ZoomReference", shaping, dropped);
    LegacyPoseShaping(c.pos_zoom_scale_max, kZoomScaleMax, "Position", "ZoomScaleMax", shaping, dropped);
    LegacyPoseShaping(c.pos_sens_x, 1.0f, "Position", "SensX", shaping, dropped);
    LegacyPoseShaping(c.pos_sens_y, 1.0f, "Position", "SensY", shaping, dropped);
    LegacyPoseShaping(c.pos_sens_z, 1.0f, "Position", "SensZ", shaping, dropped);
    LegacyPoseShaping(c.pos_invert_x, false, "Position", "InvertX", shaping, dropped);
    LegacyPoseShaping(c.pos_invert_y, false, "Position", "InvertY", shaping, dropped);
    LegacyPoseShaping(c.pos_invert_z, false, "Position", "InvertZ", shaping, dropped);

    out.toggle_key_name = KeyList(c.toggle_vk, 'Y', "Toggle", dropped);
    out.cycle_tracking_mode_key_name = KeyList(c.mode_cycle_vk, 'G', "ModeCycle", dropped);
    out.yaw_mode_key_name = KeyList(c.yaw_mode_vk, 'H', "YawMode", dropped);

    // A row still at what v0.2.1 ran on with no file is no player's choice, so it follows
    // Defaults.ini. The chords were fixed in code, so each hotkey's code decides alone. A limit is
    // compared as read, so one N4 clamped is the player's.
    LegacyFollowsDefaultsIni follows;
    follows.Setting(Concept::UdpPort, c.port, shipped.port);
    follows.Setting(Concept::EnableOnStartup, c.enabled_on_startup, shipped.enabled_on_startup);
    follows.Setting(Concept::WorldSpaceYaw, c.world_space_yaw, shipped.world_space_yaw);
    follows.TrackingMode(c.pos_enabled, shipped.pos_enabled);
    follows.Setting(Concept::LocalSmoothing, c.local_smoothing, shipped.local_smoothing);
    follows.Setting(Concept::RemoteSmoothing, c.remote_smoothing, shipped.remote_smoothing);
    follows.Setting(Concept::PositionLimitX, c.pos_limit_x, shipped.pos_limit_x);
    follows.Setting(Concept::PositionLimitY, c.pos_limit_y, shipped.pos_limit_y);
    follows.Setting(Concept::PositionLimitYDown, c.pos_limit_y, shipped.pos_limit_y);
    follows.Setting(Concept::PositionLimitZ, c.pos_limit_z, shipped.pos_limit_z);
    follows.Setting(Concept::PositionLimitZBack, c.pos_limit_z_back, shipped.pos_limit_z_back);
    follows.Setting(Concept::ToggleKey, c.toggle_vk, shipped.toggle_vk);
    follows.Setting(Concept::CycleTrackingModeKey, c.mode_cycle_vk, shipped.mode_cycle_vk);
    follows.Setting(Concept::YawModeKey, c.yaw_mode_vk, shipped.yaw_mode_vk);
    follows.NotInLegacy(Concept::CollisionEnabled);
    follows.NotInLegacy(Concept::CollisionReleaseSmoothing);

    return read == legacy::ReadStatus::Absent
               ? ImportResult::Absent(std::move(dropped), std::move(shaping), follows.Concepts())
               : ImportResult::Imported(std::move(dropped), std::move(shaping), follows.Concepts());
}

}  // namespace

cameraunlock::config::ConfigTable<Config> MakeConfigTable() {
    cameraunlock::config::ConfigTable<Config> table = cameraunlock::config::HeadTrackingConfigTable<Config>(
        {Concept::UdpPort, Concept::EnableOnStartup, Concept::WorldSpaceYaw, Concept::RotationEnabled,
         Concept::LocalSmoothing, Concept::RemoteSmoothing, Concept::PositionEnabled, Concept::PositionLimitX,
         Concept::PositionLimitY, Concept::PositionLimitYDown, Concept::PositionLimitZ, Concept::PositionLimitZBack,
         Concept::CollisionEnabled, Concept::CollisionMargin, Concept::CollisionReleaseSmoothing,
         Concept::ToggleKey, Concept::CycleTrackingModeKey, Concept::YawModeKey});
    table.Select(Concept::CollisionMargin)
        .Comment("How far above the land the view is held when you lean towards it, in the game's own units.\n"
                 "3 is the distance the game's own camera keeps. The mod never holds it closer than the\n"
                 "camera's near clip plane reaches, which it reads from the game.");
    table.Select(Concept::WorldSpaceYaw).Writable()
        .Select(Concept::RotationEnabled).Writable()
        .Select(Concept::PositionEnabled).Writable();
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
