#include "paj7620.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"

namespace esphome::paj7620 {

static const char *const TAG = "paj7620";

static constexpr uint8_t REG_BANK_SEL = 0xEF;
// GES_FLAG_0 (0x43) and GES_FLAG_1 (0x44) are adjacent, read together as one
// 2-byte transaction: low byte = directional/rotation flags, high byte's
// bit 0 = "wave" (repeated back-and-forth motion, not itself directional).
static constexpr uint8_t REG_GES_FLAG_0 = 0x43;
static constexpr uint8_t BANK0 = 0x00;
static constexpr uint8_t BANK1 = 0x01;

enum GestureFlag : uint16_t {
  GES_RIGHT_FLAG = 1 << 0,
  GES_LEFT_FLAG = 1 << 1,
  GES_UP_FLAG = 1 << 2,
  GES_DOWN_FLAG = 1 << 3,
  GES_FORWARD_FLAG = 1 << 4,
  GES_BACKWARD_FLAG = 1 << 5,
  GES_CLOCKWISE_FLAG = 1 << 6,
  GES_COUNT_CLOCKWISE_FLAG = 1 << 7,
  GES_WAVE_FLAG = 1 << 8,
};

// PAJ7620U2_20140305.asc -- Near_normal_mode_V5_6.15mm_121017 for 940nm.
// Vendor-supplied register init sequence; addresses/values are opaque to us,
// carried over verbatim from the reference implementation.
static const uint8_t INIT_REGS[][2] = {
    {0xEF, 0x00}, {0x32, 0x29}, {0x33, 0x01}, {0x34, 0x00}, {0x35, 0x01}, {0x36, 0x00}, {0x37, 0x07}, {0x38, 0x17},
    {0x39, 0x06}, {0x3A, 0x12}, {0x3F, 0x00}, {0x40, 0x02}, {0x41, 0xFF}, {0x42, 0x01}, {0x46, 0x2D}, {0x47, 0x0F},
    {0x48, 0x3C}, {0x49, 0x00}, {0x4A, 0x1E}, {0x4B, 0x00}, {0x4C, 0x20}, {0x4D, 0x00}, {0x4E, 0x1A}, {0x4F, 0x14},
    {0x50, 0x00}, {0x51, 0x10}, {0x52, 0x00}, {0x5C, 0x02}, {0x5D, 0x00}, {0x5E, 0x10}, {0x5F, 0x3F}, {0x60, 0x27},
    {0x61, 0x28}, {0x62, 0x00}, {0x63, 0x03}, {0x64, 0xF7}, {0x65, 0x03}, {0x66, 0xD9}, {0x67, 0x03}, {0x68, 0x01},
    {0x69, 0xC8}, {0x6A, 0x40}, {0x6D, 0x04}, {0x6E, 0x00}, {0x6F, 0x00}, {0x70, 0x80}, {0x71, 0x00}, {0x72, 0x00},
    {0x73, 0x00}, {0x74, 0xF0}, {0x75, 0x00}, {0x80, 0x42}, {0x81, 0x44}, {0x82, 0x04}, {0x83, 0x20}, {0x84, 0x20},
    {0x85, 0x00}, {0x86, 0x10}, {0x87, 0x00}, {0x88, 0x05}, {0x89, 0x18}, {0x8A, 0x10}, {0x8B, 0x01}, {0x8C, 0x37},
    {0x8D, 0x00}, {0x8E, 0xF0}, {0x8F, 0x81}, {0x90, 0x06}, {0x91, 0x06}, {0x92, 0x1E}, {0x93, 0x0D}, {0x94, 0x0A},
    {0x95, 0x0A}, {0x96, 0x0C}, {0x97, 0x05}, {0x98, 0x0A}, {0x99, 0x41}, {0x9A, 0x14}, {0x9B, 0x0A}, {0x9C, 0x3F},
    {0x9D, 0x33}, {0x9E, 0xAE}, {0x9F, 0xF9}, {0xA0, 0x48}, {0xA1, 0x13}, {0xA2, 0x10}, {0xA3, 0x08}, {0xA4, 0x30},
    {0xA5, 0x19}, {0xA6, 0x10}, {0xA7, 0x08}, {0xA8, 0x24}, {0xA9, 0x04}, {0xAA, 0x1E}, {0xAB, 0x1E}, {0xCC, 0x19},
    {0xCD, 0x0B}, {0xCE, 0x13}, {0xCF, 0x64}, {0xD0, 0x21}, {0xD1, 0x0F}, {0xD2, 0x88}, {0xE0, 0x01}, {0xE1, 0x04},
    {0xE2, 0x41}, {0xE3, 0xD6}, {0xE4, 0x00}, {0xE5, 0x0C}, {0xE6, 0x0A}, {0xE7, 0x00}, {0xE8, 0x00}, {0xE9, 0x00},
    {0xEE, 0x07}, {0xEF, 0x01}, {0x00, 0x1E}, {0x01, 0x1E}, {0x02, 0x0F}, {0x03, 0x10}, {0x04, 0x02}, {0x05, 0x00},
    {0x06, 0xB0}, {0x07, 0x04}, {0x08, 0x0D}, {0x09, 0x0E}, {0x0A, 0x9C}, {0x0B, 0x04}, {0x0C, 0x05}, {0x0D, 0x0F},
    {0x0E, 0x02}, {0x0F, 0x12}, {0x10, 0x02}, {0x11, 0x02}, {0x12, 0x00}, {0x13, 0x01}, {0x14, 0x05}, {0x15, 0x07},
    {0x16, 0x05}, {0x17, 0x07}, {0x18, 0x01}, {0x19, 0x04}, {0x1A, 0x05}, {0x1B, 0x0C}, {0x1C, 0x2A}, {0x1D, 0x01},
    {0x1E, 0x00}, {0x21, 0x00}, {0x22, 0x00}, {0x23, 0x00}, {0x25, 0x01}, {0x26, 0x00}, {0x27, 0x39}, {0x28, 0x7F},
    {0x29, 0x08}, {0x30, 0x03}, {0x31, 0x00}, {0x32, 0x1A}, {0x33, 0x1A}, {0x34, 0x07}, {0x35, 0x07}, {0x36, 0x01},
    {0x37, 0xFF}, {0x38, 0x36}, {0x39, 0x07}, {0x3A, 0x00}, {0x3E, 0xFF}, {0x3F, 0x00}, {0x40, 0x77}, {0x41, 0x40},
    {0x42, 0x00}, {0x43, 0x30}, {0x44, 0xA0}, {0x45, 0x5C}, {0x46, 0x00}, {0x47, 0x00}, {0x48, 0x58}, {0x4A, 0x1E},
    {0x4B, 0x1E}, {0x4C, 0x00}, {0x4D, 0x00}, {0x4E, 0xA0}, {0x4F, 0x80}, {0x50, 0x00}, {0x51, 0x00}, {0x52, 0x00},
    {0x53, 0x00}, {0x54, 0x00}, {0x57, 0x80}, {0x59, 0x10}, {0x5A, 0x08}, {0x5B, 0x94}, {0x5C, 0xE8}, {0x5D, 0x08},
    {0x5E, 0x3D}, {0x5F, 0x99}, {0x60, 0x45}, {0x61, 0x40}, {0x63, 0x2D}, {0x64, 0x02}, {0x65, 0x96}, {0x66, 0x00},
    {0x67, 0x97}, {0x68, 0x01}, {0x69, 0xCD}, {0x6A, 0x01}, {0x6B, 0xB0}, {0x6C, 0x04}, {0x6D, 0x2C}, {0x6E, 0x01},
    {0x6F, 0x32}, {0x71, 0x00}, {0x72, 0x01}, {0x73, 0x35}, {0x74, 0x00}, {0x75, 0x33}, {0x76, 0x31}, {0x77, 0x01},
    {0x7C, 0x84}, {0x7D, 0x03}, {0x7E, 0x01},
};

bool PAJ7620Component::select_bank_(uint8_t bank) { return this->write_byte(REG_BANK_SEL, bank); }

void PAJ7620Component::setup() {
  if (this->int_pin_ != nullptr) {
    this->int_pin_->setup();
  }

  // Datasheet: wait >=700us after power-up before the first transaction.
  delay_microseconds_safe(700);

  this->select_bank_(BANK0);
  this->select_bank_(BANK0);

  uint8_t part_id_lo, part_id_hi;
  if (!this->read_byte(0x00, &part_id_lo) || !this->read_byte(0x01, &part_id_hi)) {
    ESP_LOGE(TAG, "Failed to read part ID");
    this->mark_failed();
    return;
  }
  if (part_id_lo != 0x20 || part_id_hi != 0x76) {
    ESP_LOGE(TAG, "Unexpected part ID: 0x%02X 0x%02X", part_id_lo, part_id_hi);
    this->mark_failed();
    return;
  }

  // A register write can occasionally NACK during this ~175-entry burst.
  // Retry a few times with a short delay first; if a specific register keeps
  // NACKing regardless (observed reproducible on damaged hardware), log it
  // and move on rather than failing the whole component over one register in
  // an opaque vendor init table whose individual bits we can't reason about.
  //
  // A small delay is also inserted after every write (not just on failure):
  // on this specific unit, writing the whole table back-to-back with no gap
  // reliably drove the chip's I2C interface fully unresponsive partway
  // through the burst -- including the bank-select write that follows this
  // loop -- even when the individual writes that provoked it were skipped
  // entirely. Pacing the burst avoids that; it doesn't happen with a delay
  // in place, on this same table, on the same hardware.
  static constexpr uint8_t INIT_WRITE_MAX_ATTEMPTS = 5;
  for (const auto &reg : INIT_REGS) {
    bool ok = false;
    for (uint8_t attempt = 0; attempt < INIT_WRITE_MAX_ATTEMPTS; attempt++) {
      if (this->write_byte(reg[0], reg[1])) {
        ok = true;
        break;
      }
      delay_microseconds_safe(1000);
    }
    if (!ok) {
      ESP_LOGW(TAG, "Init write failed at register 0x%02X after %d attempts, continuing", reg[0],
               INIT_WRITE_MAX_ATTEMPTS);
    }
    delay_microseconds_safe(200);
  }

  // Gesture flag register (0x43) lives in bank 0 -- if this write NACKs like some of
  // the tail registers above, the chip stays in bank 1 and every runtime read() in
  // update() silently targets the wrong register, so this one is worth retrying hard.
  bool bank0_ok = false;
  for (uint8_t attempt = 0; attempt < INIT_WRITE_MAX_ATTEMPTS; attempt++) {
    if (this->select_bank_(BANK0)) {
      bank0_ok = true;
      break;
    }
    delay_microseconds_safe(1000);
  }
  if (!bank0_ok) {
    ESP_LOGE(TAG, "Failed to switch back to bank 0 after %d attempts", INIT_WRITE_MAX_ATTEMPTS);
    this->mark_failed();
  }
}

void PAJ7620Component::loop() {
  if (this->int_pin_ == nullptr)
    return;
  bool level = this->int_pin_->digital_read();
  if (level != this->last_int_level_) {
    ESP_LOGD(TAG, "int_pin transition: %s", level ? "-> high" : "-> LOW");
    this->last_int_level_ = level;
  }
  if (level)
    return;
  uint32_t now = millis();
  if (now - this->last_int_update_ms_ < INT_UPDATE_MIN_INTERVAL_MS)
    return;
  this->last_int_update_ms_ = now;
  this->update();
}

void PAJ7620Component::dump_config() {
  ESP_LOGCONFIG(TAG, "PAJ7620:");
  LOG_I2C_DEVICE(this);
  if (this->is_failed()) {
    ESP_LOGE(TAG, ESP_LOG_MSG_COMM_FAIL);
  }
  LOG_EVENT("  ", "Gesture", this);
  LOG_UPDATE_INTERVAL(this);
}

void PAJ7620Component::update() {
  uint8_t data[2];
  if (!this->read_bytes(REG_GES_FLAG_0, data, 2)) {
    ESP_LOGW(TAG, "Read failed");
    return;
  }
  uint16_t combined = data[0] | (static_cast<uint16_t>(data[1]) << 8);
  // Track every raw transition (including back to 0, the gesture registers'
  // self-clearing idle state) so a repeated gesture after an idle read is
  // still recognized as a new transition -- but only trigger the event for
  // an actual recognized gesture, not the clear-to-idle or an unknown flag.
  if (combined == this->last_state_)
    return;
  this->last_state_ = combined;
  if (const char *gesture = gesture_to_string_(combined); gesture != nullptr)
    this->trigger(gesture);
}

const char *PAJ7620Component::gesture_to_string_(uint16_t gesture) {
  // The sensor's raw up/down/left/right axes are fixed to its own physical mounting
  // orientation, which is rotated 90 degrees from the display (rotation: 90) -- so the
  // labels below are remapped one step counter-clockwise (up->left, right->up, down->right,
  // left->down) to match what the user actually sees, confirmed empirically on hardware.
  // forward/backward/clockwise/count_clockwise/wave are unaffected by an in-plane rotation.
  switch (gesture) {
    case GES_UP_FLAG:
      return "left";
    case GES_DOWN_FLAG:
      return "right";
    case GES_FORWARD_FLAG:
      return "forward";
    case GES_BACKWARD_FLAG:
      return "backward";
    case GES_RIGHT_FLAG:
      return "up";
    case GES_LEFT_FLAG:
      return "down";
    case GES_CLOCKWISE_FLAG:
      return "clockwise";
    case GES_WAVE_FLAG:
      return "wave";
    case GES_COUNT_CLOCKWISE_FLAG:
      return "count_clockwise";
    default:
      return nullptr;  // idle (0) or an unrecognized/combined flag
  }
}

}  // namespace esphome::paj7620
