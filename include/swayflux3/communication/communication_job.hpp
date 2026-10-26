#pragma once
#include "download_sink.hpp"
#include "transfer_info.hpp"
#include <swayflux3/core/job.hpp>

namespace swayflux3 {
class CommunicationJob : public Job {
  public:
    virtual CommunicationInfo communication_info() const = 0;
};
} // namespace swayflux3
