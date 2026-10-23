#include "esphome/components/va_client/external_wake_gate.h"
#include <cassert>
#include <iostream>

using esphome::va_client::ExternalWakeGate;

int main() {
  ExternalWakeGate gate;
  auto first = gate.begin(false);
  assert(first != 0 && gate.pending(first));
  assert(!gate.complete(first + 1));  // wrong/late acknowledgements stay closed
  assert(gate.complete(first));
  assert(!gate.complete(first));  // a duplicate cannot open the mic twice
  auto stopped = gate.begin(false);
  gate.cancel();
  assert(!gate.complete(stopped));  // stop/mute/disconnect cancels readiness
  auto superseded = gate.begin(false);
  auto newest = gate.begin(true);
  assert(!gate.complete(superseded));
  assert(gate.complete(newest) && gate.followup());
  std::cout << "External wake gate: readiness, duplicate, stop, stale ID and follow-up checks passed\n";
}
