#include "payload_queue.h"
#include <esphome/core/log.h>
#include <cstring>

PayloadQueue::PayloadQueue() : head_(0), tail_(0), count_(0) {}

static const char *TAG = "payload_queue";

bool PayloadQueue::enqueue(const uint8_t* payload, int send_attempts) {
  PayloadEntry entry{};
  memcpy(entry.payload, payload, esphome::directolor_radio::MAX_NRF_PAYLOAD_SIZE);
  entry.send_attempts = send_attempts;
  entry.payload_ready = true;
  entry.logical = false;
  entry.mergeable = false;
  entry.code_attempts_left = 1;
  return enqueue_entry(entry);
}

bool PayloadQueue::enqueue_entry(const PayloadEntry& entry) {
  if (count_ >= capacity()) {
    ESP_LOGW(TAG, "Queue full: count=%d, capacity=%u", count_, static_cast<unsigned>(capacity()));
    return false;
  }
  buffer_[tail_] = entry;
  tail_ = static_cast<uint8_t>((tail_ + 1) % capacity());
  count_++;
  ESP_LOGD(TAG, "Enqueued payload: count=%d", count_);
  return true;
}

bool PayloadQueue::copy_at(uint8_t index, PayloadEntry& out) const {
  if (index >= count_) {
    return false;
  }
  out = buffer_[static_cast<uint8_t>((head_ + index) % capacity())];
  return true;
}

bool PayloadQueue::update_at(uint8_t index, const PayloadEntry& entry) {
  if (index >= count_) {
    return false;
  }
  buffer_[static_cast<uint8_t>((head_ + index) % capacity())] = entry;
  return true;
}

bool PayloadQueue::remove_at(uint8_t index) {
  if (index >= count_) {
    return false;
  }
  for (uint8_t i = index; i + 1 < count_; i++) {
    const uint8_t dst = static_cast<uint8_t>((head_ + i) % capacity());
    const uint8_t src = static_cast<uint8_t>((head_ + i + 1) % capacity());
    buffer_[dst] = buffer_[src];
  }
  count_--;
  tail_ = static_cast<uint8_t>((head_ + count_) % capacity());
  return true;
}

bool PayloadQueue::dequeue(PayloadEntry& entry) {
  if (count_ == 0) {
    return false;
  }
  entry = buffer_[head_];
  head_ = static_cast<uint8_t>((head_ + 1) % capacity());
  count_--;
  return true;
}

bool PayloadQueue::isEmpty() const {
  return count_ == 0;
}
