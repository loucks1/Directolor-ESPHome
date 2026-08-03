#ifndef PAYLOAD_QUEUE_H
#define PAYLOAD_QUEUE_H

#include <cstdint>
#include "directolor_radio_types.h"

// Struct to hold a single payload and its metadata
struct PayloadEntry {
  uint8_t payload[32]; // Max payload size for NRF24L01
  int send_attempts;   // Number of send attempts for this payload
};

class PayloadQueue {
public:
  PayloadQueue();

  // Add a payload to the queue (enqueue)
  // Now takes send_attempts instead of bytes and pipe
  bool enqueue(const uint8_t* payload, int send_attempts);

  // Remove and return the next payload (dequeue)
  bool dequeue(PayloadEntry& entry);

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
