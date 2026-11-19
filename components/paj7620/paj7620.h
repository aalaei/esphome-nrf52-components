#pragma once

#include "esphome/core/component.h"
#include "esphome/core/hal.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/components/event/event.h"

namespace esphome::paj7620 {

// PAJ7620U2 gesture sensor (Grove-Gesture v1.0 and compatible boards), ported from
// https://github.com/apaex/PAJ7620-ESPHome (Arduino Wire-based) to ESPHome's portable
// i2c::I2CDevice so it also builds on non-Arduino frameworks (e.g. Zephyr/nRF52).
// Exposed as an event::Event (not a text_sensor) since a gesture is a momentary
// occurrence, not a persistent state -- this maps directly onto Home Assistant's
// native Event entity instead of a state that lingers at the last gesture forever.
class PAJ7620Component : public event::Event, public PollingComponent, public i2c::I2CDevice {
 public:
  void setup() override;
  void loop() override;
  void dump_config() override;
  void update() override;

  void set_int_pin(GPIOPin *int_pin) { this->int_pin_ = int_pin; }

 protected:
  bool select_bank_(uint8_t bank);
  // Combined value: low byte is register 0x43 (directional/rotation flags),
  // high byte is register 0x44 (only bit 0 used, the "wave" flag).
  static const char *gesture_to_string_(uint16_t gesture);

  uint16_t last_state_{0};
  // Active-low GESTURE-interrupt output. Polled from loop() (not a true
  // hardware ISR -- I2C can't safely run from interrupt context anyway), so
  // when it's low we read the gesture registers immediately instead of
  // waiting for the next `update_interval` tick.
  GPIOPin *int_pin_{nullptr};
  bool last_int_level_{true};
  // Rate-limits interrupt-triggered reads: a floating/disconnected int_pin
  // has no defined level and can read low continuously, which without this
  // guard would call update() (an I2C transaction) on every single loop()
  // iteration -- easily thousands of times per second, hammering the bus and
  // never giving the sensor's wake-up handshake a chance to complete.
  uint32_t last_int_update_ms_{0};
  static constexpr uint32_t INT_UPDATE_MIN_INTERVAL_MS = 50;
};

}  // namespace esphome::paj7620
