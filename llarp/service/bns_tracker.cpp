#include "bns_tracker.hpp"

namespace llarp::service
{
  std::function<void(std::optional<BNSLookupTracker::Addr_t>)>
  BNSLookupTracker::MakeResultHandler(
      std::string name,
      std::size_t numPeers,
      std::function<void(std::optional<Addr_t>)> resultHandler)
  {
    m_PendingLookups.emplace(name, LookupInfo{numPeers, resultHandler});
    return [name, this](std::optional<Addr_t> found) {
      auto itr = m_PendingLookups.find(name);
      if (itr == m_PendingLookups.end())
        return;
      itr->second.HandleOneResult(found);
      if (itr->second.IsDone())
        m_PendingLookups.erase(itr);
    };
  }

  bool
  BNSLookupTracker::LookupInfo::IsDone() const
  {
    return m_ResultsGotten == m_ResultsNeeded;
  }

  void
  BNSLookupTracker::LookupInfo::HandleOneResult(std::optional<Addr_t> result)
  {
    if (result)
    {
      m_CurrentValues.insert(*result);
    }
    m_ResultsGotten++;
    if (IsDone())
    {
      if (m_CurrentValues.size() == 1)
      {
        m_HandleResult(*m_CurrentValues.begin());
      }
      else
      {
        m_HandleResult(std::nullopt);
      }
    }
  }
}  // namespace llarp::service
