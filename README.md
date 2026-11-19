# esphome-nrf52-components

External ESPHome components for nRF52840 (Zephyr) boards:

- `paj7620`: PAJ7620U2 gesture sensor, exposed as a Home Assistant event entity.
- `i2c`: a copy of ESPHome's `i2c` component with `bias-pull-up` added to the default
  pinctrl group. Without it, SDA/SCL float on boards that rely on the bus pull-ups being
  enabled internally.

## Usage

```yaml
external_components:
  - source: github://aalaei/esphome-nrf52-components
    components: [paj7620, i2c]
    refresh: 1h

i2c:
  sda: P1.13
  scl: P1.01

event:
  - platform: paj7620
    name: "Gesture"
    int_pin: P1.04   # optional, the sensor's INT output
```

See [example.yaml](example.yaml) for a complete minimal config.

## Licence

- `components/i2c/`: ESPHome, GPLv3 for C++ and MIT for Python, as in [LICENSE](LICENSE).
- `components/paj7620/`: derived from the MIT-licensed Seeed Technology Grove-Gesture
  library, via [apaex/PAJ7620-ESPHome](https://github.com/apaex/PAJ7620-ESPHome). The original
  MIT notice is kept in the source headers. See [NOTICE](NOTICE).
