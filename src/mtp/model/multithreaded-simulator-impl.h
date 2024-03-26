/*
 * Copyright (c) 2023 State Key Laboratory for Novel Software Technology
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Songyuan Bai <i@f5soft.site>
 */

/**
 * \file
 * \ingroup mtp
 *  Declaration of classes ns3::MultithreadedSimulatorImpl
 */

#ifndef MULTITHREADED_SIMULATOR_IMPL_H
#define MULTITHREADED_SIMULATOR_IMPL_H

#include "ns3/event-id.h"
#include "ns3/event-impl.h"
#include "ns3/nstime.h"
#include "ns3/object-factory.h"
#include "ns3/simulator-impl.h"

#include <list>

namespace ns3
{

/**
 * @brief
 * Implementation of the multithreaded simulator
 */
class MultithreadedSimulatorImpl : public SimulatorImpl
{
  public:
    static TypeId GetTypeId();

    /** Default constructor. */
    MultithreadedSimulatorImpl();
    /** Destructor. */
    ~MultithreadedSimulatorImpl() override;

    // virtual from SimulatorImpl
    void Destroy() override;
    bool IsFinished() const override;
    void Stop() override;
    EventId Stop(const Time& delay) override;
    EventId Schedule(const Time& delay, EventImpl* event) override;
    void ScheduleWithContext(uint32_t context, const Time& delay, EventImpl* event) override;
    EventId ScheduleNow(EventImpl* event) override;
    EventId ScheduleDestroy(EventImpl* event) override;
    void Remove(const EventId& id) override;
    void Cancel(const EventId& id) override;
    bool IsExpired(const EventId& id) const override;
    void Run() override;
    Time Now() const override;
    Time GetDelayLeft(const EventId& id) const override;
    Time GetMaximumSimulationTime() const override;
    void SetScheduler(ObjectFactory schedulerFactory) override;
    uint32_t GetSystemId() const override;
    uint32_t GetContext() const override;
    uint64_t GetEventCount() const override;

  private:
    // Inherited from Object
    void DoDispose() override;

    /**
     * @brief Automatically divides the to-be-simulated topology
     *
     * This method is called at the beginning of MultithreadedSimulatorImpl::Run.
     * It will set each node a systemId. Then it creates logical processes according
     * to the number of partitions, and transfer old events to newly created logical
     * processes.
     *
     * If manual partition is enabled by calling MtpInterface::Enable with two parameters,
     * this method will not be called.
     */
    void Partition();

    bool m_partition;
    uint32_t m_maxThreads;
    Time m_minLookahead;
    TypeId m_schedulerTypeId;
    std::list<EventId> m_destroyEvents;
};

} // namespace ns3

#endif /* MULTITHREADED_SIMULATOR_IMPL_H */
