constexpr size_t ZFS_MAX_DATASET_NAME_LEN = 256;
constexpr int k_local_server_port = 9000;
/* do not change this*/
#define HEX_PER_UINT8 2
#define UBERBLOCK_DIGEST_BUF_SIZE SHA256_DIGEST_LENGTH * HEX_PER_UINT8 + 1


// body of message format for HTTP/3
struct http_submit_msg {
  uint64_t request_id;
  uint64_t latest_covered_request_id; // for optimization: the latest request id that covers all previous requests (i.e., the request with the largest request_id among the requests that cover all previous requests)
  uint64_t zil_blk_id;
  char poolname[ZFS_MAX_DATASET_NAME_LEN];
  char commitment[ZFS_MAX_DATASET_NAME_LEN];
  uint64_t ts;
  int blk_type; //this should be either TAIL or UB;
};

// body of message format for HTTP/3
struct http_reponse_msg {
  uint64_t request_id;
  uint64_t zil_blk_id; /* ZIL tail block ID or ub_txg*/
  uint64_t ccf_commit_seqno;
  char commitment[ZFS_MAX_DATASET_NAME_LEN]; /* ZIL tail cmt or ZIL head cmt*/
  int blk_type; //this should be either TAIL or UB;
  /* valid only for block_type::UB commitments*/
  char ub_digest[UBERBLOCK_DIGEST_BUF_SIZE]; 
  int zil_head_blk_id;

};

using http_submit_msg_t = struct http_submit_msg;
using http_reponse_msg_t = struct http_reponse_msg;
