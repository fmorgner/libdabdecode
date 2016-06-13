#include "ensemble/ensemble.h"
#include "ensemble/service.h"
#include "ensemble/service_component.h"

#include "frame/cif.h"
#include "frame/fib.h"

#include <constants/transmission_modes.h>

#include <algorithm>
#include <cstring>
#include <vector>

namespace dab
  {

  using namespace __internal_dabdecode;
  using namespace __internal_common::types;

  ensemble::ensemble(symbol_queue_t & symbols, transmission_mode const & mode)
    : m_symbolQueue{symbols},
      m_mode{mode}
    {
    }

  std::string const & ensemble::label() const
    {
    return m_label;
    }

  std::uint16_t ensemble::id() const
    {
    return m_id;
    }

  std::uint8_t ensemble::country() const
    {
    return m_id >> 12;
    }

  std::uint16_t ensemble::reference() const
    {
    return m_id & 0xFFF;
    }

  std::map<std::uint32_t, std::shared_ptr<service>> const & ensemble::services() const
    {
    return m_services;
    }

  void ensemble::label(std::string const & label)
    {
    m_label = label;
    }

  void ensemble::id(std::uint16_t const id)
    {
    m_id = id;
    }

  void ensemble::add(std::shared_ptr<subchannel> subchannel)
    {
    if(subchannel && m_subchannels.find(subchannel->id()) == m_subchannels.cend())
      {
      m_subchannels[subchannel->id()] = subchannel;
      }
    }

  void ensemble::add(std::shared_ptr<service> service)
    {
    if(service && m_services.find(service->id()) == m_services.cend())
      {
      m_services[service->id()] = service;
      }
    }

  void ensemble::add(std::shared_ptr<service_component> component)
    {
    if(component && m_components.find(component->id()) == m_components.cend())
      {
      m_components[component->id()] = component;
      }
    }

  void ensemble::activate(std::shared_ptr<service> const & service)
    {
    if(service && m_services.find(service->id()) != m_services.cend())
      {
      m_activeService = service;
      }
    }

  bool ensemble::update()
    {
    if(next_frame())
      {
      auto const & fic = m_frame->fic();
      m_ficParser.parse(fic);

      if(m_activeService)
        {
        auto const primaryComponent = m_activeService->primary();

        if(primaryComponent)
          {
          auto const subchannel =  primaryComponent->subchannel();

          auto const start = subchannel->start();
          auto const end = start + subchannel->size();

          for(auto const & cif : m_frame->msc())
            {
            subchannel->process(cif.begin() + start * constants::kCuBits, cif.begin() + end * constants::kCuBits);
            }
          }
        }

      return true;
      }
    else
      {
      m_id = 0;
      m_label = "";
      return false;
      }
    }

  ensemble::operator bool() const
    {
    return m_label.size() && m_id;
    }

  std::pair<transport_mechanism, std::vector<std::uint8_t>> ensemble::active_data()
    {
    std::vector<std::uint8_t> data{};

    if(!m_activeService)
      {
      return std::make_pair(transport_mechanism::unknown, data);
      }

    auto const & service_component = m_activeService->primary();

    if(!service_component)
      {
      return std::make_pair(transport_mechanism::unknown, data);
      }

    auto const & subchannel = service_component->subchannel();

    if(!subchannel)
      {
      return std::make_pair(transport_mechanism::unknown, data);
      }

    return std::make_pair(service_component->transport(), subchannel->data());
    }

  bool ensemble::next_frame()
    {
    auto symbol = std::vector<float>{};
    auto extracted = std::vector<float>(sizeof(float) * m_mode.frame_symbols * m_mode.symbol_bits);

    for(auto symbolIndex = 0u; symbolIndex < m_mode.frame_symbols; ++symbolIndex)
      {
      while(!m_symbolQueue.wait_dequeue_timed(symbol, 100))
        {
        }

      std::memcpy(extracted.data() + symbolIndex * sizeof(float) * m_mode.symbol_bits,
                  symbol.data(), symbol.size() * sizeof(float));
      }

    m_frame = std::unique_ptr<frame>(new frame{std::move(extracted), m_mode});
    return (bool)m_frame;
    }

  }
