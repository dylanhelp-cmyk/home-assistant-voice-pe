#pragma once
#include "live_state.h"
#include "esphome/core/component.h"
#include "esphome/components/microphone/microphone.h"
#include "esphome/components/speaker/speaker.h"
#include <atomic>
#include <cstdint>
#include <string>
#include <vector>
#include <freertos/FreeRTOS.h>
#include <freertos/portmacro.h>
namespace esphome { namespace va_client {
class OnPhaseTrigger;
class OnRepeatedFailureTrigger;
class VaClient : public Component {
 public:
  void set_url(const std::string &v) { url_=v; }
  void set_microphone(microphone::Microphone *v) { mic_=v; }
  void set_speaker(speaker::Speaker *v) { speaker_=v; }
  void set_mic_channel(uint8_t v) { mic_channel_=v; }
  void set_volume(float v) { volume_.store(v); }
  void add_on_phase_trigger(OnPhaseTrigger *v) { phases_.push_back(v); }
  void add_on_repeated_failure_trigger(OnRepeatedFailureTrigger *v) { failures_.push_back(v); }
  bool is_connected() const { return connected_.load() && state_.ready(); }
  bool is_external_audio() const { return external_.load(); }
  bool turn_has_reply_audio() const { return state_.has_audio(); }
  void setup() override;
  void loop() override;
  float get_setup_priority() const override { return setup_priority::AFTER_WIFI; }
  void start_session();
  void send_interrupt();
  void on_ws_event(int32_t event,void *data);
 protected:
  void connect_();
  void reconnect_();
  void text_(const char *data,size_t size);
  void binary_(const uint8_t *data,size_t size,bool first);
  void microphone_(const std::vector<uint8_t> &samples);
  void phase_(const std::string &phase);
  void clear_();
  void send_(const char *text);
  void fail_();
  std::string url_, text_buffer_;
  microphone::Microphone *mic_{nullptr};
  speaker::Speaker *speaker_{nullptr};
  uint8_t mic_channel_{0};
  void *handle_{nullptr};
  LiveState state_;
  std::atomic<bool> connected_{false}, external_{false}, reset_fragment_{true};
  std::atomic<float> volume_{1.0f};
  std::vector<OnPhaseTrigger*> phases_;
  std::vector<OnRepeatedFailureTrigger*> failures_;
  std::vector<int16_t> mic_pcm_;
  std::vector<uint8_t> fragment_, packet_header_;
  bool packet_current_{false};
  bool binary_message_{false}, reconnect_pending_{false};
  uint32_t retry_ms_{1000}, failures_count_{0}, last_fed_{0};
  std::atomic<uint32_t> prebuffer_ms_{100}, prime_started_{0};
  std::atomic<bool> priming_{false};
  size_t silence_prime_{0};
  static constexpr size_t CAPACITY=512*1024;
  uint8_t *buffer_{nullptr};
  size_t head_{0}, tail_{0}, fill_{0};
  portMUX_TYPE mutex_=portMUX_INITIALIZER_UNLOCKED;
};
}}
