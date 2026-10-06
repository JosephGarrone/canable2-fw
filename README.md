# CANable 2.0 Firmware (listen-only and quiet-LED fork)

[![Build](https://github.com/JosephGarrone/canable2-fw/actions/workflows/build.yml/badge.svg)](https://github.com/JosephGarrone/canable2-fw/actions/workflows/build.yml)

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

You do not need to build it to flash it: every push is built by
[GitHub Actions](https://github.com/JosephGarrone/canable2-fw/actions/workflows/build.yml), and
every tag becomes a [release](https://github.com/JosephGarrone/canable2-fw/releases) with the
binaries attached.

To build it yourself you need GCC for Arm (`arm-none-eabi`), `make` and `git`. The releases are
built with the [Arm GNU Toolchain](https://developer.arm.com/downloads/-/arm-gnu-toolchain-downloads)
14.3.Rel1 on Linux; your distribution's `gcc-arm-none-eabi` package works too. Add its `bin` folder
to your `PATH` and run `make`. The firmware lands in `build/canable2-<version>.bin`, where the
version is `git describe`, so build from a git checkout. `make LEDS_QUIET=1` builds the quiet variant.

## Flashing

The CANable 2.0's STM32 has a USB bootloader built in, so all you need is a USB cable and
[dfu-util](https://dfu-util.sourceforge.net/). Nothing is lost if it goes wrong: the bootloader is
in ROM and cannot be overwritten, so you can always flash again.

### 1. Download a build

From the [latest release](https://github.com/JosephGarrone/canable2-fw/releases/latest), take one of:

| File | Status LEDs at power-up |
|---|---|
| `canable2-<version>.bin` | On, as upstream (blue and green show power, bus activity and errors) |
| `canable2-<version>-quiet.bin` | Off (send `I1` to turn them on) |

Both are the same firmware otherwise. `SHA256SUMS` beside them lets you check the download. A build
of any commit that is not released yet is on that commit's
[Actions run](https://github.com/JosephGarrone/canable2-fw/actions/workflows/build.yml), under
Artifacts (you need to be signed in to GitHub to download it).

### 2. Install dfu-util

- **Linux / Raspberry Pi:** `sudo apt install dfu-util`
- **macOS:** `brew install dfu-util`
- **Windows:** download the latest `dfu-util-*-binaries` from the
  [releases page](https://dfu-util.sourceforge.net/releases/) and unzip it. Windows also needs the
  WinUSB driver for the bootloader, once: after step 3, run [Zadig](https://zadig.akeo.ie/),
  choose Options → List All Devices, pick **STM32 BOOTLOADER**, select **WinUSB** and click
  Install Driver.

### 3. Put the CANable into its bootloader

Unplug the CANable's USB. Then:

- **MKS / Makerbase CANable v2.0:** fit a jumper across the two pins marked **BOOT**.
- **Openlight Labs CANable 2.0:** hold down the **BOOT** button.

Plug the USB back in (and let go of the button). It now appears as `STM32 BOOTLOADER`, USB id
`0483:df11`, with no serial port: `lsusb` on Linux, Device Manager on Windows. The CAN wires can stay
connected; nothing is sent on them.

### 4. Flash it

From the folder holding the `.bin` (on Windows, the dfu-util folder, as `dfu-util.exe`):

```
dfu-util -d 0483:df11 -a 0 -s 0x08000000:leave -D canable2-<version>.bin
```

On Linux, run it with `sudo`. It erases, then writes, then ends with `Download done.` and
`File downloaded successfully`. An `Error during download get_status` after that is normal: it
means the CANable has already left the bootloader and started the new firmware.

With more than one CANable in the bootloader at once, `dfu-util -l` lists them; add
`-p <path>` or `-S <serial>` from that list to pick one.

### 5. Back to normal

**Take the jumper off** (MKS), then unplug and replug the USB. With the jumper left on, the CANable
starts in the bootloader at every power-up and no serial port appears.

### 6. Check it

The USB product string names the firmware: `CANable2 <version> github.com/JosephGarrone/canable2-fw.git`.
On Linux, `ls /dev/serial/by-id/` shows it. Or ask the CANable itself over its serial port with
`V`:

```
stty -F /dev/ttyACM0 raw -echo
timeout 2 cat /dev/ttyACM0 & sleep 0.3; printf 'CV' > /dev/ttyACM0; wait
```

On Windows, any serial terminal on its COM port works: type `V` and Enter.

### Going back to stock

Flash upstream's build the same way:
`https://canable.io/builds/canable2/slcan/canable2-b158aa7.bin`. The
[canable.io web updater](https://canable.io/updater/canable2.html) (Chrome) also does it in a few
clicks, but it only offers upstream builds, so it cannot install this one.

## License

See LICENSE.md
