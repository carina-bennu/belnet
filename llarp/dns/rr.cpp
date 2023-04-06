#include "rr.hpp"
#include "dns.hpp"
#include "util/formattable.hpp"
#include <llarp/util/mem.hpp>
#include <llarp/util/logging.hpp>

namespace llarp
{
  namespace dns
  {
    ResourceRecord::ResourceRecord(const ResourceRecord& other)
        : rr_name(other.rr_name)
        , rr_type(other.rr_type)
        , rr_class(other.rr_class)
        , ttl(other.ttl)
        , rData(other.rData)
    {}

    ResourceRecord::ResourceRecord(ResourceRecord&& other)
        : rr_name(std::move(other.rr_name))
        , rr_type(std::move(other.rr_type))
        , rr_class(std::move(other.rr_class))
        , ttl(std::move(other.ttl))
        , rData(std::move(other.rData))
    {}

    ResourceRecord::ResourceRecord(Name_t name, RRType_t type, RR_RData_t data)
        : rr_name{std::move(name)}
        , rr_type{type}
        , rr_class{qClassIN}
        , ttl{1}
        , rData{std::move(data)}
    {}

    bool
    ResourceRecord::Encode(llarp_buffer_t* buf) const
    {
      if (not EncodeName(buf, rr_name))
        return false;
      if (!buf->put_uint16(rr_type))
      {
        return false;
      }
      if (!buf->put_uint16(rr_class))
      {
        return false;
      }
      if (!buf->put_uint32(ttl))
      {
        return false;
      }
      if (!EncodeRData(buf, rData))
      {
        return false;
      }
      return true;
    }

    bool
    ResourceRecord::Decode(llarp_buffer_t* buf)
    {
      uint16_t discard;
      if (!buf->read_uint16(discard))
        return false;
      if (!buf->read_uint16(rr_type))
      {
        llarp::LogDebug("failed to decode rr type");
        return false;
      }
      if (!buf->read_uint16(rr_class))
      {
        llarp::LogDebug("failed to decode rr class");
        return false;
      }
      if (!buf->read_uint32(ttl))
      {
        llarp::LogDebug("failed to decode ttl");
        return false;
      }
      if (!DecodeRData(buf, rData))
      {
        llarp::LogDebug("failed to decode rr rdata ", *this);
        return false;
      }
      return true;
    }

    util::StatusObject
    ResourceRecord::ToJSON() const
    {
      return util::StatusObject{
          {"name", rr_name},
          {"type", rr_type},
          {"class", rr_class},
          {"ttl", ttl},
          {"rdata", std::string{reinterpret_cast<const char*>(rData.data()), rData.size()}}};
    }

    

     std::string
    ResourceRecord::ToString() const
    {
      return fmt::format(
          "[RR name={} type={} class={} ttl={} rdata-size={}]",
          rr_name,
          rr_type,
          rr_class,
          ttl,
          rData.size());
    }

    bool
    ResourceRecord::HasCNameForTLD(const std::string& tld) const
    {
      if (rr_type != qTypeCNAME)
        return false;
      Name_t name;
      llarp_buffer_t buf(rData);
      if (not DecodeName(&buf, name))
        return false;
      return name.find(tld) != std::string::npos
          && name.rfind(tld) == (name.size() - tld.size()) - 1;
    }

  }  // namespace dns
}  // namespace llarp
