// The config differential test (convert-a-mod-to-the-canonical-config, section 5). Every input
// is read three ways:
//
//   oracle     the reader of v0.2.1, the newest published build, with the core sources it
//              compiled at its pin c480d8a (oracle_adapter.h)
//   import     the frozen reader in src/legacy_config/
//   migration  the config owner in a folder holding only HeadTracking.ini, the legacy file,
//              importing it into a new CameraUnlock.ini, then the canonical reader and table on
//              that file
//
// Comparison 1, oracle against import, on every input: load status, every field both read
// (floats bit for bit), the startup state, and which actions every key press fires under every
// set of held modifiers. The differences it may find are kComparison1Differences below.
//
// Comparison 2, import against migration, is the proof for the migration: the settings the mod
// starts on are the import's, apart from the approved changes, each of which the import must
// record as dropped. A sensitivity, inversion, deadzone or WorldScale the player set away from
// its shipped value is dropped (pose_shaping), and a hotkey code outside 0x01-0xFE imports as
// unbound (N1). A value the canonical row cannot hold has no approved rule, so the owner defers
// that import and the session runs on what the import gave (kUnrepresentable). The hotkeys fire
// as the fleet's key lists fire (kFleetHotkeyRule): exactly as v0.2.1 fired them, except for a
// press made while Ctrl and Shift are both held.
//
// Comparison 2 runs twice, once over a Defaults.ini at the built-in values and once over one a
// player changed, since the migration writes default exactly where the imported value equals
// what Defaults.ini gives. After every load HeadTracking.ini keeps its bytes, its write time and
// its attributes, Defaults.ini is never written, and the folder holds the legacy file and
// CameraUnlock.ini and nothing else. The next load reads CameraUnlock.ini, imports nothing and
// writes nothing, and a read-only legacy file imports as a writable one does.
//
// The distinct migrated files are written beside the executable under migrated\, for
// lint-migrated.mjs to run core's canonical config lint over.
//
// Inputs: no file, an empty file, the first-run output of every published build (v0.1.0,
// v0.1.2 and v0.1.4 wrote the same file), every committed version of config/HeadTracking.ini up
// to v0.2.1 (the installer and Nexus ZIPs carried it; no build seeded one through the launcher),
// and core's corpus over v0.2.1's first-run output and over the file v0.2.1 shipped.

#include "config.h"
#include "legacy_config/legacy_config.h"
#include "oracle_adapter.h"

#include "cameraunlock/config/config_owner.h"
#include "cameraunlock/config/defaults_file.h"
#include "cameraunlock/config/testing/ini_mutations.h"
#include "cameraunlock/input/key_binding_registration.h"
#include "cameraunlock/input/key_bindings.h"
#include "cameraunlock/tracking/tracking_mode.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iterator>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace headtracking;

