# OpenAthan Device UI

Target local URL: `http://openathan.local`

This is intended to become the phone-friendly local configuration and playback UI served by the speaker itself.

**Status (2026-09-25): planning, not an implemented interface.** The initial
milestone is phone-friendly setup and ongoing prayer settings. Wi-Fi setup and
recovery are intended to use the installation USB connection, followed by a
handoff to this local interface. Manual location entry is acceptable initially.
Local access will use a device password chosen during USB setup, with password
recovery through USB. The protocol and first-run confirmation are still being
designed; these choices do not describe shipped behavior.

The [saved settings service](../../firmware/esphome/scheduler/SETTINGS.md) already
supports location/timezone, calculation/Asr/high-latitude methods, offsets,
enabled prayers and volume. It is currently exposed by optional encrypted
developer actions, not a browser HTTP API. A phone transport must retain full
snapshot revision checks, durable acknowledgments, application/fault reporting
and readback after an uncertain save. Wi-Fi changes, setup and recovery must
preserve prayer settings and consumed-prayer history.

The wider areas below remain later scope unless included in an approved plan;
they are not all requirements of the initial setup milestone.

Planned areas include:

- location and timezone;
- calculation method;
- Asr method;
- prayer offsets;
- per-prayer Athan controls;
- volume and LED settings;
- Quran/adhkar playback;
- update/status information.

The generic ESPHome web interface is not intended to be the permanent OpenAthan user experience.
