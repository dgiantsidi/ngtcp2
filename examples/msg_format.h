constexpr size_t ZFS_MAX_DATASET_NAME_LEN = 256;
constexpr int k_local_server_port = 12345;

// body of message format for HTTP/3
struct http_submit_msg {
  uint64_t request_id;
  uint64_t zil_blk_id;
  char poolname[ZFS_MAX_DATASET_NAME_LEN];
  char commitment[ZFS_MAX_DATASET_NAME_LEN];
  uint64_t ts;
};

// body of message format for HTTP/3
struct http_reponse_msg {
  uint64_t request_id;
  uint64_t zil_blk_id;
  uint64_t ccf_commit_seqno;
};

using http_submit_msg_t = struct http_submit_msg;
using http_reponse_msg_t = struct http_reponse_msg;
