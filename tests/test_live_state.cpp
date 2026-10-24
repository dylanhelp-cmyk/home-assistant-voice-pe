#include <cassert>
#include "../esphome/components/va_client/live_state.h"
using esphome::va_client::LiveState;
int main() {
 LiveState s;
 assert(!s.begin());
 s.hello(true); assert(s.begin()); assert(s.streaming());
 s.mic(false); assert(!s.streaming()); assert(s.active());
 assert(s.accept_audio()); assert(s.has_audio());
 s.mic(true); assert(s.streaming());
 s.end(); s.mic(true); assert(!s.streaming()); assert(!s.accept_audio());
 uint32_t previous=s.id();
 assert(s.begin()); assert(!s.matches(previous)); assert(s.matches(s.id()));
 s.disconnected(); assert(!s.streaming()); assert(!s.begin());
 s.hello(false); assert(!s.begin());
}
