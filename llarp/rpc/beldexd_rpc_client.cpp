#include "beldexd_rpc_client.hpp"

#include <stdexcept>
#include <llarp/util/logging.hpp>

#include <llarp/router/abstractrouter.hpp>

#include <nlohmann/json.hpp>

#include <oxenc/bt.h>

#include <oxenc/hex.h>

#include <llarp/util/time.hpp>

namespace llarp
{
  namespace rpc
  {
    static constexpr oxenmq::LogLevel
    toLokiMQLogLevel(log::Level level)
    {
      switch (level)
      {
        case log::Level::critical:
          return oxenmq::LogLevel::fatal;
        case log::Level::err:
          return oxenmq::LogLevel::error;
        case log::Level::warn:
          return oxenmq::LogLevel::warn;
        case log::Level::info:
          return oxenmq::LogLevel::info;
        case log::Level::debug:
          return oxenmq::LogLevel::debug;
        case log::Level::trace:
        case log::Level::off:
        default:
          return oxenmq::LogLevel::trace;
      }
    }

    BeldexdRpcClient::BeldexdRpcClient(LMQ_ptr lmq, std::weak_ptr<AbstractRouter> r)
        : m_lokiMQ{std::move(lmq)}, m_Router{std::move(r)}
    {
      // m_lokiMQ->log_level(toLokiMQLogLevel(LogLevel::Instance().curLevel));

      // new block handler
      m_lokiMQ->add_category("notify", oxenmq::Access{oxenmq::AuthLevel::none})
          .add_command("block", [this](oxenmq::Message& m) { HandleNewBlock(m); });

      // TODO: proper auth here
      auto beldexdCategory = m_lokiMQ->add_category("beldexd", oxenmq::Access{oxenmq::AuthLevel::none});
      beldexdCategory.add_request_command(
          "get_peer_stats", [this](oxenmq::Message& m) { HandleGetPeerStats(m); });
      m_UpdatingList = false;
    }

    void
    BeldexdRpcClient::ConnectAsync(oxenmq::address url)
    {
      if (auto router = m_Router.lock())
      {
        if (not router->IsMasterNode())
        {
          throw std::runtime_error("we cannot talk to beldexd while not a master node");
        }
        LogInfo("connecting to beldexd via LMQ at ", url.full_address());
        m_Connection = m_lokiMQ->connect_remote(
            url,
            [self = shared_from_this()](oxenmq::ConnectionID) { self->Connected(); },
            [self = shared_from_this(), url](oxenmq::ConnectionID, std::string_view f) {
              llarp::LogWarn("Failed to connect to beldexd: ", f);
              if (auto router = self->m_Router.lock())
              {
                router->loop()->call([self, url]() { self->ConnectAsync(url); });
              }
            });
      }
    }

    void
    BeldexdRpcClient::Command(std::string_view cmd)
    {
      LogDebug("beldexd command: ", cmd);
      m_lokiMQ->send(*m_Connection, std::move(cmd));
    }

    void
    BeldexdRpcClient::HandleNewBlock(oxenmq::Message& msg)
    {
      if (msg.data.size() != 2)
      {
        LogError(
            "we got an invalid new block notification with ",
            msg.data.size(),
            " parts instead of 2 parts so we will not update the list of master nodes");
        return;  // bail
      }
      try
      {
        m_BlockHeight = std::stoll(std::string{msg.data[0]});
      }
      catch (std::exception& ex)
      {
        LogError("bad block height: ", ex.what());
        return;  // bail
      }

      LogDebug("new block at height ", m_BlockHeight);
      // don't upadate on block notification if an update is pending
      if (not m_UpdatingList)
        UpdateMasterNodeList();
    }

    void
    BeldexdRpcClient::UpdateMasterNodeList()
    {
      if (m_UpdatingList.exchange(true))
        return;  // update already in progress

      nlohmann::json request{
          {"fields",
           {
               {"pubkey_ed25519", true},
               {"master_node_pubkey", true},
               {"funded", true},
               {"active", true},
               {"block_hash", true},
           }},
      };
      if (!m_LastUpdateHash.empty())
        request["fields"]["poll_block_hash"] = m_LastUpdateHash;
      Request(
          "rpc.get_master_nodes",
          [self = shared_from_this()](bool success, std::vector<std::string> data) {

            if (not success)
              LogWarn("failed to update master node list");
            else if (data.size() < 2)
              LogWarn("beldexd gave empty reply for master node list");
            else
            {
              try
              {
                auto json = nlohmann::json::parse(std::move(data[1]));
                if (json.at("status") != "OK")
                  throw std::runtime_error{"get_master_nodes did not return 'OK' status"};
                if (auto it = json.find("unchanged");
                    it != json.end() and it->is_boolean() and it->get<bool>())
                  LogDebug("master node list unchanged");
                else
                {
                  self->HandleNewMasterNodeList(json.at("master_node_states"));
                  if (auto it = json.find("block_hash"); it != json.end() and it->is_string())
                    self->m_LastUpdateHash = it->get<std::string>();
                  else
                    self->m_LastUpdateHash.clear();
                }
              }
              catch (const std::exception& ex)
              {
                LogError("failed to process master node list: ", ex.what());
              }
            }
            // set down here so that the 1) we don't start updating until we're completely finished
            // with the previous update; and 2) so that m_UpdatingList also guards m_LastUpdateHash
            self->m_UpdatingList = false;
          },
          request.dump());
    }

