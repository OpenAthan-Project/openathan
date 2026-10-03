# Voice Pyramid Reference Build — Assembly

These instructions apply to the supported AtomS3R C126 + Voice Pyramid A167
reference build. See the [bill of materials](bom.md) and manufacturer instructions
for [AtomS3R](https://docs.m5stack.com/en/core/AtomS3R) and
[Voice Pyramid](https://docs.m5stack.com/en/atom/Voice_Pyramid).

No soldering or custom PCB assembly is required:

1. Attach the supported AtomS3R controller to the Voice Pyramid using the manufacturer's intended Atom interface.
2. Disconnect Pyramid bottom power. Connect the computer to the AtomS3R USB-C
   port with a data cable and use [USB setup](https://openathan.com/install/) in
   desktop Chrome or Edge. Use only this cable during USB setup or recovery.
3. After setup, unplug Atom USB and connect only the Pyramid's bottom USB-C
   power for normal operation. Wait for Wi-Fi and time synchronization, then
   open the reported device address on the same home network.

Follow the [complete setup instructions](https://openathan.com/docs/getting-started/)
for password creation, location, timetable review and activation. If OpenAthan is
already installed, use credential recovery or the
[preserving firmware update path](../../../docs/development/firmware-upgrades.md);
fresh installation erases saved data.
