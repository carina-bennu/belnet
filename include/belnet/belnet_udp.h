#pragma once

#include "belnet_context.h"

#ifdef _WIN32
extern "C"
{
  struct iovec
  {
    void* iov_base;
    size_t iov_len;
  };
}
#else
#include <sys/uio.h>
#endif

#ifdef __cplusplus
extern "C"
{
#endif

  /// information about a udp flow
  struct belnet_udp_flowinfo
  {
    /// the socket id for this flow used for i/o purposes and closing this socket
    int socket_id;
    /// remote endpoint's .bdx or .mnode address
    char remote_addr[256];
    /// local endpoint's ip address
    char local_addr[64];
    /// remote endpont's port
    int remote_port;
    /// local endpoint's port
    int local_port;
  };


  /// a result from a belnet_udp_bind call
  struct belnet_udp_bind_result
  {
    /// a socket id used to close a belnet udp socket
    int socket_id;
  };

    /// flow acceptor hook, return 0 success, return nonzero with errno on failure
  typedef int (*belnet_udp_flow_filter)(void*, const struct belnet_udp_flowinfo*, void**, int*);

  /// hook function for handling packets
  typedef void (*belnet_udp_flow_recv_func)(
      const struct belnet_udp_flowinfo*, char*, size_t, void*);

  /// hook function for flow timeout
  typedef void (*belnet_udp_flow_timeout_func)(const belnet_udp_flowinfo*, void*);

  /// inbound listen udp socket
  /// expose udp port exposePort to the void
  /// filter MUST be non null, pointing to a flow filter for accepting new udp flows, called with
  /// user data recv MUST be non null, pointing to a packet handler function for each flow, called
  /// with per flow user data provided by filter function if accepted timeout MUST be non null,
  /// pointing to a cleanup function to clean up a stale flow, staleness determined by the value
  /// given by the filter function returns 0 on success returns nonzero on error in which it is an
  /// errno value
  int EXPORT
  belnet_udp_bind(
      int exposedPort,
      belnet_udp_flow_filter filter,
      belnet_udp_flow_recv_func recv,
      belnet_udp_flow_timeout_func timeout,
      void* user,
      struct belnet_udp_listen_result* result,
      struct belnet_context* ctx);



#ifdef __cplusplus
}
#endif
