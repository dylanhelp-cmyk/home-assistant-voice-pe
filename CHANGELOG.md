# Changelog

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
