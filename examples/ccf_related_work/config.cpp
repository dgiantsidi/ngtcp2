#include "config.h"
Config config{};
int64_t qpack_enc_stream_id, qpack_dec_stream_id;
int64_t ctrl_stream_id;
std::unique_ptr<ccf_monitor> ccf_monitor_ptr;