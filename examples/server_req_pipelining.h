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
constexpr size_t MAX_MSG_SIZE = 1024;

class Handler;
class Stream;
static int local_server_port = 12345;

struct queue_item {
  Handler *handler;
  Stream *stream;
  std::unique_ptr<quic_message> msg;
};

struct replies {
  std::queue<std::unique_ptr<queue_item>> response_queue;
  std::mutex queue_mutex;
};

struct HTTPHeader {
  HTTPHeader(const std::string_view &name, const std::string_view &value)
    : name(name), value(value) {}

  std::string_view name;
  std::string_view value;
};

struct ccf_callbacks_set {
  explicit ccf_callbacks_set(
    std::shared_ptr<void> dr,
    const std::function<void(std::weak_ptr<void>, uint64_t, uint8_t *, size_t)>
      f,
    const std::function<uint64_t(std::weak_ptr<void>)> c_f = nullptr) {
    replicate_func = f;
    check_func = c_f;
    driver = dr;
  }
  void invoke(uint64_t req_id, uint8_t *data = nullptr, size_t sz = 0) {
    if (data)
      replicate_func(driver, req_id, data, sz);
    else
      replicate_func(driver, req_id, nullptr, 0);
  }
  uint64_t invoke_check() { return check_func(driver); }
  // function obj to replicate the data
  std::function<void(std::weak_ptr<void>, uint64_t, uint8_t *, size_t)>
    replicate_func;
  // function obj to check the committed seqno
  std::function<uint64_t(std::weak_ptr<void>)> check_func;
  std::weak_ptr<void> driver;
};

struct Stream {
  Stream(int64_t stream_id, Handler *handler);
  ~Stream();

  int start_response(nghttp3_conn *conn,
                     std::unique_ptr<quic_message> msg_ptr = nullptr);
  int send_status_response(nghttp3_conn *conn, unsigned int status_code,
                           std::unique_ptr<quic_message> msg_ptr = nullptr,
                           const std::vector<HTTPHeader> &extra_headers = {});
  int send_redirect_response(nghttp3_conn *conn, unsigned int status_code,
                             const std::string_view &path);
  [[__maybe_unused__]] void http_acked_stream_data(uint64_t datalen);

  int64_t stream_id;
  Handler *handler;
  // uri is request uri/path.
  std::string uri;
  std::string method;
  std::string authority;
  std::string status_resp_body;

  // buffer with the data to be sent
  uint8_t *data;
  // datalen (size of *data)
  uint64_t datalen;
  // received data
  std::vector<uint8_t> data_vec;

  // @dimitra: maybe used so you can remove
  // dynresp is true if dynamic data response is enabled.
  bool dynresp;
  // dyndataleft is the number of dynamic data left to send.
  uint64_t dyndataleft;
  // dynbuflen is the number of bytes in-flight.
  uint64_t dynbuflen;
};

class Server;

