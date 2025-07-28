#pragma once
#include <memory>
#include <tuple>
#include "/home/azureuser/genltest/prototype/msg_processing_functions.h"

struct quic_message {
  using k_timestamp = uint64_t;
  using k_request_id = uint64_t;
  using k_payload_sz = size_t;

  template <typename T> using u_ptr = std::unique_ptr<T>;

  static void print_quick_message(const quic_message *msg) {

    uint64_t blk_id = 0;
    char buf[ZFS_MAX_DATASET_NAME_LEN];

    memcpy(&blk_id, msg->payload.get(), sizeof(blk_id));
    memcpy(buf, msg->payload.get()+ sizeof(blk_id), ZFS_MAX_DATASET_NAME_LEN);
    printf("[%s] quic_message: timestamp=%lu, req_id=%lu, payload_sz=%zu, zil_blk_id=%lu, poolname=%s\n",
           __func__, msg->timestamp, msg->req_id, msg->payload_sz, blk_id, buf);
    
  }

  static constexpr size_t payload_offset() {
    return sizeof(k_timestamp) + sizeof(k_request_id) + sizeof(k_payload_sz);
  }
  // the timestamp at the message generation at the sender side
  k_timestamp timestamp;
  // a unique id to identify the timestamp correctly
  k_request_id req_id;
  k_payload_sz payload_sz;
  u_ptr<uint8_t[]> payload;

  quic_message()
    : timestamp(0), req_id(0), payload_sz(0ULL), payload(nullptr) {}
  explicit quic_message(k_timestamp ts, k_request_id id)
    : timestamp(ts), req_id(id) {}

  static u_ptr<quic_message> construct_message(k_timestamp ts,
                                               k_request_id id) {
    return std::make_unique<quic_message>(ts, id);
  }

  static u_ptr<quic_message> deserialize_me(uint8_t *data, size_t data_sz) {
    u_ptr<quic_message> ptr = std::make_unique<quic_message>();
    size_t offset = 0;
    ::memcpy(&(ptr->timestamp), data, sizeof(k_timestamp));
    offset += sizeof(k_timestamp);
    ::memcpy(&(ptr->req_id), data + offset, sizeof(k_request_id));
    offset += sizeof(k_request_id);
    ::memcpy(&(ptr->payload_sz), data + offset, sizeof(k_payload_sz));
    offset += sizeof(k_payload_sz);
    ptr->payload = std::make_unique<uint8_t[]>(ptr->payload_sz);
    std::cout << "quic_message::deserialize_me: payload_sz=" << ptr->payload_sz << std::endl;
    ::memcpy(ptr->payload.get(), data + offset, ptr->payload_sz);
    for (auto i = 0ULL; i < ptr->payload_sz; ++i) {
      std::cout << std::hex << (int)ptr->payload.get()[i] << " ";
    }
    std::cout << "\n";
    return std::move(ptr);
  }

  std::tuple<u_ptr<uint8_t[]>, size_t> serialize_me(size_t sz) {
    std::unique_ptr<uint8_t[]> msg_ptr = std::make_unique<uint8_t[]>(sz);
    size_t offset = 0;
    ::memcpy(msg_ptr.get(), &timestamp, sizeof(timestamp));
    offset += sizeof(timestamp);
    ::memcpy(msg_ptr.get() + offset, &req_id, sizeof(req_id));
    offset += sizeof(req_id);
    ::memcpy(msg_ptr.get() + offset, &payload_sz, sizeof(payload_sz));
    offset += sizeof(payload_sz);
    ::memcpy(msg_ptr.get() + offset, payload.get(), 128);
    return {std::move(msg_ptr), sz};
  }
};