namespace {

// v0.2.1 and the frozen import read the file with the same source (src/config.cpp did not change
// between the v0.2.1 tag and the commit that froze it) and the same core IniReader (unchanged
// since c480d8a), so comparison 1 has no differences to record.
const char* const kComparison1Differences[] = {
    "none",
};

// v0.2.1 registered each hotkey code with no guard and the Ctrl+Shift chord letters beside them,
// so a code fired whatever modifiers were held, and a code bound to the chord's own letter fired
// its action twice under Ctrl+Shift. The fleet's key lists fire a binding without modifiers only
// while Ctrl and Shift are not both held, and one press runs an action once. The two differ only
// for a press made while Ctrl and Shift are both held.
const char* const kFleetHotkeyRule =
    "a hotkey code no longer fires while Ctrl and Shift are both held, and a press fires its action once";

// v0.2.1 read the four [Position] limits with no upper bound, and the canonical rows take 0 to 10
// metres. Core has no rule for a value outside a concept's range, so the owner defers such a
// file: it stays as it is, the session runs on what the import read, and nothing is saved.
const char* const kUnrepresentable =
    "[Position] LimitX, LimitY, LimitZ or LimitZBack above 10, which the canonical rows cannot hold, so the "
    "import defers";

constexpr float kMaxCanonicalLimit = 10.0f;

constexpr const char* kFileName = "HeadTracking.ini";

int g_failures = 0;
int g_checks = 0;

void Check(bool cond, const std::string& what) {
    ++g_checks;
    if (!cond) {
        if (g_failures < 200) std::printf("  FAIL: %s\n", what.c_str());
        ++g_failures;
    }
}

bool SameBits(float a, float b) { return std::memcmp(&a, &b, sizeof a) == 0; }

std::string ReadBytes(const fs::path& path) {
    std::ifstream in(path, std::ios::binary);
    if (!in) throw std::runtime_error("could not read " + path.string());
    return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

void WriteBytes(const fs::path& path, const std::string& bytes) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    if (!out) throw std::runtime_error("could not write " + path.string());
}

void SetReadOnly(const fs::path& path, bool readOnly) {
    const DWORD attrs = GetFileAttributesW(path.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES) throw std::runtime_error("no attributes for " + path.string());
    const DWORD next = readOnly ? (attrs | FILE_ATTRIBUTE_READONLY) : (attrs & ~FILE_ATTRIBUTE_READONLY);
    if (!SetFileAttributesW(path.c_str(), next)) throw std::runtime_error("could not set attributes on " + path.string());
}

using Listing = std::vector<std::pair<std::string, std::string>>;

Listing List(const fs::path& dir) {
    Listing l;
    for (const auto& e : fs::directory_iterator(dir)) {
        l.emplace_back(e.path().filename().string(), ReadBytes(e.path()));
    }
    std::sort(l.begin(), l.end());
    return l;
}

struct Input {
    std::string name;
    std::optional<std::string> bytes;  // nullopt: no file
};

// Startup state as v0.2.1's Plugin::Initialize derives it from the config: enabled from
// enabled_on_startup, RotationAndPosition when pos_enabled else RotationOnly, the yaw mode from
// world_space_yaw.
struct Startup {
    bool enabled;
    int mode;  // cameraunlock::TrackingMode: 0 rotation and position, 1 rotation only
    bool world_space_yaw;
    bool operator==(const Startup& o) const {
        return enabled == o.enabled && mode == o.mode && world_space_yaw == o.world_space_yaw;
    }
};

Startup StartupOf(const bw_oracle_view::OracleConfig& c) {
    return {c.enabled_on_startup, c.pos_enabled ? 0 : 1, c.world_space_yaw};
}

Startup StartupOf(const legacy::Config& c) {
    return {c.enabled_on_startup, c.pos_enabled ? 0 : 1, c.world_space_yaw};
}

bool CtrlShiftHeld(int held) { return (held & 3) == 3; }

// The first press whose fired actions differ, for the failure message.
std::string FirstFireDifference(const bw_oracle_view::FireTable& expected, const bw_oracle_view::FireTable& got) {
    for (std::size_t i = 0; i < expected.size() && i < got.size(); ++i) {
        if (expected[i] != got[i]) {
            char text[160];
            std::snprintf(text, sizeof text, "key 0x%02X held %d fires %d/%d/%d, not %d/%d/%d",
                          static_cast<int>(i / bw_oracle_view::kHeldStates) + bw_oracle_view::kFirstKey,
                          static_cast<int>(i % bw_oracle_view::kHeldStates), got[i][0], got[i][1], got[i][2],
                          expected[i][0], expected[i][1], expected[i][2]);
            return text;
        }
    }
    return expected.size() == got.size() ? "none" : "the tables differ in size";
}

// Every field the import reads, against the oracle's field of the same name.
std::vector<std::string> FieldDifferences(const bw_oracle_view::OracleConfig& o, const legacy::Config& i) {
    std::vector<std::string> d;
    auto b = [&d](const char* n, bool x, bool y) { if (x != y) d.push_back(n); };
    auto f = [&d](const char* n, float x, float y) { if (!SameBits(x, y)) d.push_back(n); };
    auto n = [&d](const char* name, long long x, long long y) { if (x != y) d.push_back(name); };
    n("port", o.port, i.port);
    b("enabled_on_startup", o.enabled_on_startup, i.enabled_on_startup);
    f("sens_yaw", o.sens_yaw, i.sens_yaw);
    f("sens_pitch", o.sens_pitch, i.sens_pitch);
    f("sens_roll", o.sens_roll, i.sens_roll);
    b("invert_yaw", o.invert_yaw, i.invert_yaw);
    b("invert_pitch", o.invert_pitch, i.invert_pitch);
    b("invert_roll", o.invert_roll, i.invert_roll);
    f("local_smoothing", o.local_smoothing, i.local_smoothing);
    f("remote_smoothing", o.remote_smoothing, i.remote_smoothing);
    f("deadzone_yaw", o.deadzone_yaw, i.deadzone_yaw);
    f("deadzone_pitch", o.deadzone_pitch, i.deadzone_pitch);
    f("deadzone_roll", o.deadzone_roll, i.deadzone_roll);
    b("pos_enabled", o.pos_enabled, i.pos_enabled);
    f("pos_sens_x", o.pos_sens_x, i.pos_sens_x);
    f("pos_sens_y", o.pos_sens_y, i.pos_sens_y);
    f("pos_sens_z", o.pos_sens_z, i.pos_sens_z);
    b("pos_invert_x", o.pos_invert_x, i.pos_invert_x);
    b("pos_invert_y", o.pos_invert_y, i.pos_invert_y);
    b("pos_invert_z", o.pos_invert_z, i.pos_invert_z);
    f("pos_limit_x", o.pos_limit_x, i.pos_limit_x);
    f("pos_limit_y", o.pos_limit_y, i.pos_limit_y);
    f("pos_limit_z", o.pos_limit_z, i.pos_limit_z);
    f("pos_limit_z_back", o.pos_limit_z_back, i.pos_limit_z_back);
    f("pos_world_scale", o.pos_world_scale, i.pos_world_scale);
    f("pos_zoom_reference", o.pos_zoom_reference, i.pos_zoom_reference);
    f("pos_zoom_scale_max", o.pos_zoom_scale_max, i.pos_zoom_scale_max);
    n("toggle_vk", o.toggle_vk, i.toggle_vk);
    n("yaw_mode_vk", o.yaw_mode_vk, i.yaw_mode_vk);
    n("mode_cycle_vk", o.mode_cycle_vk, i.mode_cycle_vk);
    n("debounce_ms", o.debounce_ms, i.debounce_ms);
    b("world_space_yaw", o.world_space_yaw, i.world_space_yaw);
    b("log_position_trace", o.log_position_trace, i.log_position_trace);
    return d;
}

std::string Join(const std::vector<std::string>& v) {
    std::string s;
    for (const std::string& x : v) s += (s.empty() ? "" : ", ") + x;
    return s;
}

// The corpus descriptor of every key the frozen reader reads.
std::vector<cameraunlock::config::testing::MutationKey> MutationKeys() {
    using cameraunlock::config::testing::MutationKey;
    auto plain = [](const char* s, const char* k, const char* alt, std::vector<std::string> oor = {}) {
        MutationKey m;
        m.section = s;
        m.key = k;
        m.alternate = alt;
        m.out_of_range = std::move(oor);
        return m;
    };
    auto hotkey = [](const char* k, const char* alt) {
        MutationKey m;
        m.section = "Hotkeys";
        m.key = k;
        m.alternate = alt;
        m.out_of_range = {"0x100"};
        m.hotkey = true;
        return m;
    };
    return {
        plain("Network", "Port", "4243", {"0", "65536"}),
        plain("Network", "EnableOnStartup", "false"),
        plain("Sensitivity", "Yaw", "0.5"),
        plain("Sensitivity", "Pitch", "0.5"),
        plain("Sensitivity", "Roll", "0.5"),
        plain("Sensitivity", "InvertYaw", "true"),
        plain("Sensitivity", "InvertPitch", "true"),
        plain("Sensitivity", "InvertRoll", "true"),
        plain("Smoothing", "LocalSmoothing", "0.3", {"-0.5", "1.5"}),
        plain("Smoothing", "RemoteSmoothing", "0.3", {"-0.5", "1.5"}),
        plain("Smoothing", "Amount", "0.3"),
        plain("Position", "Smoothing", "0.3"),
        plain("Deadzone", "Yaw", "2.0", {"-1.0"}),
        plain("Deadzone", "Pitch", "2.0", {"-1.0"}),
        plain("Deadzone", "Roll", "2.0", {"-1.0"}),
        plain("Position", "Enabled", "false"),
        plain("Position", "WorldScale", "60.0", {"-1.0"}),
        plain("Position", "ZoomReference", "500.0", {"-1.0"}),
        plain("Position", "ZoomScaleMax", "4.0", {"0.5"}),
        plain("Position", "SensX", "0.5"),
        plain("Position", "SensY", "0.5"),
        plain("Position", "SensZ", "0.5"),
        plain("Position", "InvertX", "true"),
        plain("Position", "InvertY", "true"),
        plain("Position", "InvertZ", "true"),
        plain("Position", "LimitX", "0.5", {"-0.3", "11"}),
        plain("Position", "LimitY", "0.5", {"-0.2", "11"}),
        plain("Position", "LimitZ", "0.5", {"-0.4", "11"}),
        plain("Position", "LimitZBack", "0.2", {"-0.1", "11"}),
        hotkey("Toggle", "0x70"),
        hotkey("YawMode", "0x72"),
        hotkey("ModeCycle", "0x71"),
        plain("Hotkeys", "DebounceMs", "300"),
        plain("View", "WorldSpaceYaw", "false"),
        plain("Logging", "PositionTrace", "true"),
    };
}

// One folder per reading under a root of this process's own, emptied before each input so the
// test never holds more than one input's files.
class Scratch {
public:
    Scratch() {
        root_ = fs::temp_directory_path() / ("bw-config-differential-" + std::to_string(GetCurrentProcessId()));
        Remove(root_);
        fs::create_directories(root_);
    }
    ~Scratch() { Remove(root_); }
    Scratch(const Scratch&) = delete;
    Scratch& operator=(const Scratch&) = delete;

