# Home Assistant Voice PE — GPT-Live firmware 2.0.0

This fork now provides greeting-first GPT-Live firmware. Wake opens microphone streaming immediately and asks the backend to start a conversation. The backend streams your configured greeting; no wake chime or fixed wake delay is played.

Update the matching [GPT-Live add-on](https://github.com/dylanhelp-cmyk/ha-openai-realtime) to **1.0.0** and read its migration guide before flashing this firmware. The former `home-assistant-voice.realtime.yaml` package is a compatibility entry point to `home-assistant-voice.live.yaml`, so existing device substitutions and your Hey Jarvis `!extend alexa` override remain valid. Keep your working `va_url` and device port.

The backend controls gated/full-duplex capture, idle expiry and speaker routing. Stop, centre-button cancellation, hardware/software mute and disconnect close microphone capture locally. Wake-generation IDs on controls and PCM packets prevent stale messages/audio from reopening or contaminating a newer conversation. The persistent device WebSocket does not itself start a paid GPT-Live session.

Both code updates are prepared on `gpt-live-redesign` branches. Merge both pull requests before updating through the normal app/ESPHome interfaces; the package's component reference uses `main`. Nothing is installed or flashed by these repository changes.

The hardware configuration, LEDs, local volume, mute/error feedback and wake sensitivity are retained. Local wake-chime handling, Realtime turn/VAD gates, follow-up timers, dead microphone pre-roll, and external chime acknowledgements are removed from this component. Remote model access and real speaker latency/echo still need testing on the installation.

## Existing ESPHome device stub

Keep your existing configuration. A package reference to the old `.realtime.yaml` file will include the new package after merge. If you prefer the explicit new filename:

```yaml
packages:
  voice:
    url: https://github.com/dylanhelp-cmyk/home-assistant-voice-pe
    ref: main
    files: [home-assistant-voice.live.yaml]
    refresh: 0s

# Preserve your own working IP/port, API encryption key and Wi-Fi secrets.
substitutions:
  va_url: ws://192.168.1.56:18081/

micro_wake_word:
  models:
    - id: !extend alexa
      model: hey_jarvis
```

This is a package excerpt, not a replacement for your complete device YAML. The internal `alexa` ID is deliberately retained for the existing sensitivity control.

## Validation

Host state tests cover Stop/mute gating, disconnect, unsupported backend and stale wake IDs. ESPHome validation/source generation uses a local component override and the Hey Jarvis extension. Compile tests never upload to hardware.

The original stock hardware sources remain in the repository for provenance; the active custom package is `.live.yaml`.
