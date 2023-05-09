#pragma once

#include <string>
#include <memory>
#include <llarp/net/sock_addr.hpp>
#include <llarp/util/logging.hpp>

#include <stdexcept>

namespace llarp::dns
{
  /// sets dns settings in a platform dependant way
  class I_Platform
  {
   public:
    virtual ~I_Platform() = default;

    /// Attempts to set belnet as the DNS server.
    /// throws if unsupported or fails.
    ///
    ///
    /// \param if_name -- the interface name to which we add the DNS servers, e.g. beldextun0.
    /// Typically tun_endpoint.GetIfName().
    /// \param dns -- the listening address of the belnet DNS server
    /// \param global -- whether to set up belnet for all DNS queries (true) or just .bdx & .mnode
    /// addresses (false).
    virtual void
    set_resolver(std::string if_name, llarp::SockAddr dns, bool global) = 0;
  };

}  // namespace llarp::dns