    // root/leaf, created empty.
    fs::path Clean(const std::string& leaf) {
        const fs::path dir = root_ / leaf;
        Remove(dir);
        fs::create_directories(dir);
        return dir;
    }

private:
    // Read-only files included, which remove_all will not delete.
    static void Remove(const fs::path& dir) {
        std::error_code ec;
        if (!fs::exists(dir, ec)) return;
        for (const auto& e : fs::recursive_directory_iterator(dir, ec)) {
            if (e.is_regular_file()) SetFileAttributesW(e.path().c_str(), FILE_ATTRIBUTE_NORMAL);
        }
        fs::remove_all(dir, ec);
        if (ec) throw std::runtime_error("could not empty " + dir.string() + ": " + ec.message());
    }

    fs::path root_;
};

fs::path Place(const fs::path& dir, const Input& input) {
    const fs::path file = dir / kFileName;
    if (input.bytes) WriteBytes(file, *input.bytes);
    return file;
}

// v0.2.1 reads HeadTracking.ini beside the running executable, so the oracle's input goes there
// and is removed again afterwards.
bw_oracle_view::OracleConfig RunOracleOn(const Input& input, std::string* written = nullptr) {
    const fs::path file = bw_oracle_view::OraclePath();
    std::error_code ec;
    fs::remove(file, ec);
    if (fs::exists(file)) throw std::runtime_error("could not remove " + file.string());
    if (input.bytes) WriteBytes(file, *input.bytes);
    const bw_oracle_view::OracleConfig c = bw_oracle_view::RunOracle();
    if (written) *written = ReadBytes(file);
    fs::remove(file, ec);
    return c;
}

struct ImportRun {
    legacy::Config config;
    legacy::ReadStatus status = legacy::ReadStatus::Read;
};

// The import on a read-only copy of the input, which must leave its folder as it found it.
ImportRun RunImport(Scratch& scratch, const Input& input) {
    const fs::path dir = scratch.Clean("import");
    const fs::path file = Place(dir, input);
    if (input.bytes) SetReadOnly(file, true);
    const Listing before = List(dir);
    ImportRun run;
    run.status = legacy::Read(file.string().c_str(), run.config);
    Check(List(dir) == before, input.name + ": the import changed its folder");
    return run;
}

ImportRun Comparison1(Scratch& scratch, const Input& input) {
    const bw_oracle_view::OracleConfig oracle = RunOracleOn(input);
    const ImportRun import = RunImport(scratch, input);

    Check(input.bytes.has_value() == (import.status == legacy::ReadStatus::Read),
          input.name + ": the import's status does not say whether there was a file");
    const std::vector<std::string> fields = FieldDifferences(oracle, import.config);
    Check(fields.empty(), input.name + ": fields differ: " + Join(fields));
    Check(StartupOf(oracle) == StartupOf(import.config), input.name + ": startup state differs");
    const bw_oracle_view::FireTable oracleFires =
        bw_oracle_view::OracleFires(oracle.toggle_vk, oracle.yaw_mode_vk, oracle.mode_cycle_vk);
    const bw_oracle_view::FireTable importFires =
        bw_oracle_view::OracleFires(import.config.toggle_vk, import.config.yaw_mode_vk, import.config.mode_cycle_vk);
    Check(oracleFires == importFires,
          input.name + ": hotkeys fire differently: " + FirstFireDifference(oracleFires, importFires));
    return import;
}

// ---------------------------------------------------------------------------
// Comparison 2
// ---------------------------------------------------------------------------

namespace cfg = cameraunlock::config;
using cfg::ConfigLoadStatus;
using cfg::DropRule;
using cfg::DroppedValue;
using cfg::ImportResult;
using cfg::ImportStatus;

cameraunlock::input::KeyModifiers g_currentHeld = cameraunlock::input::KeyModifiers::kNone;

cameraunlock::input::KeyModifiers CurrentHeld() { return g_currentHeld; }

cameraunlock::input::KeyModifiers ModifiersOf(int held) {
    using cameraunlock::input::KeyModifiers;
    KeyModifiers m = KeyModifiers::kNone;
    if ((held & 1) != 0) m = m | KeyModifiers::kCtrl;
    if ((held & 2) != 0) m = m | KeyModifiers::kShift;
    if ((held & 4) != 0) m = m | KeyModifiers::kAlt;
    return m;
}

// OracleFires' table for the current build. Its HotkeyHandler::Start parses each key list and
// hands it to RegisterKeyBindings, which puts one detail::GuardKey callback per distinct key on
// the poller, holding that key's bindings in list order. The same callbacks are built here with
// the held modifiers read from the test rather than the keyboard, since the poller keeps its
// callbacks to itself. Actions in the order toggle, cycle, yaw mode, as OracleFires counts them.
bw_oracle_view::FireTable CurrentFires(const Config& m) {
    using bw_oracle_view::kFirstKey;
    using bw_oracle_view::kHeldStates;
    using bw_oracle_view::kLastKey;
    std::array<int, 3> fired{};
    std::vector<std::pair<int, std::function<void()>>> registered;
    const std::string* lists[3] = {&m.toggle_key_name, &m.cycle_tracking_mode_key_name, &m.yaw_mode_key_name};
    for (int action = 0; action < 3; ++action) {
        const cameraunlock::input::KeyBindingsParseResult parsed = cameraunlock::input::ParseKeyBindings(*lists[action]);
        if (!parsed.ok()) throw std::logic_error("migrated hotkey list '" + *lists[action] + "' does not parse");
        std::vector<int> keys;
        std::vector<std::vector<cameraunlock::input::KeyModifiers>> modifiers;
        for (const cameraunlock::input::KeyBinding& b : parsed.bindings) {
            const auto at = std::find(keys.begin(), keys.end(), b.vk);
            if (at == keys.end()) {
                keys.push_back(b.vk);
                modifiers.push_back({b.modifiers});
            } else {
                modifiers[static_cast<std::size_t>(at - keys.begin())].push_back(b.modifiers);
            }
        }
        for (std::size_t i = 0; i < keys.size(); ++i) {
            registered.emplace_back(keys[i], cameraunlock::input::detail::GuardKey(
                                                 std::move(modifiers[i]), [&fired, action] { ++fired[action]; },
                                                 &CurrentHeld));
        }
    }

    bw_oracle_view::FireTable table;
    table.reserve((kLastKey - kFirstKey + 1) * kHeldStates);
    for (int vk = kFirstKey; vk <= kLastKey; ++vk) {
        for (int held = 0; held < kHeldStates; ++held) {
            fired = {};
            g_currentHeld = ModifiersOf(held);
            for (const auto& r : registered) {
                if (r.first == vk) r.second();
            }
            table.push_back(fired);
        }
    }
    g_currentHeld = cameraunlock::input::KeyModifiers::kNone;
    return table;
}

// kFleetHotkeyRule applied to v0.2.1's codes, built independently of core: each action's code
// fires with Ctrl and Shift not both held, and its chord letter with both held, once per press.
// A code outside 0x01-0xFE is unbound (N1), and code 0 was unbound already.
bw_oracle_view::FireTable FleetRuleFires(const legacy::Config& l) {
    using bw_oracle_view::kFirstKey;
    using bw_oracle_view::kHeldStates;
    using bw_oracle_view::kLastKey;
    const int codes[3] = {l.toggle_vk, l.mode_cycle_vk, l.yaw_mode_vk};
    const int letters[3] = {'Y', 'G', 'H'};
    bw_oracle_view::FireTable table;
    table.reserve((kLastKey - kFirstKey + 1) * kHeldStates);
    for (int vk = kFirstKey; vk <= kLastKey; ++vk) {
        for (int held = 0; held < kHeldStates; ++held) {
            std::array<int, 3> fired{};
            for (int a = 0; a < 3; ++a) {
                const bool plain = codes[a] == vk && !CtrlShiftHeld(held);
                const bool chord = letters[a] == vk && CtrlShiftHeld(held);
                if (plain || chord) fired[a] = 1;
            }
            table.push_back(fired);
        }
    }
    return table;
}

// Where v0.2.1's table and the fleet rule's differ, the press was made with Ctrl and Shift both
// held, and nowhere else.
bool DiffersOnlyUnderCtrlShift(const bw_oracle_view::FireTable& before, const bw_oracle_view::FireTable& after) {
    if (before.size() != after.size()) return false;
    for (std::size_t i = 0; i < before.size(); ++i) {
        if (before[i] != after[i] && !CtrlShiftHeld(static_cast<int>(i % bw_oracle_view::kHeldStates))) return false;
    }
    return true;
}

// A file as the test holds it to: its bytes, its last write time and its attributes.
struct FileStamp {
    std::string bytes;
    FILETIME written{};
    DWORD attributes = 0;

