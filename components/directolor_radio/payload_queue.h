#ifndef PAYLOAD_QUEUE_H
#define PAYLOAD_QUEUE_H

#include <cstdint>
#include "directolor_radio_types.h"

// One queued transmission. Raw entries are opaque 32-byte frames (sendPayload).
// Logical entries remember the radio, action, and channels so a later request
// can be merged into this one before it is built.
struct PayloadEntry {
  uint8_t payload[32]{}; // On-air bytes once built; raw frames are stored here immediately
  int send_attempts{0};  // Radio repeats left for the current on-air frame
  bool logical{false};
  bool mergeable{false};
  bool payload_ready{false};
  uint8_t radio_code[4]{};
  BlindAction action{directolor_open};
  uint8_t channels[esphome::directolor_radio::REMOTE_CHANNELS]{};
  uint8_t channel_count{0};
  uint8_t code_attempts_left{1}; // Distinct random/CRC payloads still to send, including the current one
};

class PayloadQueue {
public:
  PayloadQueue();

  // Add a raw 32-byte frame (not merged with logical commands)
  bool enqueue(const uint8_t* payload, int send_attempts);

  // Add a fully populated entry (logical command or raw)
  bool enqueue_entry(const PayloadEntry& entry);

  // Remove and return the next payload (dequeue)
  bool dequeue(PayloadEntry& entry);

  // Logical index 0 is the next entry that will be dequeued
  bool copy_at(uint8_t index, PayloadEntry& out) const;
  bool update_at(uint8_t index, const PayloadEntry& entry);
  bool remove_at(uint8_t index);

  // Check if the queue is empty
  bool isEmpty() const;

  // Number of queued payloads
  uint8_t size() const { return count_; }

  // Maximum capacity
  static constexpr size_t capacity() { return esphome::directolor_radio::PAYLOAD_QUEUE_SIZE; }

private:
  PayloadEntry buffer_[esphome::directolor_radio::PAYLOAD_QUEUE_SIZE];
  uint8_t head_;  // Index of the next item to dequeue
  uint8_t tail_;  // Index where the next item will be enqueued
  uint8_t count_; // Number of items in the queue
};

#endif // PAYLOAD_QUEUE_H
