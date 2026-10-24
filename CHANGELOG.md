# Changelog

## 2.0.0 — GPT-Live protocol

- Open microphone capture at wake; no local/external wake chime and no fixed wake delay.
- Replace Realtime VAD/follow-up state handling with explicit backend microphone controls and session termination.
- Tag controls and PCM with a wake generation, ignoring stale controls/audio after Stop or another wake.
- Preserve microphone conversion, PSRAM speaker buffering, volume, cold-start prime, hardware mute and failure feedback.
- Keep `.realtime.yaml` as a compatibility include for `.live.yaml`, retaining Hey Jarvis overrides and device substitutions.
- Requires GPT-Live add-on 1.0.0. Builds do not install or flash hardware.


## 1.3.0

- Support external wake chimes with backend add-on 0.7.1+. Wake words and the
  center button use the same selected Home Assistant player as spoken replies.
- Keep the microphone closed until the remote chime and echo guard finish.
  Start the no-speech watchdog after microphone readiness, not during playback.
- Cancel stopped or superseded wakes and reject old/duplicate acknowledgements.
  Ignore stale phase updates while waiting for the external wake sound.
- Permit the existing stop-word guard during external replies, even though the
  Voice PE receives no reply PCM packets in that mode.
- Route requested follow-up chimes externally and preserve their timeout.
- Preserve local audio when selected in the add-on or used with an older backend.
- Add a host-compiled test for the wake acknowledgement/cancellation state.