    bool operator==(const FileStamp& o) const {
        return bytes == o.bytes && CompareFileTime(&written, &o.written) == 0 && attributes == o.attributes;
    }
};

FileStamp Stamp(const fs::path& path) {
    WIN32_FILE_ATTRIBUTE_DATA data;
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) {
        throw std::runtime_error("cannot stat " + path.string());
    }
    FileStamp s;
    s.bytes = ReadBytes(path);
    s.written = data.ftLastWriteTime;
    s.attributes = data.dwFileAttributes;
    return s;
}

// Where Defaults.ini is for each run of comparison 2: at the built-in values, which the first
// load creates, and with the values a player changed, written from it.
fs::path g_builtinDefaults;
fs::path g_alteredDefaults;

cfg::ConfigOwnerOptions<Config> OwnerOptions(const fs::path& dir, const fs::path& defaults) {
    return MakeConfigOwnerOptions(dir.wstring() + L"\\", cfg::DefaultsFile::At(defaults.wstring()));
}

// The import with its map, for the values it records.
ImportResult RunMappedImport(Scratch& scratch, const Input& input) {
    const fs::path file = Place(scratch.Clean("mapped"), input);
    Config out = MakeConfigTable().defaults();
    return MakeLegacyImport().run(cfg::LegacyInput{file.wstring(), file.string(), false}, out);
}