    void
    BeldexdRpcClient::Connected()
    {
      constexpr auto PingInterval = 30s;
      auto makePingRequest = [self = shared_from_this()]() {
        // send a ping
        PubKey pk{};
        auto r = self->m_Router.lock();
        if (not r)
          return;  // router has gone away, maybe shutting down?

        pk = r->pubkey();

        nlohmann::json payload = {
            {"pubkey_ed25519", oxenc::to_hex(pk.begin(), pk.end())},
            {"version", {VERSION[0], VERSION[1], VERSION[2]}}};

        if (auto err = r->BeldexdErrorState())
          payload["error"] = *err;

        self->Request(
            "admin.belnet_ping",
            [](bool success, std::vector<std::string> data) {
              (void)data;
              LogDebug("Received response for ping. Successful: ", success);
            },
            payload.dump());

        // subscribe to block updates
        self->Request("sub.block", [](bool success, std::vector<std::string> data) {
          if (data.empty() or not success)
          {
            LogError("failed to subscribe to new blocks");
            return;
          }
          LogDebug("subscribed to new blocks: ", data[0]);
        });
        // Trigger an update on a regular timer as well in case we missed a block notify for some
        // reason (e.g. beldexd restarts and loses the subscription); we poll using the last known
        // hash so that the poll is very cheap (basically empty) if the block hasn't advanced.
        self->UpdateMasterNodeList();
      };
      // Fire one ping off right away to get things going.
      makePingRequest();
      m_lokiMQ->add_timer(makePingRequest, PingInterval);
    }

    void
    BeldexdRpcClient::HandleNewMasterNodeList(const nlohmann::json& j)
    {
      std::unordered_map<RouterID, PubKey> keymap;
      std::vector<RouterID> activeNodeList, decommNodeList, unfundedNodeList;
      if (not j.is_array())
        throw std::runtime_error{
            "Invalid master node list: expected array of master node states"};

      for (auto& mnode : j)
      {
        const auto ed_itr = mnode.find("pubkey_ed25519");
        if (ed_itr == mnode.end() or not ed_itr->is_string())
          continue;
        const auto svc_itr = mnode.find("master_node_pubkey");
        if (svc_itr == mnode.end() or not svc_itr->is_string())
          continue;
        const auto active_itr = mnode.find("active");
        if (active_itr == mnode.end() or not active_itr->is_boolean())
          continue;
        const bool active = active_itr->get<bool>();

        const auto funded_itr = mnode.find("funded");
        if (funded_itr == mnode.end() or not funded_itr->is_boolean())
          continue;
        const bool funded = funded_itr->get<bool>();

        RouterID rid;
        PubKey pk;
        if (not rid.FromHex(ed_itr->get<std::string_view>())
            or not pk.FromHex(svc_itr->get<std::string_view>()))
          continue;

        keymap[rid] = pk;
        (active       ? activeNodeList
             : funded ? decommNodeList
                      : unfundedNodeList)
            .push_back(std::move(rid));
      }

      if (activeNodeList.empty())
      {
        LogWarn("got empty master node list, ignoring.");
        return;
      }

      // inform router about the new list
      if (auto router = m_Router.lock())
      {
        auto& loop = router->loop();
        loop->call([this,
                    active = std::move(activeNodeList),
                    decomm = std::move(decommNodeList),
                    unfunded = std::move(unfundedNodeList),
                    keymap = std::move(keymap),
                    router = std::move(router)]() mutable {
          m_KeyMap = std::move(keymap);
          router->SetRouterWhitelist(active, decomm, unfunded);
        });
      }
      else
        LogWarn("Cannot update whitelist: router object has gone away");
    }

    void
    BeldexdRpcClient::InformConnection(RouterID router, bool success)
    {
      if (auto r = m_Router.lock())
      {
        r->loop()->call([router, success, this]() {
          if (auto itr = m_KeyMap.find(router); itr != m_KeyMap.end())
          {
            const nlohmann::json request = {
                {"passed", success}, {"pubkey", itr->second.ToHex()}, {"type", "belnet"}};
            Request(
                "admin.report_peer_status",
                [self = shared_from_this()](bool success, std::vector<std::string>) {
                  if (not success)
                  {
                    LogError("Failed to report connection status to Beldexd");
                    return;
                  }
                  LogDebug("reported connection status to core");
                },
                request.dump());
          }
        });
      }
    }