// Endpoint is a local endpoint for QUIC.
struct Endpoint {
  Address addr;
  ev_io rev;
  Server *server;
  int fd;
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
  ev_io wev_, wev;
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
    std::cout << "*==== CCF ====* " << __PRETTY_FUNCTION__
              << ": committed_seqno=" << committed_seqno << "\n";
    return committed_seqno;
  }

  void register_replication(std::shared_ptr<ccf_callbacks_set> callback) {
    replication = callback;
  }

  void reply_func(const uint64_t last_cmt_seqno) {
    {
      std::lock_guard<std::mutex> lock(queue_handle->queue_mutex);
      while (!queue_handle->response_queue.empty()) {
        auto &item = queue_handle->response_queue.front();
        uint64_t blk_id = item->msg->req_id;

        if (cmd_replicated() >= blk_id) {
          std::cout << "*==== QUEUE ====* " << __PRETTY_FUNCTION__
                    << ": respond to stream_id=" << item->stream->stream_id
                    << " for blk_id=" << blk_id << "\n";
          item->stream->start_response(item->handler->httpconn_,
                                       std::move(item->msg));
          queue_handle->response_queue.pop();
        } else {
          return;
        }
      }
    }
  }

  int create_local_endpoint_receiver() {
    auto server_fd = socket(AF_INET, SOCK_STREAM, 0);

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;        // listen on all interfaces
    server_addr.sin_port = htons(local_server_port); // specify port number

    if (bind(server_fd, (sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
      std::cerr << "*==== ERROR ====* " << __func__
                << " could not bind to port " << local_server_port << std::endl;
      ::close(server_fd);
      return -1;
    }

    if (listen(server_fd, 5) < 0) {
      std::cerr << "*==== ERROR ====* " << __func__
                << " could not listen on port " << local_server_port
                << std::endl;
      ::close(server_fd);
      return -1;
    }

    std::cout << "*==== SYSTEM OPERATION ====* " << __func__
              << " we listen for connections at " << local_server_port
              << std::endl;

    sockaddr_in client_addr{};
    socklen_t client_len = sizeof(client_addr);
    int client_fd = accept(server_fd, (sockaddr *)&client_addr, &client_len);
    if (client_fd < 0) {
      std::cerr << "*==== ERROR ====* " << __func__
                << " error accepting connections (" << strerror(errno) << ")"
                << std::endl;
      ::close(server_fd);
      return -1;
    }

    std::cout << "*==== SYSTEM OPERATION ====* " << __func__
              << " accepted connection from monitor thread " << std::endl;

    // set the socket to non-blocking mode
    int flags = fcntl(client_fd, F_GETFL, 0);
    if (flags == -1) {
      std::cerr << "*==== ERROR ====* " << __func__ << " error getting flags ("
                << strerror(errno) << ")" << std::endl;
      ::close(client_fd);
      return -1;
    }

    if (fcntl(client_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
      std::cerr << "*==== ERROR ====* " << __func__
                << " error setting the O_NONBLOCK mode (" << strerror(errno)
                << ")" << std::endl;
      ::close(client_fd);
      return -1;
    }
    return client_fd;
  }
  struct replies *queue_handle;

  int get_local_endpoint() { return local_endpoint; }

private:
  std::unordered_map<std::string, Handler *, string_hash, std::equal_to<>>
    handlers_;
  struct ev_loop *loop_;
  std::vector<Endpoint> endpoints_;
  TLSServerContext &tls_ctx_;
  ev_signal sigintev_;
  ev_timer stateless_reset_regen_timer_;
  ev_timer timer_;
  int local_endpoint;
  size_t stateless_reset_bucket_;
  int server_id = -1;
  std::shared_ptr<ccf_callbacks_set> replication;
};

#endif // !defined(SERVER_H)

class ccf_monitor {
public:
  std::thread reply_thread;
  struct replies *handle_queue;
  ccf_monitor(Server *server, struct replies *handle)
    : srv(server), handle_queue(handle) {
    reply_thread = std::thread(&ccf_monitor::ccf_monitor_thread_func, this);
  }

private:
  Server *srv;

public:
  int create_local_endpoint_sender() {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0); // TCP socket
    if (socket_fd < 0) {
      std::cerr << "*=== =ERROR ====* " << __func__
                << " error creating socket\n";
      return -1;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(local_server_port);

    // convert IP address from text to binary
    if (inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr) <= 0) {
      std::cerr << "*==== ERROR ====* " << __func__
                << " error converting IP address" << std::endl;
      ::close(socket_fd);
      return -1;
    }

    int connect_tries = 0;
    for (;;) {
      // connect to the server
      if (connect(socket_fd, (sockaddr *)&server_addr, sizeof(server_addr)) <
          0) {
        std::cerr << "*==== ERROR ====* " << __func__
                  << " error connecting to the server"
                  << " (" << std::strerror(errno) << ")" << std::endl;
        connect_tries++;
        std::this_thread::sleep_for(std::chrono::seconds(3));
        if (connect_tries > 1000) {
          ::close(socket_fd);
          return -1;
        }
      } else {
        break;
      }
    }

    std::cout << "*==== SYSTEM OPERATION ====* " << __func__
              << " connected to server at port " << local_server_port
              << std::endl;
    // set the socket to non-blocking mode
    int flags = fcntl(socket_fd, F_GETFL, 0);
    if (flags == -1) {
      std::cerr << "*==== ERROR ====* " << __func__
                << " error getting flags for socket" << std::endl;
      ::close(socket_fd);
      return -1;
    }

    if (fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
      std::cerr << "*==== ERROR ====* " << __func__
                << " error setting non-blocking mode" << std::endl;
      ::close(socket_fd);
      return -1;
    }

    return socket_fd;
  }

  void ccf_monitor_thread_func();
};

void config_set_default(Config &config);

void print_usage();