const DroppedValue* FindDrop(const std::vector<DroppedValue>& dropped, DropRule rule, const char* section,
                             const char* key) {
    for (const DroppedValue& d : dropped) {
        if (d.rule == rule && d.section == section && d.key == key) return &d;
    }
    return nullptr;
}

struct Tally {
    std::string committed;
    std::set<std::string> migrated;
    struct Run {
        int created = 0;
        int imported = 0;
        int deferred = 0;
        // Migrated files holding at least one default row.
        int with_default_rows = 0;
        // Migrated files that differ from the committed file.
        int with_values = 0;
    } builtin, altered;
    int with_pose_shaping_dropped = 0;
    int with_key_code_dropped = 0;
    int with_hotkey_rule_difference = 0;
};

bool OutOfKeyRange(int vk) { return vk != 0 && (vk < 0x01 || vk > 0xFE); }

// Every pose-shaping value the frozen reader read is listed in its place, folded where it holds
// the value v0.2.1 shipped and dropped as PoseShaping where it does not; a hotkey code outside
// 0x01-0xFE is dropped as KeyCodeOutOfRange; and nothing is dropped by any other rule.
void CheckDrops(const std::string& name, const legacy::Config& l, const ImportResult& imported, Tally& tally) {
    const legacy::Config shipped;
    struct Read {
        const char* section;
        const char* key;
        bool atShipped;
    };
    const Read reads[] = {
        {"Sensitivity", "Yaw", SameBits(l.sens_yaw, shipped.sens_yaw)},
        {"Sensitivity", "Pitch", SameBits(l.sens_pitch, shipped.sens_pitch)},
        {"Sensitivity", "Roll", SameBits(l.sens_roll, shipped.sens_roll)},
        {"Sensitivity", "InvertYaw", l.invert_yaw == shipped.invert_yaw},
        {"Sensitivity", "InvertPitch", l.invert_pitch == shipped.invert_pitch},
        {"Sensitivity", "InvertRoll", l.invert_roll == shipped.invert_roll},
        {"Deadzone", "Yaw", SameBits(l.deadzone_yaw, shipped.deadzone_yaw)},
        {"Deadzone", "Pitch", SameBits(l.deadzone_pitch, shipped.deadzone_pitch)},
        {"Deadzone", "Roll", SameBits(l.deadzone_roll, shipped.deadzone_roll)},
        {"Position", "WorldScale", SameBits(l.pos_world_scale, kWorldUnitsPerMetre)},
        {"Position", "SensX", SameBits(l.pos_sens_x, shipped.pos_sens_x)},
        {"Position", "SensY", SameBits(l.pos_sens_y, shipped.pos_sens_y)},
        {"Position", "SensZ", SameBits(l.pos_sens_z, shipped.pos_sens_z)},
        {"Position", "InvertX", l.pos_invert_x == shipped.pos_invert_x},
        {"Position", "InvertY", l.pos_invert_y == shipped.pos_invert_y},
        {"Position", "InvertZ", l.pos_invert_z == shipped.pos_invert_z},
    };
    Check(imported.pose_shaping.size() == std::size(reads),
          name + ": the import lists " + std::to_string(imported.pose_shaping.size()) + " pose-shaping values, not 16");
    if (imported.pose_shaping.size() != std::size(reads)) return;
    bool anyDropped = false;
    for (size_t k = 0; k < std::size(reads); ++k) {
        const cfg::PoseShapingValue& v = imported.pose_shaping[k];
        const std::string label = std::string("[") + reads[k].section + "] " + reads[k].key;
        Check(v.section == reads[k].section && v.key == reads[k].key, name + ": " + label + " is not listed in its place");
        Check(v.folded == reads[k].atShipped, name + ": " + label + " is " + (v.folded ? "folded" : "dropped") + " wrongly");
        const bool listed = FindDrop(imported.dropped, DropRule::PoseShaping, reads[k].section, reads[k].key) != nullptr;
        Check(listed != reads[k].atShipped,
              name + ": " + label + (listed ? " is dropped at its shipped value" : " is changed and not dropped"));
        if (!reads[k].atShipped) anyDropped = true;
    }
    if (anyDropped) ++tally.with_pose_shaping_dropped;

    const std::pair<const char*, int> codes[] = {{"Toggle", l.toggle_vk}, {"ModeCycle", l.mode_cycle_vk},
                                                 {"YawMode", l.yaw_mode_vk}};
    bool anyCode = false;
    for (const auto& [key, vk] : codes) {
        const bool listed = FindDrop(imported.dropped, DropRule::KeyCodeOutOfRange, "Hotkeys", key) != nullptr;
        Check(listed == OutOfKeyRange(vk), name + ": [Hotkeys] " + key + " dropped does not match its code");
        if (listed) anyCode = true;
    }
    if (anyCode) ++tally.with_key_code_dropped;

    for (const DroppedValue& d : imported.dropped) {
        Check(d.rule == DropRule::PoseShaping || d.rule == DropRule::KeyCodeOutOfRange,
              name + ": the import drops [" + d.section + "] " + d.key + " by a rule this map never applies");
    }
}

