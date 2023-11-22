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
 *  Declaration of classes ns3::LogicalProcess
 */

#ifndef LOGICAL_PROCESS_H
#define LOGICAL_PROCESS_H

#include "ns3/event-id.h"
#include "ns3/event-impl.h"
#include "ns3/nstime.h"
#include "ns3/object-factory.h"
#include "ns3/ptr.h"
#include "ns3/scheduler.h"

#include <atomic>
#include <chrono>
#include <map>
#include <tuple>
#include <vector>

namespace ns3
{

/**
 * @brief
 * Implementation of the logical process (LP) used by the multhreaded simulator.
 */
class LogicalProcess
{
  public:
    /** Default constructor */
    LogicalProcess();

    /** Destructor */
    ~LogicalProcess();

    /**
     * Enable this logical process object by giving it a unique systemId,
     * and let it know the total number of systems.
     *
     * @param systemId
     * @param systemCount
     */
    void Enable(const uint32_t systemId, const uint32_t systemCount);

    /**
     * @brief Calculate the lookahead value.
     */
    void CalculateLookAhead();

    /**
     * @brief Receive events sent by other logical processes in the previous round.
     */
    void ReceiveMessages();

    /**
     * @brief Process all events in the current round.
     */
    void ProcessOneRound();

    /**
     * @brief Get the execution time of the last round.
     *
     * This method is called by MtpInterfaceused to determine the priority of each LP.
     *
     * @return The execution tiem of the last round
     */
    inline uint64_t GetExecutionTime() const
    {
        return m_executionTime;
    }

    /**
     * @brief Get the pending event count of the next round.
     *
     * This method is called by MtpInterfaceused to determine the priority of each LP.
     *
     * @return Number of pending events of the next round
     */
    inline uint64_t GetPendingEventCount() const
    {
        return m_pendingEventCount;
    }

    /**
     * @brief Get the future event list (scheduler)
     *
     * @return The event list
     */
    inline Ptr<Scheduler> GetPendingEvents() const
    {
        return m_events;
    }

    /**
     * @brief Invoke an event immediately at the current time.
     *
     * This method is called when another thread wants to process an event of an LP
     * that does not belongs to it. It is used at the very beginning of the simulation
     * when the main thread will invoke events of newly allocated LP, whose timestamps
     * are zero.
     * 
     * @param ev The event to be invoked now
     */
    void InvokeNow(const Scheduler::Event& ev);

    // The following methods are mapped from MultithreadedSimulatorImpl
    EventId Schedule(const Time& delay, EventImpl* event);
    void ScheduleAt(const uint32_t context, const Time& time, EventImpl* event);
    void ScheduleWithContext(LogicalProcess* remote,
                             const uint32_t context,
                             const Time& delay,
                             EventImpl* event);
    void Remove(const EventId& id);
    void Cancel(const EventId& id);
    bool IsExpired(const EventId& id) const;
    void SetScheduler(ObjectFactory schedulerFactory);
    Time Next() const;

    inline bool isLocalFinished() const
    {
        return m_stop || m_events->IsEmpty();
    }

    inline void Stop()
    {
        m_stop = true;
    }

    inline Time Now() const
    {
        return TimeStep(m_currentTs);
    }

    inline Time GetDelayLeft(const EventId& id) const
    {
        return TimeStep(id.GetTs() - m_currentTs);
    }

    inline uint32_t GetSystemId(void) const
    {
        return m_systemId;
    }

    inline uint32_t GetContext() const
    {
        return m_currentContext;
    }

    inline uint64_t GetEventCount() const
    {
        return m_eventCount;
    }

  private:
    uint32_t m_systemId;
    uint32_t m_systemCount;
    bool m_stop;
    uint32_t m_uid;
    uint32_t m_currentContext;
    uint32_t m_currentUid;
    uint64_t m_currentTs;
    uint64_t m_eventCount;
    uint64_t m_pendingEventCount;
    Ptr<Scheduler> m_events;
    Time m_lookAhead;

    std::map<uint32_t, std::vector<std::tuple<uint64_t, uint32_t, uint32_t, Scheduler::Event>>>
        m_mailbox; // event message mail box
    std::chrono::nanoseconds::rep m_executionTime;
};

} // namespace ns3

#endif /* LOGICAL_PROCESS_H */
