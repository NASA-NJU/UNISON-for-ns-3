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
    ~MultithreadedSimulatorImpl();

    // virtual from SimulatorImpl
    virtual void Destroy();
    virtual bool IsFinished() const;
    virtual void Stop();
    virtual void Stop(const Time& delay);
    virtual EventId Schedule(const Time& delay, EventImpl* event);
    virtual void ScheduleWithContext(uint32_t context, const Time& delay, EventImpl* event);
    virtual EventId ScheduleNow(EventImpl* event);
    virtual EventId ScheduleDestroy(EventImpl* event);
    virtual void Remove(const EventId& id);
    virtual void Cancel(const EventId& id);
    virtual bool IsExpired(const EventId& id) const;
    virtual void Run();
    virtual Time Now() const;
    virtual Time GetDelayLeft(const EventId& id) const;
    virtual Time GetMaximumSimulationTime() const;
    virtual void SetScheduler(ObjectFactory schedulerFactory);
    virtual uint32_t GetSystemId() const;
    virtual uint32_t GetContext() const;
    virtual uint64_t GetEventCount() const;

  private:
    // Inherited from Object
    virtual void DoDispose();

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
