extern "C"
{
#include <wintun.h>
}

#include <iphlpapi.h>
#include "wintun.hpp"
#include "exception.hpp"
#include "dll.hpp"
#include "guid.hpp"
#include <unordered_set>
#include <map>
#include <llarp/router/abstractrouter.hpp>
#include <llarp/util/str.hpp>
#include <llarp/util/thread/queue.hpp>
#include <llarp/util/logging.hpp>
#include <llarp/vpn/platform.hpp>

namespace llarp::win32
{
  namespace
  {
    auto logcat = log::Cat("wintun");
    constexpr auto PoolName = "belnet";
  }  // namespace

  using Adapter_ptr = std::shared_ptr<_WINTUN_ADAPTER>;

  struct PacketWrapper
  {
    BYTE* data;
    DWORD size;
    WINTUN_SESSION_HANDLE session;
    WINTUN_RELEASE_RECEIVE_PACKET_FUNC* release;
    /// copy our data into an ip packet struct
    net::IPPacket
    copy() const
    {
      net::IPPacket pkt{size};
      std::copy_n(data, size, pkt.data());
      return pkt;
    }

    ~PacketWrapper()
    {
      release(session, data);
    }
  };

  class WintunDLL : public DLL
  {
   public:
    WINTUN_CREATE_ADAPTER_FUNC* create_adapter;
    WINTUN_OPEN_ADAPTER_FUNC* open_adapter;
    WINTUN_CLOSE_ADAPTER_FUNC* close_adapter;

    WINTUN_START_SESSION_FUNC* start_session;
    WINTUN_END_SESSION_FUNC* end_session;

    WINTUN_GET_ADAPTER_LUID_FUNC* get_adapter_LUID;
    WINTUN_GET_READ_WAIT_EVENT_FUNC* get_adapter_handle;

    WINTUN_RECEIVE_PACKET_FUNC* read_packet;
    WINTUN_RELEASE_RECEIVE_PACKET_FUNC* release_read;

    WINTUN_ALLOCATE_SEND_PACKET_FUNC* alloc_write;
    WINTUN_SEND_PACKET_FUNC* write_packet;

    WINTUN_SET_LOGGER_FUNC* set_logger;
    WINTUN_GET_RUNNING_DRIVER_VERSION_FUNC* get_version;
    /// read out all the wintun function pointers from a library handle
    WintunDLL() : DLL{"wintun.dll"}
    {
      init("WintunGetRunningDriverVersion", get_version);
      init("WintunCreateAdapter", create_adapter);
      init("WintunOpenAdapter", open_adapter);
      init("WintunCloseAdapter", close_adapter);
      init("WintunStartSession", start_session);
      init("WintunEndSession", end_session);
      init("WintunGetAdapterLUID", get_adapter_LUID);
      init("WintunReceivePacket", read_packet);
      init("WintunReleaseReceivePacket", release_read);
      init("WintunSendPacket", write_packet);
      init("WintunAllocateSendPacket", alloc_write);
      init("WintunSetLogger", set_logger);
      init("WintunGetReadWaitEvent", get_adapter_handle);

      log::info(logcat, fmt::format("wintun version {0:x} loaded", get_version()));
    }

    /// autovivify a wintun adapter handle
    [[nodiscard]] auto
    make_adapter(std::string adapter_name, std::string tunnel_name) const
    {
      auto adapter_name_wide = to_wide(adapter_name);
      if (auto _impl = open_adapter(adapter_name_wide.c_str()))
      {
        log::info(logcat, "opened existing adapter: '{}'", adapter_name);
        return _impl;
      }
      if (auto err = GetLastError())
      {
        log::info(
            logcat, "did not open existing adapter '{}': {}", adapter_name, error_to_string(err));
        SetLastError(0);
      }
      const auto guid =
          llarp::win32::MakeDeterministicGUID(fmt::format("{}|{}", adapter_name, tunnel_name));
      log::info(logcat, "creating adapter: '{}' on pool '{}'", adapter_name, tunnel_name);
      auto tunnel_name_wide = to_wide(tunnel_name);
      if (auto _impl = create_adapter(adapter_name_wide.c_str(), tunnel_name_wide.c_str(), &guid))
        return _impl;
      throw win32::error{"failed to create wintun adapter"};
    }
  };

  class WintunAdapter
  {
    WINTUN_CLOSE_ADAPTER_FUNC* _close_adapter;
    WINTUN_GET_ADAPTER_LUID_FUNC* _get_adapter_LUID;

