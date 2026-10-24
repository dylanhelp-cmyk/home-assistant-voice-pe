#pragma once
#include "esphome/core/automation.h"
#include "va_client.h"
namespace esphome { namespace va_client {
class OnPhaseTrigger : public Trigger<std::string> {
 public:
  explicit OnPhaseTrigger(VaClient *p) { p->add_on_phase_trigger(this); }
};
class OnRepeatedFailureTrigger : public Trigger<> {
 public:
  explicit OnRepeatedFailureTrigger(VaClient *p) { p->add_on_repeated_failure_trigger(this); }
};
}}
