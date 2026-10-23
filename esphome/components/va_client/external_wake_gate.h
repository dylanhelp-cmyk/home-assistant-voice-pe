#pragma once

#include <cstdint>

namespace esphome {
namespace va_client {

// All access happens on the ESPHome main loop. A stop, reconnect, or newer
// wake invalidates the previous acknowledgement before it can open the mic.
class ExternalWakeGate {
 public:
  uint32_t begin(bool followup) {
    if (++this->id_ == 0)
      ++this->id_;
    this->pending_ = true;
    this->followup_ = followup;
    return this->id_;
  }
  void cancel() { this->pending_ = false; }
  bool pending(uint32_t id) const { return this->pending_ && this->id_ == id; }
  bool complete(uint32_t id) {
    if (!this->pending(id))
      return false;
    this->pending_ = false;
    return true;
  }
  uint32_t id() const { return this->id_; }
  bool followup() const { return this->followup_; }

 protected:
  uint32_t id_{0};
  bool pending_{false};
  bool followup_{false};
};

}  // namespace va_client
}  // namespace esphome