    WINTUN_GET_READ_WAIT_EVENT_FUNC* _get_handle;
    WINTUN_START_SESSION_FUNC* _start_session;
    WINTUN_END_SESSION_FUNC* _end_session;

    WINTUN_ADAPTER_HANDLE _handle;

    [[nodiscard]] auto
    get_adapter_LUID() const
    {
      NET_LUID _uid{};
      _get_adapter_LUID(_handle, &_uid);
      return _uid;
    }

   public:
    WintunAdapter(const WintunDLL& dll, std::string name)
        : _close_adapter{dll.close_adapter}
        , _get_adapter_LUID{dll.get_adapter_LUID}
        , _get_handle{dll.get_adapter_handle}
        , _start_session{dll.start_session}
        , _end_session{dll.end_session}
    {
      _handle = dll.make_adapter(std::move(name), PoolName);
      if (_handle == nullptr)
        throw std::runtime_error{"failed to create wintun adapter"};
    }

    /// put adapter up
    void
    Up(const vpn::InterfaceInfo& info) const
    {
      const auto luid = get_adapter_LUID();
      for (const auto& addr : info.addrs)
      {
        // TODO: implement ipv6
        if (addr.fam != AF_INET)
          continue;
        MIB_UNICASTIPADDRESS_ROW AddressRow;
        InitializeUnicastIpAddressEntry(&AddressRow);
        AddressRow.InterfaceLuid = luid;

        AddressRow.Address.Ipv4.sin_family = AF_INET;
        AddressRow.Address.Ipv4.sin_addr.S_un.S_addr = ToNet(net::TruncateV6(addr.range.addr)).n;
        AddressRow.OnLinkPrefixLength = addr.range.HostmaskBits();
        AddressRow.DadState = IpDadStatePreferred;

        if (auto err = CreateUnicastIpAddressEntry(&AddressRow); err != ERROR_SUCCESS)
          throw error{err, fmt::format("cannot set address '{}'", addr.range)};
        LogDebug(fmt::format("added address: '{}'", addr.range));
      }
    }

    /// put adapter down and close it
    void
    Down() const
    {
      _close_adapter(_handle);
    }

    /// auto vivify a wintun session handle and read handle off of our adapter
    [[nodiscard]] std::pair<WINTUN_SESSION_HANDLE, HANDLE>
    session() const
    {
      if (auto impl = _start_session(_handle, WINTUN_MAX_RING_CAPACITY))
      {
        if (auto handle = _get_handle(impl))
          return {impl, handle};
        _end_session(impl);
      }
      return {nullptr, nullptr};
    }
  };

  class WintunSession
  {
    WINTUN_END_SESSION_FUNC* _end_session;
    WINTUN_RECEIVE_PACKET_FUNC* _recv_pkt;
    WINTUN_RELEASE_RECEIVE_PACKET_FUNC* _release_pkt;
    WINTUN_ALLOCATE_SEND_PACKET_FUNC* _alloc_write;
    WINTUN_SEND_PACKET_FUNC* _write_pkt;
    WINTUN_SESSION_HANDLE _impl;
    HANDLE _handle;

   public:
    WintunSession(const WintunDLL& dll)
        : _end_session{dll.end_session}
        , _recv_pkt{dll.read_packet}
        , _release_pkt{dll.release_read}
        , _alloc_write{dll.alloc_write}
        , _write_pkt{dll.write_packet}
        , _impl{nullptr}
        , _handle{nullptr}
    {}

    void
    Start(const std::shared_ptr<WintunAdapter>& adapter)
    {
      if (auto [impl, handle] = adapter->session(); impl and handle)
      {
        _impl = impl;
        _handle = handle;
        return;
      }
      throw error{GetLastError(), "could not create wintun session"};
    }

    void
    Stop() const
    {
      _end_session(_impl);
    }

    void
    WaitFor(std::chrono::milliseconds dur)
    {
      WaitForSingleObject(_handle, dur.count());
    }

    /// read a unique pointer holding a packet read from wintun, returns the packet if we read one
    /// and a bool, set to true if our adapter is now closed
    [[nodiscard]] std::pair<std::unique_ptr<PacketWrapper>, bool>
    ReadPacket() const
    {
      // typedef so the return statement fits on 1 line :^D
      using Pkt_ptr = std::unique_ptr<PacketWrapper>;
      DWORD sz;
      if (auto* ptr = _recv_pkt(_impl, &sz))
        return {Pkt_ptr{new PacketWrapper{ptr, sz, _impl, _release_pkt}}, false};
      const auto err = GetLastError();
      if (err == ERROR_NO_MORE_ITEMS or err == ERROR_HANDLE_EOF)
      {
        SetLastError(0);
        return {nullptr, err == ERROR_HANDLE_EOF};
      }
      throw error{err, "failed to read packet"};
    }

