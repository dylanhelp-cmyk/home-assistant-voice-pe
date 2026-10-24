#include "va_client.h"
#include "automation.h"
#include "esphome/core/log.h"
#include "esphome/components/audio/audio.h"
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <esp_websocket_client.h>
#include <esp_event.h>
#include <esp_heap_caps.h>
namespace esphome { namespace va_client {
static const char *const TAG="va_client";
static void ws_event(void *arg,esp_event_base_t,int32_t id,void *data) {
  if (arg) static_cast<VaClient*>(arg)->on_ws_event(id,data);
}
void VaClient::setup() {
  if (mic_) mic_->add_data_callback([this](const std::vector<uint8_t> &data){ microphone_(data); });
  buffer_=static_cast<uint8_t*>(heap_caps_malloc(CAPACITY,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
  if (!buffer_) { ESP_LOGE(TAG,"Audio buffer allocation failed"); this->mark_failed(); return; }
  if (speaker_) {
    speaker_->set_audio_stream_info(audio::AudioStreamInfo(16,1,24000));
    speaker_->start();
  }
  connect_();
}
void VaClient::connect_() {
  if (!handle_) {
    esp_websocket_client_config_t config={};
    config.uri=url_.c_str(); config.disable_auto_reconnect=true;
    auto handle=esp_websocket_client_init(&config);
    if (!handle) { reconnect_(); return; }
    handle_=handle;
    if (esp_websocket_register_events(handle,WEBSOCKET_EVENT_ANY,ws_event,this)!=ESP_OK) {
      ESP_LOGE(TAG,"Could not register WebSocket events"); this->mark_failed(); return;
    }
  }
  if (esp_websocket_client_start(static_cast<esp_websocket_client_handle_t>(handle_))!=ESP_OK) reconnect_();
}
void VaClient::fail_() { for (auto *trigger:failures_) trigger->trigger(); }
void VaClient::reconnect_() {
  if (reconnect_pending_) return;
  reconnect_pending_=true;
  if (++failures_count_==3) this->defer([this](){ fail_(); });
  uint32_t delay=retry_ms_; retry_ms_=std::min<uint32_t>(retry_ms_*2,10000);
  this->set_timeout("live_reconnect",delay,[this](){ reconnect_pending_=false; connect_(); });
}
void VaClient::send_(const char *text) {
  if (connected_.load() && handle_)
    esp_websocket_client_send_text(static_cast<esp_websocket_client_handle_t>(handle_),text,std::strlen(text),10/portTICK_PERIOD_MS);
}
void VaClient::on_ws_event(int32_t event,void *raw) {
  if (event==WEBSOCKET_EVENT_CONNECTED) {
    connected_.store(true); state_.disconnected();
    this->defer([this](){ retry_ms_=1000; failures_count_=0; clear_(); phase_("idle"); });
    send_("{\"type\":\"start\",\"protocol\":2}");
  } else if (event==WEBSOCKET_EVENT_DATA) {
    auto *data=static_cast<esp_websocket_event_data_t*>(raw);
    if (!data || !data->data_ptr || data->data_len<=0) return;
    if (data->op_code==0x01) binary_message_=false;
    else if (data->op_code==0x02) binary_message_=true;
    else if (data->op_code!=0x00) return;
    if (binary_message_) binary_(reinterpret_cast<const uint8_t*>(data->data_ptr),data->data_len,data->payload_offset==0);
    else {
      if (data->payload_offset==0) text_buffer_.clear();
      if (text_buffer_.size()+data->data_len>2048) { text_buffer_.clear(); return; }
      text_buffer_.append(data->data_ptr,data->data_len);
      if (data->payload_offset+data->data_len>=data->payload_len) text_(text_buffer_.data(),text_buffer_.size());
    }
  } else if (event==WEBSOCKET_EVENT_DISCONNECTED || event==WEBSOCKET_EVENT_CLOSED || event==WEBSOCKET_EVENT_ERROR) {
    connected_.store(false); state_.disconnected();
    this->defer([this](){ clear_(); phase_("idle"); reconnect_(); });
  }
}
void VaClient::phase_(const std::string &phase) {
  for (auto *trigger:phases_) trigger->trigger(phase);
}
void VaClient::text_(const char *data,size_t size) {
  std::string msg(data,size);
  if (msg.find("\"type\":\"hello\"")!=std::string::npos) {
    bool live=msg.find("\"live\":true")!=std::string::npos;
    bool external=msg.find("\"external_audio\":true")!=std::string::npos;
    auto key=msg.find("\"playback_prebuffer_ms\":");
    uint32_t prebuffer=100;
    if (key!=std::string::npos) prebuffer=std::min<unsigned long>(2000,std::strtoul(msg.c_str()+key+24,nullptr,10));
    this->defer([this,live,external,prebuffer](){
      state_.hello(live); external_.store(external); prebuffer_ms_=prebuffer; clear_(); phase_("idle");
      if (!live) { ESP_LOGW(TAG,"This firmware requires the GPT-Live backend"); fail_(); }
    });
  } else if ([&]() {
    auto key=msg.find("\"wake_id\":");
    return key==std::string::npos || !state_.matches(static_cast<uint32_t>(std::strtoul(msg.c_str()+key+10,nullptr,10)));
  }()) {
    return;  // old Stop/end/phase/mic controls cannot affect a newer wake
  } else if (msg.find("\"type\":\"mic\"")!=std::string::npos) {
    bool enabled=msg.find("\"enabled\":true")!=std::string::npos;
    state_.mic(enabled);
  } else if (msg.find("\"type\":\"session_end\"")!=std::string::npos) {
    uint32_t wake_id=state_.id(); state_.end();
    this->defer([this,wake_id](){ if (state_.id()==wake_id) { clear_(); phase_("idle"); } });
  } else if (msg.find("\"type\":\"error\"")!=std::string::npos) {
    uint32_t wake_id=state_.id(); state_.end();
    this->defer([this,wake_id](){ if (state_.id()==wake_id) { clear_(); phase_("idle"); fail_(); } });
  } else if (msg.find("\"type\":\"phase\"")!=std::string::npos) {
    for (const char *value:{"idle","listening","thinking","replying"}) {
      if (msg.find(std::string("\"value\":\"")+value+"\"")!=std::string::npos) {
        std::string phase(value);
        uint32_t wake_id=state_.id();
        this->defer([this,phase,wake_id](){ if (state_.id()==wake_id && (state_.active() || phase=="idle")) phase_(phase); });
        break;
      }
    }
  }
}
void VaClient::clear_() {
  portENTER_CRITICAL(&mutex_); head_=tail_=fill_=0; portEXIT_CRITICAL(&mutex_);
  priming_=false; silence_prime_=0; reset_fragment_.store(true);
  if (speaker_) speaker_->stop();
}
void VaClient::start_session() {
  if (!is_connected()) { fail_(); return; }
  if (state_.active()) send_interrupt();
  clear_();
  if (!state_.begin()) return;
  char wake[96];
  std::snprintf(wake,sizeof(wake),"{\"type\":\"wake\",\"protocol\":2,\"wake_id\":%u}",static_cast<unsigned>(state_.id()));
  send_(wake);
  phase_("listening");
}
void VaClient::send_interrupt() {
  state_.end(); clear_(); send_("{\"type\":\"interrupt\"}"); phase_("idle");
}
void VaClient::microphone_(const std::vector<uint8_t> &data) {
  if (!state_.streaming() || !connected_.load() || !handle_ || data.size()<8) return;
  const auto *samples=reinterpret_cast<const int32_t*>(data.data());
  size_t count=data.size()/8;
  mic_pcm_.resize(count);
  for (size_t i=0;i<count;++i) mic_pcm_[i]=static_cast<int16_t>(samples[i*2+(mic_channel_&1)]>>16);
  esp_websocket_client_send_bin(static_cast<esp_websocket_client_handle_t>(handle_),
    reinterpret_cast<const char*>(mic_pcm_.data()),count*2,10/portTICK_PERIOD_MS);
}
void VaClient::binary_(const uint8_t *data,size_t size,bool first) {
  if (first) { packet_header_.clear(); packet_current_=false; }
  while (packet_header_.size()<4 && size) { packet_header_.push_back(*data++); --size; }
  if (packet_header_.size()<4) return;
  uint32_t id=static_cast<uint32_t>(packet_header_[0]) | (static_cast<uint32_t>(packet_header_[1])<<8) |
    (static_cast<uint32_t>(packet_header_[2])<<16) | (static_cast<uint32_t>(packet_header_[3])<<24);
  packet_current_=state_.matches(id);
  if (!buffer_ || !packet_current_ || !state_.accept_audio()) return;
  if (reset_fragment_.exchange(false)) fragment_.clear();
  fragment_.insert(fragment_.end(),data,data+size);
  size_t count=fragment_.size()/2;
  float volume=std::max(0.0f,std::min(1.0f,volume_.load()));
  int32_t scale=static_cast<int32_t>(volume*32768);
  portENTER_CRITICAL(&mutex_);
  size_t accepted=std::min(count,(CAPACITY-fill_)/2);
  bool empty=fill_==0;
  for (size_t i=0;i<accepted;++i) {
    int16_t sample=static_cast<int16_t>(fragment_[i*2]|(static_cast<uint16_t>(fragment_[i*2+1])<<8));
    int16_t scaled=static_cast<int16_t>((static_cast<int32_t>(sample)*scale)>>15);
    buffer_[tail_]=static_cast<uint8_t>(scaled); tail_=(tail_+1)%CAPACITY;
    buffer_[tail_]=static_cast<uint8_t>(static_cast<uint16_t>(scaled)>>8); tail_=(tail_+1)%CAPACITY;
  }
  fill_+=accepted*2;
  portEXIT_CRITICAL(&mutex_);
  fragment_.erase(fragment_.begin(),fragment_.begin()+count*2);
  if (accepted<count) ESP_LOGW(TAG,"Output overflow; dropped %u samples",static_cast<unsigned>(count-accepted));
  if (empty && prebuffer_ms_ && !priming_) { prime_started_=millis(); priming_=true; }
}
void VaClient::loop() {
  if (!speaker_ || !buffer_) return;
  size_t fill,head,tail;
  portENTER_CRITICAL(&mutex_); fill=fill_; head=head_; tail=tail_; portEXIT_CRITICAL(&mutex_);
  if (!fill) return;
  if (priming_) {
    if (fill<prebuffer_ms_*48 && millis()-prime_started_<prebuffer_ms_) return;
    priming_=false;
  }
  if (speaker_->is_stopped()) speaker_->start();
  // Preserve the proven resampler cold-start prime without any wake chime.
  if (!silence_prime_ && millis()-last_fed_>500) silence_prime_=60*48;
  if (silence_prime_) {
    static const uint8_t silence[960]={0};
    size_t sent=speaker_->play(silence,std::min(silence_prime_,sizeof(silence)));
    silence_prime_-=sent; if (sent) last_fed_=millis(); return;
  }
  size_t contiguous=std::min(fill,head<tail?tail-head:CAPACITY-head);
  size_t sent=speaker_->play(buffer_+head,contiguous);
  if (sent) {
    last_fed_=millis();
    portENTER_CRITICAL(&mutex_); head_=(head_+sent)%CAPACITY; fill_-=sent; portEXIT_CRITICAL(&mutex_);
  }
}
}}
