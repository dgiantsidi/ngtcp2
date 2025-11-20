#pragma once
#include "server_ccf.h"
extern Config config;
extern int64_t qpack_enc_stream_id, qpack_dec_stream_id;
extern int64_t ctrl_stream_id;
extern std::unique_ptr<ccf_monitor> ccf_monitor_ptr;