    /// write an ip packet to the interface, return 2 bools, first is did we write the packet,
    /// second if we are terminating
    std::pair<bool, bool>
    WritePacket(net::IPPacket pkt) const
    {
      if (auto* buf = _alloc_write(_impl, pkt.size()))
      {
        std::copy_n(pkt.data(), pkt.size(), buf);
        _write_pkt(_impl, buf);
        return {true, false};
      }
      const auto err = GetLastError();
      if (err == ERROR_BUFFER_OVERFLOW or err == ERROR_HANDLE_EOF)
      {
        SetLastError(0);
        return {err != ERROR_BUFFER_OVERFLOW, err == ERROR_HANDLE_EOF};
      }
      throw error{err, "failed to write packet"};
    }
  };

  class WintunInterface : public vpn::NetworkInterface
  {
    AbstractRouter* const _router;
    std::shared_ptr<WintunAdapter> _adapter;
    std::shared_ptr<WintunSession> _session;
    thread::Queue<net::IPPacket> _recv_queue;
    thread::Queue<net::IPPacket> _send_queue;
    std::thread _recv_thread;
    std::thread _send_thread;

    static inline constexpr size_t packet_queue_length = 1024;

   public:
    WintunInterface(const WintunDLL& dll, vpn::InterfaceInfo info, AbstractRouter* router)
        : vpn::NetworkInterface{std::move(info)}
        , _router{router}
        , _adapter{std::make_shared<WintunAdapter>(dll, m_Info.ifname)}
        , _session{std::make_shared<WintunSession>(dll)}
        , _recv_queue{packet_queue_length}
        , _send_queue{packet_queue_length}
    {}

    void
    Start() override
    {
      m_Info.index = 0;
      // put the adapter and set addresses
      _adapter->Up(m_Info);
      // start up io session
      _session->Start(_adapter);

      // start read packet loop
      _recv_thread = std::thread{[session = _session, this]() {
        do
        {
          // read all our packets this iteration
          bool more{true};
          do
          {
            auto [pkt, done] = session->ReadPacket();
            // bail if we are closing
            if (done)
              return;
            if (pkt)
              _recv_queue.pushBack(pkt->copy());
            else
              more = false;
          } while (more);
          // wait for more packets
          session->WaitFor(5s);
        } while (true);
      }};
      // start write packet loop
      _send_thread = std::thread{[this, session = _session]() {
        do
        {
          if (auto maybe = _send_queue.popFrontWithTimeout(100ms))
          {
            auto [written, done] = session->WritePacket(std::move(*maybe));
            if (done)
              return;
          }
        } while (_send_queue.enabled());
      }};
    }

    void
    Stop() override
    {
      // end writing packets
      _send_queue.disable();
      _send_thread.join();
      // end reading packets
      _session->Stop();
      _recv_thread.join();
      // close session
      _session.reset();
      // put adapter down
      _adapter->Down();
      _adapter.reset();
    }

    net::IPPacket
    ReadNextPacket() override
    {
      net::IPPacket pkt{};
      if (auto maybe_pkt = _recv_queue.tryPopFront())
        pkt = std::move(*maybe_pkt);
      return pkt;
    }

    bool
    WritePacket(net::IPPacket pkt) override
    {
      return _send_queue.tryPushBack(std::move(pkt)) == thread::QueueReturn::Success;
    }

    int
    PollFD() const override
    {
      return -1;
    }

    void
    MaybeWakeUpperLayers() const override
    {
      _router->TriggerPump();
    }
  };

  struct WintunContext
  {
    WintunDLL dll{};
  };

  std::shared_ptr<WintunContext>
  WintunContext_new()
  {
    return std::make_shared<WintunContext>();
  }

  std::shared_ptr<vpn::NetworkInterface>
  WintunInterface_new(
      std::shared_ptr<llarp::win32::WintunContext> const& ctx,
      const llarp::vpn::InterfaceInfo& info,
      llarp::AbstractRouter* r)
  {
    return std::static_pointer_cast<vpn::NetworkInterface>(
        std::make_shared<WintunInterface>(ctx->dll, info, r));
  }

}  // namespace llarp::win32