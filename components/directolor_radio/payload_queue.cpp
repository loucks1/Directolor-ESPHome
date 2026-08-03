#include "payload_queue.h"
#include <esphome/core/log.h>
#include <cstring>

PayloadQueue::PayloadQueue() : head_(0), tail_(0), count_(0) {}

static const char *TAG = "payload_queue";

bool PayloadQueue::enqueue(const uint8_t* payload, int send_attempts) {
  if (count_ >= capacity()) {
    ESP_LOGW(TAG, "Queue full: count=%d, capacity=%u", count_, static_cast<unsigned>(capacity()));
    return false;
  }
  PayloadEntry& entry = buffer_[tail_];
  memcpy(entry.payload, payload, esphome::directolor_radio::MAX_NRF_PAYLOAD_SIZE);
  entry.send_attempts = send_attempts;
  tail_ = static_cast<uint8_t>((tail_ + 1) % capacity());
  count_++;
  ESP_LOGD(TAG, "Enqueued payload: count=%d", count_);
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