// The settings the mod starts on after the migration against the ones the frozen reader's build
// started on, with the approved changes applied: identity pose shaping (CheckDrops holds the
// import to recording every value it leaves out), and the hotkeys as kFleetHotkeyRule has them.
// v0.2.1 applied its one LimitY both ways.
std::vector<std::string> StartupDifferences(const legacy::Config& l, const Config& m, Tally& tally) {
    std::vector<std::string> d;
    if (m.enable_on_startup != l.enabled_on_startup) d.push_back("EnableOnStartup");
    if (m.udp_port != l.port) d.push_back("UdpPort");
    if (m.world_space_yaw != l.world_space_yaw) d.push_back("WorldSpaceYaw");
    const auto mode = cameraunlock::DecodeTrackingMode(m.rotation_enabled, m.position_enabled);
    if (!mode || *mode != (l.pos_enabled ? cameraunlock::TrackingMode::RotationAndPosition
                                         : cameraunlock::TrackingMode::RotationOnly)) {
        d.push_back("tracking mode");
    }
    if (!SameBits(m.local_smoothing, l.local_smoothing) || !SameBits(m.position.local_smoothing, l.local_smoothing)) {
        d.push_back("LocalSmoothing");
    }
    if (!SameBits(m.remote_smoothing, l.remote_smoothing) || !SameBits(m.position.remote_smoothing, l.remote_smoothing)) {
        d.push_back("RemoteSmoothing");
    }
    if (!SameBits(m.position.limit_x, l.pos_limit_x)) d.push_back("PositionLimitX");
    if (!SameBits(m.position.limit_y, l.pos_limit_y)) d.push_back("PositionLimitY");
    if (!SameBits(m.position.limit_y_down, l.pos_limit_y)) d.push_back("PositionLimitYDown");
    if (!SameBits(m.position.limit_z, l.pos_limit_z)) d.push_back("PositionLimitZ");
    if (!SameBits(m.position.limit_z_back, l.pos_limit_z_back)) d.push_back("PositionLimitZBack");
    const cameraunlock::PositionSettings identity;
    if (!SameBits(m.position.sensitivity_x, identity.sensitivity_x) ||
        !SameBits(m.position.sensitivity_y, identity.sensitivity_y) ||
        !SameBits(m.position.sensitivity_z, identity.sensitivity_z) || m.position.invert_x || m.position.invert_y ||
        m.position.invert_z) {
        d.push_back("position shaping");
    }
    if (!SameBits(m.pos_zoom_reference, l.pos_zoom_reference)) d.push_back("ZoomReference");
    if (!SameBits(m.pos_zoom_scale_max, l.pos_zoom_scale_max)) d.push_back("ZoomScaleMax");
    if (m.log_position_trace != l.log_position_trace) d.push_back("PositionTrace");

    const bw_oracle_view::FireTable before = bw_oracle_view::OracleFires(l.toggle_vk, l.yaw_mode_vk, l.mode_cycle_vk);
    const bw_oracle_view::FireTable rule = FleetRuleFires(l);
    const bw_oracle_view::FireTable after = CurrentFires(m);
    if (rule != after) d.push_back("hotkeys against the fleet rule: " + FirstFireDifference(rule, after));
    if (!DiffersOnlyUnderCtrlShift(before, rule)) d.push_back("hotkeys differ from v0.2.1 without Ctrl+Shift held");
    if (before != rule) ++tally.with_hotkey_rule_difference;
    return d;
}

bool Unrepresentable(const legacy::Config& l) {
    return l.pos_limit_x > kMaxCanonicalLimit || l.pos_limit_y > kMaxCanonicalLimit ||
           l.pos_limit_z > kMaxCanonicalLimit || l.pos_limit_z_back > kMaxCanonicalLimit;
}

// Every field the table binds, as the canonical renderer writes it, so two Configs compare whole.
std::string AllValues(const Config& c) {
    return cfg::RenderCanonical(MakeConfigTable(), c, {kConfigDisplayName});
}

bool Contains(const std::vector<std::string>& lines, const std::string& text) {
    for (const std::string& line : lines) {
        if (line.find(text) != std::string::npos) return true;
    }
    return false;
}

