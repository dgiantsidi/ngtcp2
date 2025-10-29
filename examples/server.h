/*
 * ngtcp2
 *
 * Copyright (c) 2017 ngtcp2 contributors
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#pragma once
#ifndef SERVER_H
#  define SERVER_H

#  ifdef HAVE_CONFIG_H
#    include <config.h>
#  endif // defined(HAVE_CONFIG_H)

#  include <vector>
#  include <unordered_map>
#  include <string>
#  include <deque>
#  include <string_view>
#  include <memory>
#  include <functional>
#  include <thread>
#  include <mutex>

// #include <span>
#  include "custom_span.h"

#  include <ngtcp2/ngtcp2.h>
#  include <ngtcp2/ngtcp2_crypto.h>
#  include <nghttp3/nghttp3.h>
#  include <ev.h>

#  include "server_base.h"
#  include "tls_server_context.h"
#  include "network.h"
#  include "shared.h"
#  include "message_format.h"
#  include <queue>

using namespace ngtcp2;

struct HTTPHeader {
  HTTPHeader(const std::string_view &name, const std::string_view &value)
    : name(name), value(value) {}

  std::string_view name;
  std::string_view value;
};

class Handler;
struct FileEntry;

struct callable_replication {
  explicit callable_replication(
    std::shared_ptr<void> dr,
    const std::function<void(std::weak_ptr<void>, uint64_t, uint8_t *, size_t)>
      f,
    const std::function<uint64_t(std::weak_ptr<void>)> c_f = nullptr) {
    func = f;
    check_func = c_f;
    driver = dr;
  }
  void invoke(uint64_t req_id, uint8_t *data = nullptr, size_t sz = 0) {
    if (data)
      func(driver, req_id, data, sz);
    else
      func(driver, req_id, nullptr, 0);
  }
  uint64_t invoke_check() { return check_func(driver); }
  std::function<void(std::weak_ptr<void>, uint64_t, uint8_t *, size_t)> func;
  std::function<uint64_t(std::weak_ptr<void>)> check_func;
  std::weak_ptr<void> driver;
};

struct Stream {
  Stream(int64_t stream_id, Handler *handler);

  int start_response(nghttp3_conn *conn,
                     std::unique_ptr<quic_message> msg_ptr = nullptr);
  std::pair<FileEntry, int> open_file(const std::string &path);
  void map_file(const FileEntry &fe);
  int send_status_response(nghttp3_conn *conn, unsigned int status_code,
                           std::unique_ptr<quic_message> msg_ptr = nullptr,
                           const std::vector<HTTPHeader> &extra_headers = {});
  int send_redirect_response(nghttp3_conn *conn, unsigned int status_code,
                             const std::string_view &path);
  int64_t find_dyn_length(const std::string_view &path);
  void http_acked_stream_data(uint64_t datalen);

  int64_t stream_id;
  Handler *handler;
  // uri is request uri/path.
  std::string uri;
  std::string method;
  std::string authority;
  std::string status_resp_body;
  // data is a pointer to the memory which maps file denoted by fd.
  uint8_t *data;
  // datalen is the length of mapped file by data.
  uint64_t datalen;
  // dynresp is true if dynamic data response is enabled.
  bool dynresp;
  // dyndataleft is the number of dynamic data left to send.
  uint64_t dyndataleft;
  // dynbuflen is the number of bytes in-flight.
  uint64_t dynbuflen;
  std::vector<uint8_t> data_vec;
};

class Server;

// Endpoint is a local endpoint.
struct Endpoint {
  Address addr;
  ev_io rev;
  Server *server;
  int fd;
};

struct queue_item {
  Handler *handler;
  Stream *stream;
  std::unique_ptr<quic_message> msg;
};

class Handler : public HandlerBase {
public:
  Handler(struct ev_loop *loop, Server *server);
  ~Handler();

  int init(const Endpoint &ep, const Address &local_addr, const sockaddr *sa,
           socklen_t salen, const ngtcp2_cid *dcid, const ngtcp2_cid *scid,
           const ngtcp2_cid *ocid, Span<const uint8_t> token,
           ngtcp2_token_type token_type, uint32_t version,
           TLSServerContext &tls_ctx);

  int on_read(const Endpoint &ep, const Address &local_addr, const sockaddr *sa,
              socklen_t salen, const ngtcp2_pkt_info *pi,
              Span<const uint8_t> data);
  int on_write();
  int write_streams();
  int feed_data(const Endpoint &ep, const Address &local_addr,
                const sockaddr *sa, socklen_t salen, const ngtcp2_pkt_info *pi,
                Span<const uint8_t> data);
  void update_timer();
  int handle_expiry();
  void signal_write();
  int handshake_completed();

  Server *server() const;
  int recv_stream_data(uint32_t flags, int64_t stream_id,
                       Span<const uint8_t> data);
  int acked_stream_data_offset(int64_t stream_id, uint64_t datalen);
  uint32_t version() const;
  void on_stream_open(int64_t stream_id);
  int on_stream_close(int64_t stream_id, uint64_t app_error_code);
  void start_draining_period();
  int start_closing_period();
  int handle_error();
  int send_conn_close();

  int update_key(uint8_t *rx_secret, uint8_t *tx_secret,
                 ngtcp2_crypto_aead_ctx *rx_aead_ctx, uint8_t *rx_iv,
                 ngtcp2_crypto_aead_ctx *tx_aead_ctx, uint8_t *tx_iv,
                 const uint8_t *current_rx_secret,
                 const uint8_t *current_tx_secret, size_t secretlen);

  int setup_httpconn();
  void http_consume(int64_t stream_id, size_t nconsumed);
  void extend_max_remote_streams_bidi(uint64_t max_streams);
  Stream *find_stream(int64_t stream_id);
  void http_begin_request_headers(int64_t stream_id);
  void http_recv_request_header(Stream *stream, int32_t token,
                                nghttp3_rcbuf *name, nghttp3_rcbuf *value);
  int http_end_request_headers(Stream *stream);
  int http_end_stream(Stream *stream);
  int start_response(Stream *stream,
                     std::unique_ptr<quic_message> msg_ptr = nullptr);
  int on_stream_reset(int64_t stream_id);
  int on_stream_stop_sending(int64_t stream_id);
  int extend_max_stream_data(int64_t stream_id, uint64_t max_data);
  void shutdown_read(int64_t stream_id, int app_error_code);
  void http_acked_stream_data(Stream *stream, uint64_t datalen);
  void http_stream_close(int64_t stream_id, uint64_t app_error_code);
  int http_stop_sending(int64_t stream_id, uint64_t app_error_code);
  int http_reset_stream(int64_t stream_id, uint64_t app_error_code);

  void write_qlog(const void *data, size_t datalen);

  void on_send_blocked(Endpoint &ep, const ngtcp2_addr &local_addr,
                       const ngtcp2_addr &remote_addr, unsigned int ecn,
                       Span<const uint8_t> data, size_t gso_size);
  void start_wev_endpoint(const Endpoint &ep);

  int send_blocked_packet();
  std::unordered_map<int64_t, std::unique_ptr<Stream>> streams_;

  nghttp3_conn *httpconn_;
  int64_t ctrl_stream_id;
  int64_t qpack_enc_stream_id, qpack_dec_stream_id;

private:
  struct ev_loop *loop_;
  Server *server_;
  std::mutex handler_mtx_;
  ev_io wev_;
  ev_timer timer_;
  FILE *qlog_;
  ngtcp2_cid scid_;
  // nghttp3_conn *httpconn_;
  // conn_closebuf_ contains a packet which contains CONNECTION_CLOSE.
  // This packet is repeatedly sent as a response to the incoming
  // packet in draining period.
  std::unique_ptr<Buffer> conn_closebuf_;
  // nkey_update_ is the number of key update occurred.
  size_t nkey_update_;
  bool no_gso_;

  struct {
    bool send_blocked;
    size_t num_blocked;
    size_t num_blocked_sent;
    // blocked field is effective only when send_blocked is true.
    struct {
      Endpoint *endpoint;
      Address local_addr;
      Address remote_addr;
      unsigned int ecn;
      Span<const uint8_t> data;
      size_t gso_size;
    } blocked[2];
    std::unique_ptr<uint8_t[]> data;
  } tx_;
};

struct string_hash {
  using is_transparent = void;

  size_t operator()(const std::string_view &s) const {
    return std::hash<std::string_view>{}(s);
  }

  size_t operator()(const std::string &s) const {
    return std::hash<std::string>{}(s);
  }
};

class Server {
public:
  Server(struct ev_loop *loop, TLSServerContext &tls_ctx);
  ~Server();

  int init(const char *addr, const char *port);
  void disconnect();
  void close();

  int on_read(Endpoint &ep);
  void read_pkt(Endpoint &ep, const Address &local_addr, const sockaddr *sa,
                socklen_t salen, const ngtcp2_pkt_info *pi,
                Span<const uint8_t> data);
  int send_version_negotiation(uint32_t version, Span<const uint8_t> dcid,
                               Span<const uint8_t> scid, Endpoint &ep,
                               const Address &local_addr, const sockaddr *sa,
                               socklen_t salen);
  int send_retry(const ngtcp2_pkt_hd *chd, Endpoint &ep,
                 const Address &local_addr, const sockaddr *sa, socklen_t salen,
                 size_t max_pktlen);
  int send_stateless_connection_close(const ngtcp2_pkt_hd *chd, Endpoint &ep,
                                      const Address &local_addr,
                                      const sockaddr *sa, socklen_t salen);
  int send_stateless_reset(size_t pktlen, Span<const uint8_t> dcid,
                           Endpoint &ep, const Address &local_addr,
                           const sockaddr *sa, socklen_t salen);
  int verify_retry_token(ngtcp2_cid *ocid, const ngtcp2_pkt_hd *hd,
                         const sockaddr *sa, socklen_t salen);
  int verify_token(const ngtcp2_pkt_hd *hd, const sockaddr *sa,
                   socklen_t salen);
  int send_packet(Endpoint &ep, const ngtcp2_addr &local_addr,
                  const ngtcp2_addr &remote_addr, unsigned int ecn,
                  Span<const uint8_t> data);
  std::pair<Span<const uint8_t>, int>
  send_packet(Endpoint &ep, bool &no_gso, const ngtcp2_addr &local_addr,
              const ngtcp2_addr &remote_addr, unsigned int ecn,
              Span<const uint8_t> data, size_t gso_size);
  void remove(const Handler *h);

  void associate_cid(const ngtcp2_cid *cid, Handler *h);
  void dissociate_cid(const ngtcp2_cid *cid);

  void on_stateless_reset_regen();
  void assign_server_id(const int &id) { server_id = id; };

  int get_id() { return server_id; }

  int replicate_cmd(uint64_t req_id, uint8_t *data = nullptr, size_t sz = 0) {
    replication->invoke(req_id, data, sz);
    return 0;
  }

  uint64_t cmd_replicated() {
    uint64_t committed_seqno = replication->invoke_check();
    std::cout << "*====RAFT====* " << __PRETTY_FUNCTION__
              << ": committed_seqno=" << committed_seqno << "\n";
    return committed_seqno;
  }

  void register_replication(std::shared_ptr<callable_replication> callback) {
    replication = callback;
  }

  void reply_func(const uint64_t last_cmt_seqno) {
    {
#  if 1
      while (!response_queue.empty()) {
        auto &item = response_queue.front();
        uint64_t blk_id = item->msg->req_id;

        if (cmd_replicated() >= blk_id) {
          std::cout << "*====STATUS====* " << __PRETTY_FUNCTION__
                    << ": Processing response queue, stream_id="
                    << item->stream->stream_id << ", blk_id=" << blk_id << "\n";
          item->stream->start_response(item->handler->httpconn_,
                                       std::move(item->msg));
          // item->handler->on_stream_close(item->stream->stream_id,
          // NGHTTP3_H3_NO_ERROR);
          response_queue.pop();
        } else {
          return;
        }
      }
#  endif
    }
  }

  void server_reply_thread_func(void *server) {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0); // TCP socket
    if (socket_fd < 0) {
      std::cerr << __func__ << ":" << __LINE__ << ": error creating socket"
                << std::endl;
      return;
    }
    if (socket_fd < 0) {
      std::cerr << __func__ << ":" << __LINE__ << ": error creating socket"
                << std::endl;
      return;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(12345); // port number

    // convert IP address from text to binary
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
      std::cerr << __func__ << ":" << __LINE__
                << ": error converting IP address" << std::endl;
      ::close(socket_fd);
      return;
    }

    // connect to the server
    if (connect(socket_fd, (sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
      std::cerr << __func__ << ":" << __LINE__
                << ": error connecting to the server" << std::endl;
      ::close(socket_fd);
      return;
    }
    // Set the socket to non-blocking mode
    int flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags == -1) {
      std::cerr << __func__ << ":" << __LINE__
                << ": error getting flags for socket" << std::endl;
      ::close(socket_fd);
      return;
    }

    if (fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
      std::cerr << __func__ << ":" << __LINE__
                << ": error setting non-blocking mode" << std::endl;
      ::close(socket_fd);
      return;
    }

    std::cout << __func__
              << ": Thread has connected to the notifications thread!"
              << std::endl;
    Server *srv = static_cast<Server *>(server);
    uint64_t cur_seq_no = 0;
    while (true) {
      {
        std::lock_guard<std::mutex> lock(srv->queue_mutex);
        if (!srv->response_queue.empty()) {
          if (srv->cmd_replicated() != cur_seq_no) {
            cur_seq_no = srv->cmd_replicated();
            // send to the thread that seqno changed
            std::cout << "*====NOTIFY====* " << __PRETTY_FUNCTION__
                      << ": Notifying cmt thread, cur_seq_no=" << cur_seq_no
                      << "\n";
            send(socket_fd, &cur_seq_no, sizeof(uint64_t), 0);
          }
        } else {
          std::cout << "*====NOTIFY====* " << __PRETTY_FUNCTION__
                    << ": response_queue is empty, sleeping...\n";
        }
      }
      std::this_thread::sleep_for(std::chrono::microseconds(100000));
    }
  }

  std::queue<std::unique_ptr<queue_item>> response_queue;
  std::mutex queue_mutex;

private:
  std::unordered_map<std::string, Handler *, string_hash, std::equal_to<>>
    handlers_;
  struct ev_loop *loop_;
  std::vector<Endpoint> endpoints_;
  TLSServerContext &tls_ctx_;
  ev_signal sigintev_;
  ev_timer stateless_reset_regen_timer_;
  ev_timer timer_;

  size_t stateless_reset_bucket_;
  int server_id = -1;
  std::shared_ptr<callable_replication> replication;
  std::thread reply_thread;
  ev_io wev; // local-thread related
  int server_port = 12345;
};

#endif // !defined(SERVER_H)

void config_set_default(Config &config);

void print_usage();
