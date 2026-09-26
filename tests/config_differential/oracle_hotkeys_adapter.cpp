// Compiled into the hotkey oracle library only, with `cameraunlock` and `headtracking` renamed,
// so "hotkey_handler.h" here is v0.2.1's HotkeyHandler, the poller under it is oracle_fake's and
// so is the Plugin it drives.
#include "hotkey_handler.h"
#include "oracle_adapter.h"
#include "plugin.h"

namespace bw_oracle_view {

FireTable OracleFires(int toggle_vk, int yaw_mode_vk, int mode_cycle_vk) {
    namespace input = cameraunlock::input;
    headtracking::Plugin plugin;
    input::FakeRegistrations().clear();
    headtracking::HotkeyHandler hotkeys;
    hotkeys.Start(plugin, toggle_vk, yaw_mode_vk, mode_cycle_vk);
    const std::vector<input::FakeRegistration> registered = input::FakeRegistrations();
    hotkeys.Stop();

    // v0.2.1's poller: a callback runs when its nonzero key goes down.
    FireTable table;
    table.reserve((kLastKey - kFirstKey + 1) * kHeldStates);
    for (int vk = kFirstKey; vk <= kLastKey; ++vk) {
        for (int held = 0; held < kHeldStates; ++held) {
            plugin.fired = {};
            input::FakeHeld() = held;
            for (const input::FakeRegistration& r : registered) {
                if (r.vk == vk && r.callback) r.callback();
            }
            table.push_back(plugin.fired);
        }
    }
    input::FakeHeld() = 0;
    return table;
}

}  // namespace bw_oracle_view