void Comparison2(Scratch& scratch, const Input& input, const ImportRun& import, const ImportResult* mapped,
                 const fs::path& defaults, Tally& tally) {
    const bool builtin = defaults == g_builtinDefaults;
    Tally::Run& run = builtin ? tally.builtin : tally.altered;
    const std::string name =
        input.name + (builtin ? " (Defaults.ini at the built-in values)" : " (Defaults.ini changed)");

    const fs::path dir = scratch.Clean("migration");
    const fs::path config = dir / kConfigFileName;
    const fs::path legacyFile = Place(dir, input);
    const FileStamp defaultsBefore = Stamp(defaults);
    FileStamp legacyBefore;
    if (input.bytes) legacyBefore = Stamp(legacyFile);

    const cfg::ConfigLoadResult<Config> loaded = cfg::ConfigOwner<Config>(OwnerOptions(dir, defaults)).Load();
    const Listing after = List(dir);
    Check(Stamp(defaults) == defaultsBefore, name + ": the load wrote Defaults.ini");
    if (input.bytes) {
        Check(Stamp(legacyFile) == legacyBefore, name + ": HeadTracking.ini did not keep its bytes, write time and attributes");
    }

    if (!input.bytes) {
        // A fresh install, which follows Defaults.ini.
        ++run.created;
        Check(loaded.status == ConfigLoadStatus::Created, name + ": no file is not Created");
        Check(after == Listing{{kConfigFileName, tally.committed}},
              name + ": the folder does not hold CameraUnlock.ini as config/HeadTracking.ini and nothing else");
        if (builtin) {
            const std::vector<std::string> d = StartupDifferences(import.config, loaded.config, tally);
            Check(d.empty(), name + ": comparison 2: " + Join(d));
        }
        return;
    }

    if (builtin) CheckDrops(name, import.config, *mapped, tally);

    // Imported or deferred, the session runs on the settings the load hands back.
    {
        const std::vector<std::string> d = StartupDifferences(import.config, loaded.config, tally);
        Check(d.empty(), name + ": comparison 2: " + Join(d));
    }

    if (Unrepresentable(import.config)) {
        ++run.deferred;
        Check(loaded.status == ConfigLoadStatus::Deferred,
              name + ": " + kUnrepresentable + ", but the load is " + cfg::ConfigLoadStatusName(loaded.status));
        Check(after == Listing{{kFileName, *input.bytes}}, name + ": a deferred import created CameraUnlock.ini or another file");
        Check(loaded.reason.find("cannot be converted") != std::string::npos,
              name + ": the player is not told which value stops the import: " + loaded.reason);
        return;
    }

    ++run.imported;
    Check(loaded.status == ConfigLoadStatus::Migrated,
          name + ": the migration is " + cfg::ConfigLoadStatusName(loaded.status) + ": " + loaded.reason);
    if (loaded.status != ConfigLoadStatus::Migrated) return;
    Check(after.size() == 2 && after[0].first == kConfigFileName && after[1].first == kFileName &&
              after[1].second == *input.bytes,
          name + ": the folder does not hold HeadTracking.ini and CameraUnlock.ini and nothing else");
    Check(Contains(loaded.log, "created from"), name + ": the log does not say where CameraUnlock.ini came from");
    const std::string migrated = ReadBytes(config);
    tally.migrated.insert(migrated);
    if (migrated.find("=default\r\n") != std::string::npos) ++run.with_default_rows;
    if (migrated != tally.committed) ++run.with_values;

    // The next launch reads CameraUnlock.ini over the same Defaults.ini, with nothing to report,
    // to the same settings, does not import, and writes neither file.
    {
        const cfg::ConfigLoadResult<Config> reread = cfg::ConfigOwner<Config>(OwnerOptions(dir, defaults)).Load();
        Check(reread.status == ConfigLoadStatus::Canonical && reread.diagnostics.empty(),
              name + ": the next launch does not read CameraUnlock.ini cleanly");
        Check(AllValues(reread.config) == AllValues(loaded.config), name + ": the next launch runs on other settings");
        Check(!Contains(reread.log, "created from"), name + ": the next launch imports again");
        Check(Contains(reread.log, "is left as it was and is not read"),
              name + ": the next launch does not say HeadTracking.ini is not read");
        Check(List(dir) == after && Stamp(legacyFile) == legacyBefore && Stamp(defaults) == defaultsBefore,
              name + ": the next launch changed a file");
    }

    // A read-only HeadTracking.ini imports as a writable one does and keeps its attribute, bytes
    // and write time.
    if (builtin) {
        const fs::path roDir = scratch.Clean("read-only");
        const fs::path roLegacy = Place(roDir, input);
        SetReadOnly(roLegacy, true);
        const FileStamp roBefore = Stamp(roLegacy);
        const cfg::ConfigLoadResult<Config> fromReadOnly = cfg::ConfigOwner<Config>(OwnerOptions(roDir, defaults)).Load();
        Check(fromReadOnly.status == ConfigLoadStatus::Migrated && AllValues(fromReadOnly.config) == AllValues(loaded.config) &&
                  ReadBytes(roDir / kConfigFileName) == migrated,
              name + ": a read-only HeadTracking.ini does not import as a writable one does");
        Check(Stamp(roLegacy) == roBefore && (roBefore.attributes & FILE_ATTRIBUTE_READONLY) != 0,
              name + ": a read-only HeadTracking.ini did not keep its attribute, bytes and write time");
    }
}

// Defaults.ini as a player may have changed it, from the one the owner created: every value this
// game takes from it differs from the built-in one, each set to the corpus's alternate for the
// legacy key it comes from, so a corpus input holding that alternate migrates as default.
void WriteAlteredDefaults() {
    std::string text = ReadBytes(g_builtinDefaults);
    const std::pair<const char*, const char*> changes[] = {
        {"UdpPort=4242", "UdpPort=4243"},
        {"EnableOnStartup=true", "EnableOnStartup=false"},
        {"WorldSpaceYaw=true", "WorldSpaceYaw=false"},
        {"PositionEnabled=true", "PositionEnabled=false"},
        {"LocalSmoothing=0.0", "LocalSmoothing=0.3"},
        {"RemoteSmoothing=0.15", "RemoteSmoothing=0.3"},
        {"PositionLimitX=0.3", "PositionLimitX=0.5"},
        {"PositionLimitY=0.2", "PositionLimitY=0.5"},
        {"PositionLimitYDown=0.2", "PositionLimitYDown=0.5"},
        {"PositionLimitZ=0.4", "PositionLimitZ=0.5"},
        {"PositionLimitZBack=0.1", "PositionLimitZBack=0.2"},
        {"ToggleKey=End, Ctrl+Shift+Y", "ToggleKey=F1, Ctrl+Shift+Y"},
        {"CycleTrackingModeKey=PageUp, Ctrl+Shift+G", "CycleTrackingModeKey=F2, Ctrl+Shift+G"},
        {"YawModeKey=PageDown, Ctrl+Shift+H", "YawModeKey=F3, Ctrl+Shift+H"},
    };
    for (const auto& [from, to] : changes) {
        const std::string line = std::string("\r\n") + from + "\r\n";
        const size_t at = text.find(line);
        if (at == std::string::npos) throw std::runtime_error(std::string("the created Defaults.ini has no line ") + from);
        text.replace(at + 2, std::strlen(from), to);
    }
    fs::create_directories(g_alteredDefaults.parent_path());
    WriteBytes(g_alteredDefaults, text);
}

std::string Data(const char* name) {
    const std::string bytes = ReadBytes(fs::path(BW_DIFFERENTIAL_DATA) / name);
    Check(!bytes.empty(), std::string("data/") + name + " is empty");
    return bytes;
}

