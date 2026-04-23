#include "payload_queue.h"
#include <esphome/core/log.h>
#include <cstring> // For memcpy

PayloadQueue::PayloadQueue() : head_(0), tail_(0), count_(0) {}

static const char *TAG = "payload_queue";

bool PayloadQueue::enqueue(const uint8_t* payload, int send_attempts) {
  if (count_ >= QUEUE_SIZE) {
    ESP_LOGW(TAG, "Queue full: count=%d, QUEUE_SIZE=%d", count_, QUEUE_SIZE);
    return false; // Queue is full
  }
  PayloadEntry& entry = buffer_[tail_];
  // Copy the entire 32 bytes, assuming the payload is always 32 bytes
  memcpy(entry.payload, payload, 32);
  entry.send_attempts = send_attempts;
  tail_ = (tail_ + 1) % QUEUE_SIZE;
  count_++;
  ESP_LOGD(TAG, "Enqueued payload: count=%d", count_);
  return true;
}

bool PayloadQueue::dequeue(PayloadEntry& entry) {
  if (count_ == 0) {
    return false; // Queue is empty
  }
  entry = buffer_[head_];
  head_ = (head_ + 1) % QUEUE_SIZE;
  count_--;
  return true;
}

bool PayloadQueue::isEmpty() const {
  return count_ == 0;
}