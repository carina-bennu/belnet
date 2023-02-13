#pragma once

#include "belnet_context.h"


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
    /// remote endpont's port
    int remote_port;
    
  };


  /// a result from a belnet_udp_bind call
  struct belnet_udp_bind_result
  {
    /// a socket id used to close a belnet udp socket
    int socket_id;
  };

    /// flow acceptor hook, return 0 success, return nonzero with errno on failure
  typedef int (*belnet_udp_flow_filter)(
    void* /*user*/, 
    const struct belnet_udp_flowinfo* /* remote address */, 
    void** /* flow-userdata */, 
    int* /* timeout */);

  /// hook function for handling packets
  typedef void (*belnet_udp_flow_recv_func)(
    const struct belnet_udp_flowinfo* /* remote address */, 
    char* /* data pointer */, 
    size_t /* data length */, 
    void* /* flow-userdata */);

  /// hook function for flow timeout
  typedef void (*belnet_udp_flow_timeout_func)(
    const struct belnet_udp_flowinfo* /* remote address  */, 
    void* /* flow-userdata */);

  /// inbound listen udp socket
  /// expose udp port exposePort to the void
  ///
  /// @param filter MUST be non null, pointing to a flow filter for accepting new udp flows, called
  /// with user data
  ///
  /// @param recv MUST be non null, pointing to a packet handler function for each flow, called
  /// with per flow user data provided by filter function if accepted
  ///
  /// @param timeout MUST be non null,
  /// pointing to a cleanup function to clean up a stale flow, staleness determined by the value
  /// given by the filter function returns 0 on success
  ///
  /// @returns nonzero on error in which it is an errno value
  int EXPORT
  belnet_udp_bind(
      int exposedPort,
      belnet_udp_flow_filter filter,
      belnet_udp_flow_recv_func recv,
      belnet_udp_flow_timeout_func timeout,
      void* user,
      struct belnet_udp_listen_result* result,
      struct belnet_context* ctx);


  /// @brief establish a udp flow to remote endpoint
  ///
  /// @param remote the remote address to establish to
  ///
  /// @param ctx the belnet context to use
  ///
  /// @return 0 on success, non zero errno on fail
  int EXPORT
  belnet_udp_establish(const struct belnet_udp_flowinfo* remote, struct belnet_context* ctx);

  /// @brief send on an established flow to remote endpoint
  ///
  /// @param flowinfo populated after call on success
  ///
  /// @param ptr pointer to data to send
  ///
  /// @param len the length of the data
  ///
  /// @param ctx the belnet context to use
  ///
  /// @returns 0 on success and non zero errno on fail
  int EXPORT
  belnet_udp_flow_send(
      const struct belnet_udp_flowinfo* remote,
      const void* ptr,
      size_t len,
      struct belnet_ctx* ctx);



#ifdef __cplusplus
}
#endif
