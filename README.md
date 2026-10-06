# CANable 2.0 Firmware (listen-only and quiet-LED fork)

This is a fork of [normaldotcom/canable2-fw](https://github.com/normaldotcom/canable2-fw), the slcan
firmware for the CANable 2.0. It implements non-standard slcan commands to support CANFD messaging
alongside a LAWICEL-style command set. Prebuilt binaries are on the
[Releases](https://github.com/JosephGarrone/canable2-fw/releases) page.

## What this fork changes

Based on upstream `b158aa7`, the build canable.io ships.

- **`M1` (silent mode) works.** Upstream stores the mode `M1` asks for, but `O` then opens the
  channel in normal mode every time, so `M1` has no effect. A host that probes for a bus's bitrate
  under `M1` is therefore on the bus in normal mode, and at a wrong bitrate the controller sends
  error frames. On a vehicle that sets a communication fault code (U0001 on a Ford PCM). Here `O`
  opens in the mode `M` chose, and in silent mode (FDCAN bus monitoring) the controller cannot
  transmit, acknowledge or send error flags. Frames sent while `M1` is selected are refused, so none
  waits to go out after a later `M0` and `O`.
- **`I0` turns the status LEDs off; `I1` turns them back on.** Nothing lights the blue or green LED
  while they are off: not activity, not errors, not the power-on blink. For an adapter inside an
  enclosure, where the flashes show through. A power LED wired straight to the supply on some boards
  is not under firmware control and stays lit.
- **It builds with GCC 14**, which rejects upstream's undeclared `snprintf_`.

The `V` reply names the release, for example `v1.0-listenonly github.com/JosephGarrone/canable2-fw.git`,
so this firmware can be told from upstream's (`b158aa7 github.com/normaldotcom/canable2.git`).

## Supported Commands

- `O` - Open channel 
- `C` - Close channel 
- `S0` - Set nominal bitrate to 10k
- `S1` - Set nominal bitrate to 20k
- `S2` - Set nominal bitrate to 50k
- `S3` - Set nominal bitrate to 100k
- `S4` - Set nominal bitrate to 125k
- `S5` - Set nominal bitrate to 250k
- `S6` - Set nominal bitrate to 500k
- `S7` - Set nominal bitrate to 750k
- `S8` - Set nominal bitrate to 1M
- `S9` - Set nominal bitrate to 83.3k
- `Y2` - Set data bitrate to 2M (CANFD only) (default)
- `Y5` - Set data bitrate to 5M (CANFD only)
- `M0` - Set mode to normal mode (default)
- `M1` - Set mode to silent mode (listen only: nothing is transmitted, not even ACKs)
- `A0` - Disable automatic retransmission 
- `A1` - Enable automatic retransmission (default)
- `tIIILDD...` - Transmit data frame (Standard ID) [ID, length, data]
- `TIIIIIIIILDD...` - Transmit data frame (Extended ID) [ID, length, data]
- `RIIIIIIIIL` - Transmit remote frame (Extended ID) [ID, length]
- `rIIIL` - Transmit remote frame (Standard ID) [ID, length]
- `dIIILDD...` - Transmit CAN FD standard ID (no BRS) [ID, length]
- `DIIIIIIIILDD...` - Transmit CAN FD extended ID (no BRS) [ID, length]
- `bIIILDD...` - Transmit CAN FD BRS standard ID [ID, length]
- `BIIIIIIIILDD...` - Transmit CAN FD extended ID [ID, length]

- `I0` - Turn the status LEDs off
- `I1` - Turn the status LEDs on (default)

- `V` - Returns firmware version and remote path as a string
- `E` - Returns error register

Note: CANFD message lengths are as follows (expressed in hexadecimal):
- `0-8`: Same as standard CAN
- `9`: Length = 12
- `A`: Length = 16
- `B`: Length = 20
- `C`: Length = 24
- `D`: Length = 32
- `E`: Length = 48
- `F`: Length = 64

Note: Channel configuration commands must be sent before opening the channel. The channel must be opened before transmitting frames.

This firmware currently does not provide any ACK/NACK feedback for serial commands.

## Building

You need GCC for Arm (`arm-none-eabi`), `make` and `git`. The releases are built with the
[Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads) 14.3.Rel1 on
Linux; your distribution's `gcc-arm-none-eabi` package works too. Add its `bin` folder to your
`PATH` and run `make`. The firmware lands in `build/canable2-<version>.bin`, where the version is
`git describe`, so build from a git checkout.

`make LEDS_QUIET=1` builds firmware that starts with the status LEDs off, as if `I0` had been sent
at power-up, so they stay dark before the host connects too.

## Flashing

The STM32's built-in USB bootloader takes the firmware; no programmer is needed. Flash one adapter
at a time.

1. **Enter the bootloader.** Unplug the adapter. On the MKS CANable v2.0, fit a jumper across the two
   pins marked BOOT; on the Openlight CANable 2.0, hold the BOOT button. Plug it in. It appears as
   USB device `0483:df11` (STM32 BOOTLOADER) with no serial port.
2. **Flash it** with [dfu-util](https://dfu-util.sourceforge.net/) 0.9 or later:
   ```
   dfu-util -d 0483:df11 -a 0 -s 0x08000000:leave -D canable2-v1.0-listenonly.bin
   ```
   On Linux run it with `sudo`, or `sudo apt install dfu-util` first. With several adapters in the
   bootloader at once, add `-p <usb path>` or `-S <serial>`; `dfu-util -l` lists them. It ends with
   `Download done.`; an `Error during download get_status` after that is normal once `:leave` has
   started the new firmware. On Windows, install the WinUSB driver for STM32 BOOTLOADER with
   [Zadig](https://zadig.akeo.ie/) once first. `make flash` does the same for a local build.
3. **Remove the jumper** (or release the button) and replug. With the jumper left on, the adapter
   starts in the bootloader at every power-up.
4. **Check it.** The USB product string names the firmware (on Linux, in `/dev/serial/by-id/`), or
   send `V` over the serial port:
   ```
   stty -F /dev/ttyACM0 raw -echo
   timeout 2 cat /dev/ttyACM0 & sleep 0.3; printf 'CV' > /dev/ttyACM0; wait
   ```

The [canable.io web updater](https://canable.io/updater/canable2.html) only flashes upstream builds,
so it cannot install this firmware, but it is the quickest way back to stock. Stock firmware is also
at `https://canable.io/builds/canable2/slcan/canable2-b158aa7.bin`, flashed as above.

## License

See LICENSE.md
