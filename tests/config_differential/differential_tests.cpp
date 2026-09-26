// The config differential test (convert-a-mod-to-the-canonical-config, section 5). Every input
// is read two ways:
//
//   oracle     the reader of v0.2.1, the newest published build, with the core sources it
//              compiled at its pin c480d8a (oracle_adapter.h)
//   import     the frozen reader in src/legacy_config/
//
// Comparison 1, oracle against import, on every input: load status, every field both read
// (floats bit for bit), the startup state, and which actions every key press fires under every
// set of held modifiers. The differences it may find are kComparison1Differences below.
//
// The import runs on a read-only copy of each input and must leave its folder as it found it.
//
// Inputs: no file, an empty file, the first-run output of every published build (v0.1.0,
// v0.1.2 and v0.1.4 wrote the same file), every committed version of config/HeadTracking.ini up
// to v0.2.1 (the installer and Nexus ZIPs carried it; no build seeded one through the launcher),
// and core's corpus over v0.2.1's first-run output and over the file v0.2.1 shipped.

#include "legacy_config/legacy_config.h"
#include "oracle_adapter.h"

#include "cameraunlock/config/testing/ini_mutations.h"

#include <windows.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;
using namespace headtracking;

namespace {

// v0.2.1 and this commit read the file with the same source (src/config.cpp is unchanged since
// the v0.2.1 tag) and the same core IniReader (unchanged since c480d8a), so comparison 1 has no
// differences to record.
const char* const kComparison1Differences[] = {
    "none",
};

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
        plain("Position", "LimitX", "0.5", {"-0.3"}),
        plain("Position", "LimitY", "0.5", {"-0.2"}),
        plain("Position", "LimitZ", "0.5", {"-0.4"}),
        plain("Position", "LimitZBack", "0.2", {"-0.1"}),
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

void Comparison1(Scratch& scratch, const Input& input) {
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

        // v0.2.1's first-run output, committed once as test data, is what the oracle still
        // writes for a missing file.
        {
            std::string written;
            RunOracleOn({"no file", std::nullopt}, &written);
            Check(written == Data("v0.2.1-first-run.ini"),
                  "the oracle's first-run output differs from data/v0.2.1-first-run.ini");
        }

        const std::vector<Input> inputs = Inputs();
        std::printf("%zu inputs\n", inputs.size());
        std::printf("comparison 1, the oracle (v0.2.1) against the import:\n");
        for (const char* d : kComparison1Differences) std::printf("  recorded difference: %s\n", d);
        for (const Input& input : inputs) Comparison1(scratch, input);
    } catch (const std::exception& e) {
        std::printf("  FAIL: threw: %s\n", e.what());
        ++g_failures;
    }

    std::printf("%d checks, %d failures\n", g_checks, g_failures);
    return g_failures == 0 ? 0 : 1;
}
