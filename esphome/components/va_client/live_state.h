#pragma once
#include <atomic>
#include <cstdint>
namespace esphome { namespace va_client {
// Protocol state is independent of LED phases. Only an explicit mic command
// can reopen the microphone; a late phase message cannot undo Stop or mute.
class LiveState {
 public:
  void hello(bool live) { this->end(); ready_.store(live); }
  bool begin() {
    if (!ready_.load()) return false;
    wake_id_.fetch_add(1);
    audio_.store(false); active_.store(true); mic_.store(true); return true;
  }
  void mic(bool enabled) { mic_.store(enabled && active_.load()); }
  void end() { active_.store(false); mic_.store(false); audio_.store(false); }
  void disconnected() { ready_.store(false); this->end(); }
  bool ready() const { return ready_.load(); }
  bool active() const { return active_.load(); }
  bool streaming() const { return mic_.load() && active_.load(); }
  bool accept_audio() { if (!active_.load()) return false; audio_.store(true); return true; }
  uint32_t id() const { return wake_id_.load(); }
  bool matches(uint32_t id) const { return id==wake_id_.load() && active_.load(); }
  bool has_audio() const { return audio_.load(); }
 private:
  std::atomic<uint32_t> wake_id_{0};
  std::atomic<bool> ready_{false}, active_{false}, mic_{false}, audio_{false};
};
}}  // namespace esphome::va_client