    SecretKey
    BeldexdRpcClient::ObtainIdentityKey()
    {
      std::promise<SecretKey> promise;
      Request(
          "admin.get_master_privkeys",
          [self = shared_from_this(), &promise](bool success, std::vector<std::string> data) {
            try
            {
              if (not success)
              {
                throw std::runtime_error(
                    "failed to get private key request "
                    "failed");
              }
              if (data.empty() or data.size() < 2)
              {
                throw std::runtime_error(
                    "failed to get private key request "
                    "data empty");
              }
              const auto j = nlohmann::json::parse(data[1]);
              SecretKey k;
              if (not k.FromHex(j.at("master_node_ed25519_privkey").get<std::string>()))
              {
                throw std::runtime_error("failed to parse private key");
              }
              promise.set_value(k);
            }
            catch (const std::exception& e)
            {
              LogWarn("Caught exception while trying to request admin keys: ", e.what());
              promise.set_exception(std::current_exception());
            }
            catch (...)
            {
              LogWarn("Caught non-standard exception while trying to request admin keys");
              promise.set_exception(std::current_exception());
            }
          });
      auto ftr = promise.get_future();
      return ftr.get();
    }

    void
    BeldexdRpcClient::LookupLNSNameHash(
        dht::Key_t namehash,
        std::function<void(std::optional<service::EncryptedName>)> resultHandler)
    {
      LogDebug("Looking Up LNS NameHash ", namehash);
      const nlohmann::json req{{"type", 2}, {"name_hash", namehash.ToHex()}};
      Request(
          "rpc.lns_resolve",
          [this, resultHandler](bool success, std::vector<std::string> data) {
            std::optional<service::EncryptedName> maybe = std::nullopt;
            if (success)
            {
              try
              {
                service::EncryptedName result;
                const auto j = nlohmann::json::parse(data[1]);
                result.ciphertext = oxenc::from_hex(j["encrypted_value"].get<std::string>());
                const auto nonce = oxenc::from_hex(j["nonce"].get<std::string>());
                if (nonce.size() != result.nonce.size())
                {
                  throw std::invalid_argument{fmt::format(
                      "nonce size mismatch: {} != {}", nonce.size(), result.nonce.size())};
                }

                std::copy_n(nonce.data(), nonce.size(), result.nonce.data());
                maybe = result;
              }
              catch (std::exception& ex)
              {
                LogError("failed to parse response from lns lookup: ", ex.what());
              }
            }
            if (auto r = m_Router.lock())
            {
              r->loop()->call(
                  [resultHandler, maybe = std::move(maybe)]() { resultHandler(std::move(maybe)); });
            }
          },
          req.dump());
    }

    void
    BeldexdRpcClient::HandleGetPeerStats(oxenmq::Message& msg)
    {
      LogInfo("Got request for peer stats (size: ", msg.data.size(), ")");
      for (auto str : msg.data)
      {
        LogInfo("    :", str);
      }

      if (auto router = m_Router.lock())
      {
        if (not router->peerDb())
        {
          LogWarn("HandleGetPeerStats called when router has no peerDb set up.");

          // TODO: this can sometimes occur if beldexd hits our API before we're done configuring
          //       (mostly an issue in a loopback testnet)
          msg.send_reply("EAGAIN");
          return;
        }

        try
        {
          // msg.data[0] is expected to contain a bt list of router ids (in our preferred string
          // format)
          if (msg.data.empty())
          {
            LogWarn("beldexd requested peer stats with no request body");
            msg.send_reply("peer stats request requires list of router IDs");
            return;
          }

          std::vector<std::string> routerIdStrings;
          oxenc::bt_deserialize(msg.data[0], routerIdStrings);

          std::vector<RouterID> routerIds;
          routerIds.reserve(routerIdStrings.size());

          for (const auto& routerIdString : routerIdStrings)
          {
            RouterID id;
            if (not id.FromString(routerIdString))
            {
              LogWarn("beldexd sent us an invalid router id: ", routerIdString);
              msg.send_reply("Invalid router id");
              return;
            }

            routerIds.push_back(std::move(id));
          }

          auto statsList = router->peerDb()->listPeerStats(routerIds);

          int32_t bufSize =
              256 + (statsList.size() * 1024);  // TODO: tune this or allow to grow dynamically
          auto buf = std::unique_ptr<uint8_t[]>(new uint8_t[bufSize]);
          llarp_buffer_t llarpBuf(buf.get(), bufSize);

          PeerStats::BEncodeList(statsList, &llarpBuf);

          msg.send_reply(
              std::string_view((const char*)llarpBuf.base, llarpBuf.cur - llarpBuf.base));
        }
        catch (const std::exception& e)
        {
          LogError("Failed to handle get_peer_stats request: ", e.what());
          msg.send_reply("server error");
        }
      }
    }
  }  // namespace rpc
}  // namespace llarp