std::vector<Input> Inputs() {
    using cameraunlock::config::testing::GenerateIniMutations;
    std::vector<Input> inputs;
    inputs.push_back({"no file", std::nullopt});
    inputs.push_back({"empty file", std::string()});
    for (const char* name : {"v0.1.x-first-run.ini", "v0.2.0-first-run.ini", "v0.2.1-first-run.ini",
                             "shipped-7fa312c.ini", "shipped-09b59c7.ini", "shipped-be7c8a6.ini",
                             "shipped-f2c72cc.ini"}) {
        inputs.push_back({name, Data(name)});
    }
    for (const char* base : {"v0.2.1-first-run.ini", "shipped-f2c72cc.ini"}) {
        for (auto& m : GenerateIniMutations(Data(base), legacy::ReadKeys(), MutationKeys())) {
            inputs.push_back({std::string("corpus over ") + base + ": " + m.name, std::move(m.bytes)});
        }
    }
    return inputs;
}

}  // namespace

int main() {
    try {
        Scratch scratch;
        Tally tally;
        tally.committed = ReadBytes(fs::path(BW_COMMITTED_CONFIG));
        Check(!tally.committed.empty(), "config/HeadTracking.ini is missing");

        // Each Defaults.ini sits outside the game folder, in a user folder of its own whose
        // parent exists, as the owner requires before it creates the file.
        g_builtinDefaults = scratch.Clean("user-builtin") / "CameraUnlock" / "Defaults.ini";
        g_alteredDefaults = scratch.Clean("user-altered") / "CameraUnlock" / "Defaults.ini";
        {
            const fs::path dir = scratch.Clean("first-load");
            Check(cfg::ConfigOwner<Config>(OwnerOptions(dir, g_builtinDefaults)).Load().status == ConfigLoadStatus::Created,
                  "the first load is not Created");
            Check(fs::exists(g_builtinDefaults), "the first load did not create Defaults.ini");
        }
        WriteAlteredDefaults();

        // v0.2.1's first-run output, committed once as test data, is what the oracle still
        // writes for a missing file.
        {
            std::string written;
            RunOracleOn({"no file", std::nullopt}, &written);
            Check(written == Data("v0.2.1-first-run.ini"),
                  "the oracle's first-run output differs from data/v0.2.1-first-run.ini");
        }

        // Fresh equals upgrade: over Defaults.ini at the built-in values, v0.2.1's first-run
        // output and the file it shipped each import into a CameraUnlock.ini that is the
        // committed file, which is what a fresh install creates.
        for (const char* name : {"v0.2.1-first-run.ini", "shipped-f2c72cc.ini"}) {
            const fs::path dir = scratch.Clean("fresh-equals-upgrade");
            WriteBytes(dir / kFileName, Data(name));
            Check(cfg::ConfigOwner<Config>(OwnerOptions(dir, g_builtinDefaults)).Load().status == ConfigLoadStatus::Migrated &&
                      ReadBytes(dir / kConfigFileName) == tally.committed,
                  std::string(name) + " does not import into the committed file");
        }

        const std::vector<Input> inputs = Inputs();
        std::printf("%zu inputs\n", inputs.size());
        std::printf("comparison 1, the oracle (v0.2.1) against the import:\n");
        for (const char* d : kComparison1Differences) std::printf("  recorded difference: %s\n", d);
        for (const Input& input : inputs) {
            const ImportRun import = Comparison1(scratch, input);
            std::optional<ImportResult> mapped;
            if (input.bytes) {
                mapped = RunMappedImport(scratch, input);
                Check(mapped->status == ImportStatus::Imported, input.name + ": the mapped import is not Imported");
            }
            for (const fs::path& defaults : {g_builtinDefaults, g_alteredDefaults}) {
                Comparison2(scratch, input, import, mapped ? &*mapped : nullptr, defaults, tally);
            }
        }

        std::printf("comparison 2, the import against the migration, %zu distinct files:\n", tally.migrated.size());
        for (const auto& [over, run] : {std::pair<const char*, const Tally::Run*>{"at the built-in values", &tally.builtin},
                                        std::pair<const char*, const Tally::Run*>{"changed", &tally.altered}}) {
            std::printf("  over Defaults.ini %s: %d created, %d imported (%d holding a default row, %d differing from "
                        "the committed file), %d deferred\n",
                        over, run->created, run->imported, run->with_default_rows, run->with_values, run->deferred);
            Check(run->deferred > 0, std::string("no input is deferred over ") + over);
            Check(run->with_default_rows > 0, std::string("no import writes default over ") + over);
            Check(run->with_values > 0, std::string("no import writes a value over ") + over);
        }
        std::printf("  %d with a changed sensitivity, inversion, deadzone or WorldScale dropped (pose_shaping)\n",
                    tally.with_pose_shaping_dropped);
        std::printf("  %d with a hotkey code outside 0x01-0xFE imported as unbound (N1)\n", tally.with_key_code_dropped);
        std::printf("  %d loads whose hotkeys differ from v0.2.1 under Ctrl+Shift: %s\n", tally.with_hotkey_rule_difference,
                    kFleetHotkeyRule);
        std::printf("  deferred: %s\n", kUnrepresentable);
        Check(tally.with_pose_shaping_dropped > 0, "no input drops a changed pose-shaping value");
        Check(tally.with_key_code_dropped > 0, "no input drops an out-of-range hotkey code");
        Check(tally.with_hotkey_rule_difference > 0, "no input shows the fleet hotkey rule");
        Check(tally.migrated.count(tally.committed) == 1, "no input migrated to the committed file");

        wchar_t exe[MAX_PATH];
        GetModuleFileNameW(nullptr, exe, MAX_PATH);
        const fs::path lintDir = fs::path(exe).parent_path() / "migrated";
        fs::remove_all(lintDir);
        fs::create_directories(lintDir);
        int n = 0;
        for (const std::string& file : tally.migrated) WriteBytes(lintDir / (std::to_string(n++) + ".ini"), file);
    } catch (const std::exception& e) {
        std::printf("  FAIL: threw: %s\n", e.what());
        ++g_failures;
    }

    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
