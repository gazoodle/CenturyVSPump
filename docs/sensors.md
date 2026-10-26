# Additional sensors

The pump exposes far more than RPM and demand. This page collects the
registers that have been read successfully on a Gen3 motor, contributed by
[@stevemurphymsu](https://github.com/stevemurphymsu).

Addresses come from the
[Regal Modbus v4.17 protocol](Gen3%20EPC%20Modbus%20Communication%20Protocol%20_Rev4.17.pdf)
in this directory. Scale values are the divisor applied to the raw reading.

## Polling budget — read this first

Every sensor costs one Modbus round trip per update cycle. Century function
codes are in the user-defined range, so the ESPHome modbus hub allows only one
frame in flight per device address at a time. In practice each command takes
roughly 600–650 ms end to end.

That means a rough budget of:

    cycle time ≈ (number of sensors + 2) × 0.65 seconds

The `+ 2` covers the status poll and the demand number, which are always
present.

If `update_interval` is shorter than that, the queue never drains. New commands
pile on top of unsent ones, the hub starts refusing frames, and the log fills
with `Frame already active` and dropped commands. Symptomatically this looks
like the component working fine for a while and then degrading.

Some worked examples:

| Sensors | Minimum sane update_interval |
|---|---|
| 3 | 10s (the default) |
| 10 | 30s |
| 20 | 60s |
| 35 | 120s |

Set it on the `centuryvspump:` block:

```yaml
centuryvspump:
  update_interval: 120s
```

Most of these registers are diagnostics. Lifetime hour counters and fault
totals do not need ten-second resolution — if you want fast RPM feedback plus
slow diagnostics, that is a good reason to keep the sensor list short and read
the rest on demand.

## Page 0 — live operating data

| Name | Address | Scale | Unit |
|---|---|---|---|
| Motor Operating Mode | 2 | | |
| Motor Demand | 3 | 4 | rpm |
| Motor Torque | 4 | | ft-lb |
| Input Power | 5 | | W |
| DC Bus Voltage | 6 | 64 | V |
| Ambient Temperature | 7 | 128 | °C |
| Status | 8 | | |
| Previous Fault | 9 | | |
| Output Power | 0xa | | W |
| SVRS Bypass Status | 0xb | | |
| Current Faults | 0xc | | |
| Motor Line Voltage | 0xd | 10 | V |
| Ramp Status | 0xe | | |
| Total Faults | 0xf | | |
| Prime Status | 0x10 | | |
| Motor Input Power | 0x11 | | W |
| IGBT Temperature | 0x12 | 128 | °C |
| PCB Temperature | 0x13 | 128 | °C |

## Page 1 — configuration and counters

| Name | Address | Unit |
|---|---|---|
| Prime Detection Timer | 2 | |
| Prime Verify Timer | 3 | |
| Entrapment Detection | 5 | |
| SVRS Bypass Timer | 6 | |
| Serial Timeout Counter | 7 | |
| Total Run Time Low | 8 | Hours |
| Total Run Time High | 9 | Hours |
| Total Lifetime Low | 0xa | Hours |
| Total Lifetime High | 0xb | Hours |
| Total Session Timer Low | 0xd | Hours |
| Fault Timer Low | 0xe | |
| Fault Timer High | 0xf | |

### Unverified addresses

Three page 1 entries need checking against the protocol document before use:

- **SVRS Functionality** was contributed as address 3, which collides with
  Prime Verify Timer. Address 4 is unassigned and is the likely value.
- **Total Session Timer High** was contributed as address 0xd, the same as
  Total Session Timer Low. The natural next address, 0xe, is taken by Fault
  Timer Low.
- **Address 0xc** is skipped entirely between 0xb and 0xd.

If you confirm any of these against a real motor, please open an issue or PR.

## Example

```yaml
centuryvspump:
  id: pool_pump
  update_interval: 60s

sensor:
  - platform: centuryvspump
    name: "Input Power"
    type: custom
    address: 5
    page: 0
    unit_of_measurement: "W"

  - platform: centuryvspump
    name: "IGBT Temperature"
    type: custom
    address: 0x12
    page: 0
    scale: 128
    unit_of_measurement: "°C"
```

The `Total Run Time` and `Total Lifetime` values are split across low and high
16-bit registers. Combining them into a single value requires a template
sensor; the raw halves are exposed here as-is.
