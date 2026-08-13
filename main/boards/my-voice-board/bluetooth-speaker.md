# Bluetooth Speaker Output (External BT Receiver Module)

Route audio output from my-voice-board to a Bluetooth speaker without modifying
any firmware. The on-board MAX98357A is reused as a line driver: ESP32-S3 still
outputs I2S as usual, the MAX98357A converts it to analog audio, and a small
external Bluetooth receiver module re-broadcasts that analog audio to a paired
Bluetooth speaker over A2DP.

## Why this approach

The on-board ESP32-S3 only supports Bluetooth **LE**. A2DP (the profile that
streams audio to a Bluetooth speaker) requires **Classic Bluetooth**, which is
not available on any current ESP32-S3 / C-series chip. Adding it in firmware
is not possible.

A practical workaround is to add an external Bluetooth receiver module that
takes **analog** audio input. The module's only job is to take the line-level
signal coming out of the MAX98357A and forward it to a paired BT speaker.

### Signal chain

```
ESP32-S3  ──I2S──>  MAX98357A  ──analog──>  BT receiver  ──A2DP──>  BT speaker
   (on-board)      (on-board)      audio       (new)
```

Software does not change. The ESP32-S3 keeps writing I2S data to the same
GPIOs (BCLK=15, LRC=16, DOUT=7). The MAX98357A is still powered and still
produces analog audio on its output pads; we just tap that signal.

## Bill of materials

| Item | Notes |
|------|-------|
| Bluetooth audio receiver module (A2DP sink, analog input) | See "Module selection" below |
| 3–4 wires, ~10 cm | Power, GND, L, R |
| (Optional) 3.5 mm AUX pigtail | Only if the module uses a 3.5 mm jack instead of L/R pads |

No firmware changes. No new components on the board.

## Module selection

Any A2DP-sink Bluetooth module with an **analog line-level input** works.
Search Taobao for any of:

- `蓝牙音频接收板 模拟输入`
- `蓝牙接收模块 AUX`
- `XY-WRBT 模块`
- `BK8000L 蓝牙模块`
- `CSR8630 蓝牙接收模块`
- `汽车蓝牙接收器` (then open it up and use the LINE IN pads)

### Power and input voltage

| Module | Supply | Analog input |
|--------|--------|--------------|
| XY-WRBT | 3.3 V – 5 V (on-board LDO) | L / R pads or 3.5 mm jack |
| BK8000L | 3.3 V – 5 V (on-board LDO) | L / R pads or 3.5 mm jack |
| CSR8630 board | 3.3 V | L / R pads |
| AC692N receiver | 5 V | L / R pads |

Pick a module that is happy with whatever rail is convenient on your board
(5 V or 3.3 V — see the next section).

## Wiring

```
ESP32-S3 board                      BT receiver module
─────────────                       ──────────────────
5 V header  (or VIN)  ────────────▶ VCC   (5 V modules)
3V3 header            ────────────▶ VCC   (3.3 V modules)
                                      pick one of the two — never both
any GND               ────────────▶ GND      ← mandatory
                  (no I2S / I2C / data lines needed)

MAX98357A output pads   ──────────▶ L (left)
(or speaker + / − pads) ──────────▶ R (right)
```

Three (or four) wires total: **VCC, GND, L, R**. No I2S or data lines.

### Where to get 5 V / 3.3 V on the board

Most ESP32-S3 dev boards expose a 5 V pin (the USB rail before the LDO) and
a 3V3 pin (after the LDO). On a board powered only over USB, both are
available as long as the USB cable is plugged in. Pick whichever voltage
your BT module wants.

### Where to get GND

Any GND header pin works. Pick the closest one to where the BT module
sits to keep the wire short.

### Where to get the analog audio

The MAX98357A on the board is wired to a speaker connector (usually
labelled `SPK+` / `SPK−` or similar). Solder two wires to those pads:

- `SPK+` → BT module `L`
- `SPK−` → BT module `R`

The MAX98357A outputs a BTL (bridge-tied load) differential signal. Most
cheap BT modules accept a single-ended input on each channel; if you hear
a loud 50 Hz hum, see the troubleshooting section below.

The on-board speaker (if any) can stay connected or be removed — it does
not interfere with the BT module's input.

## Assembly checklist

1. Solder VCC and GND between ESP32 board and BT module.
2. Solder L and R from MAX98357A output pads to BT module input.
3. Power on. The BT module LED should start blinking (pairing mode).
4. Pair your Bluetooth speaker. The module LED usually stays solid once
   paired.
5. On the ESP32-S3, trigger any audio output (e.g. boot welcome sound)
   and confirm it is heard from the BT speaker.

## Troubleshooting

**No sound on BT speaker but module pairs fine.**
- Check L / R wiring — easy to swap.
- Verify the MAX98357A is actually outputting audio: temporarily connect
  a regular speaker to its output pads. If you hear sound there, the
  issue is the analog wiring to the BT module.

**Loud 50 Hz / 100 Hz hum.**
- GND not shared. Make sure ESP32 GND and BT module GND are connected
  with a short, thick wire.
- The MAX98357A is BTL differential. If your BT module is single-ended
  and refuses to accept the differential signal cleanly, you can either
  (a) use a 100 µF DC-blocking cap in series with each L / R line, or
  (b) drive the BT module from a different source (e.g. an external DAC).

**Audio plays on BT speaker with a few hundred ms delay.**
- This is normal for A2DP. The protocol adds ~150–300 ms of buffering.
  Don't use BT output for round-trip latency-sensitive testing.

**Audio is distorted or quiet.**
- The MAX98357A is set up for 8 Ω speakers. Feeding its output directly
  into a line-level input on a BT module can over-drive the input. If
  distortion is bad, add a simple resistor divider (e.g. 10 kΩ + 10 kΩ)
  on each channel to drop the level by 6 dB.

**BT module does not power up.**
- Voltage mismatch. Re-check the module's spec sheet — CSR8630 boards
  are strictly 3.3 V and will be damaged by 5 V.

## Reversibility

This modification is fully reversible. Remove the 3–4 wires and the
board is back to its original wired-speaker configuration. No components
are desoldered.

## See also

- `README.md` — base board pin assignments and build instructions
