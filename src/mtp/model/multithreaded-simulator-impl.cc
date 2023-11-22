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
 *  Implementation of classes ns3::MultithreadedSimulatorImpl
 */

#include "multithreaded-simulator-impl.h"

#include "mtp-interface.h"

#include "ns3/channel.h"
#include "ns3/node-container.h"
#include "ns3/node-list.h"
#include "ns3/node.h"
#include "ns3/simulator.h"
#include "ns3/type-id.h"
#include "ns3/uinteger.h"

#include <algorithm>
#include <queue>
#include <thread>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("MultithreadedSimulatorImpl");

NS_OBJECT_ENSURE_REGISTERED(MultithreadedSimulatorImpl);

MultithreadedSimulatorImpl::MultithreadedSimulatorImpl()
{
    NS_LOG_FUNCTION(this);
    if (!MtpInterface::isPartitioned())
    {
        MtpInterface::Enable(1, 0);
        m_partition = true;
    }
    else
    {
        m_partition = false;
    }
}

MultithreadedSimulatorImpl::~MultithreadedSimulatorImpl()
{
    NS_LOG_FUNCTION(this);
}

TypeId
MultithreadedSimulatorImpl::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::MultithreadedSimulatorImpl")
            .SetParent<SimulatorImpl>()
            .SetGroupName("Mtp")
            .AddConstructor<MultithreadedSimulatorImpl>()
            .AddAttribute("MaxThreads",
                          "The maximum threads used in simulation",
                          UintegerValue(std::thread::hardware_concurrency()),
                          MakeUintegerAccessor(&MultithreadedSimulatorImpl::m_maxThreads),
                          MakeUintegerChecker<uint32_t>(1))
            .AddAttribute("MinLookahead",
                          "The minimum lookahead in a partition",
                          TimeValue(TimeStep(0)),
                          MakeTimeAccessor(&MultithreadedSimulatorImpl::m_minLookahead),
                          MakeTimeChecker(TimeStep(0)));
    return tid;
}

void
MultithreadedSimulatorImpl::Destroy()
{
    while (!m_destroyEvents.empty())
    {
        Ptr<EventImpl> ev = m_destroyEvents.front().PeekEventImpl();
        m_destroyEvents.pop_front();
        NS_LOG_LOGIC("handle destroy " << ev);
        if (!ev->IsCancelled())
        {
            ev->Invoke();
        }
    }
    MtpInterface::Disable();
}

bool
MultithreadedSimulatorImpl::IsFinished() const
{
    return MtpInterface::isFinished();
}

void
MultithreadedSimulatorImpl::Stop()
{
    NS_LOG_FUNCTION(this);
    for (uint32_t i = 0; i < MtpInterface::GetSize(); i++)
    {
        MtpInterface::GetSystem(i)->Stop();
    }
}

void
MultithreadedSimulatorImpl::Stop(const Time& delay)
{
    NS_LOG_FUNCTION(this << delay.GetTimeStep());
    Simulator::Schedule(delay, &Simulator::Stop);
}

EventId
MultithreadedSimulatorImpl::Schedule(const Time& delay, EventImpl* event)
{
    NS_LOG_FUNCTION(this << delay.GetTimeStep() << event);
    return MtpInterface::GetSystem()->Schedule(delay, event);
}

void
MultithreadedSimulatorImpl::ScheduleWithContext(uint32_t context,
                                                const Time& delay,
                                                EventImpl* event)
{
    NS_LOG_FUNCTION(this << context << delay.GetTimeStep() << event);
    LogicalProcess* remote = MtpInterface::GetSystem(NodeList::GetNode(context)->GetSystemId());
    MtpInterface::GetSystem()->ScheduleWithContext(remote, context, delay, event);
}

EventId
MultithreadedSimulatorImpl::ScheduleNow(EventImpl* event)
{
    return Schedule(TimeStep(0), event);
}

EventId
MultithreadedSimulatorImpl::ScheduleDestroy(EventImpl* event)
{
    EventId id(Ptr<EventImpl>(event, false),
               GetMaximumSimulationTime().GetTimeStep(),
               0xffffffff,
               EventId::DESTROY);
    MtpInterface::CriticalSection cs;
    m_destroyEvents.push_back(id);
    return id;
}

void
MultithreadedSimulatorImpl::Remove(const EventId& id)
{
    if (id.GetUid() == EventId::DESTROY)
    {
        // destroy events.
        for (auto i = m_destroyEvents.begin(); i != m_destroyEvents.end(); i++)
        {
            if (*i == id)
            {
                m_destroyEvents.erase(i);
                break;
            }
        }
    }
    else
    {
        MtpInterface::GetSystem()->Remove(id);
    }
}

void
MultithreadedSimulatorImpl::Cancel(const EventId& id)
{
    if (!IsExpired(id))
    {
        id.PeekEventImpl()->Cancel();
    }
}

bool
MultithreadedSimulatorImpl::IsExpired(const EventId& id) const
{
    if (id.GetUid() == EventId::DESTROY)
    {
        // destroy events.
        if (id.PeekEventImpl() == nullptr || id.PeekEventImpl()->IsCancelled())
        {
            return true;
        }
        for (auto i = m_destroyEvents.begin(); i != m_destroyEvents.end(); i++)
        {
            if (*i == id)
            {
                return false;
            }
        }
        return true;
    }
    else
    {
        return MtpInterface::GetSystem()->IsExpired(id);
    }
}

void
MultithreadedSimulatorImpl::Run()
{
    NS_LOG_FUNCTION(this);
    // auto partition
    if (m_partition)
    {
        Partition();
    }
    MtpInterface::Run();
}

Time
MultithreadedSimulatorImpl::Now() const
{
    // Do not add function logging here, to avoid stack overflow
    return MtpInterface::GetSystem()->Now();
}

Time
MultithreadedSimulatorImpl::GetDelayLeft(const EventId& id) const
{
    if (IsExpired(id))
    {
        return TimeStep(0);
    }
    else
    {
        return MtpInterface::GetSystem()->GetDelayLeft(id);
    }
}

Time
MultithreadedSimulatorImpl::GetMaximumSimulationTime() const
{
    return Time::Max() / 2;
}

void
MultithreadedSimulatorImpl::SetScheduler(ObjectFactory schedulerFactory)
{
    NS_LOG_FUNCTION(this << schedulerFactory);
    for (uint32_t i = 0; i < MtpInterface::GetSize(); i++)
    {
        MtpInterface::GetSystem(i)->SetScheduler(schedulerFactory);
    }
    m_schedulerTypeId = schedulerFactory.GetTypeId();
}

uint32_t
MultithreadedSimulatorImpl::GetSystemId() const
{
    return MtpInterface::GetSystem()->GetSystemId();
}

uint32_t
MultithreadedSimulatorImpl::GetContext() const
{
    return MtpInterface::GetSystem()->GetContext();
}

uint64_t
MultithreadedSimulatorImpl::GetEventCount() const
{
    uint64_t eventCount = 0;
    for (uint32_t i = 0; i < MtpInterface::GetSize(); i++)
    {
        eventCount += MtpInterface::GetSystem(i)->GetEventCount();
    }
    return eventCount;
}

void
MultithreadedSimulatorImpl::DoDispose()
{
    SimulatorImpl::DoDispose();
}

void
MultithreadedSimulatorImpl::Partition()
{
    NS_LOG_FUNCTION(this);
    uint32_t systemId = 0;
    const NodeContainer nodes = NodeContainer::GetGlobal();
    bool* visited = new bool[nodes.GetN()]{false};
    std::queue<Ptr<Node>> q;

    // if m_minLookahead is not set, calculate the median of delay for every link
    if (m_minLookahead == TimeStep(0))
    {
        std::vector<Time> delays;
        for (auto it = nodes.Begin(); it != nodes.End(); it++)
        {
            Ptr<Node> node = *it;
            for (uint32_t i = 0; i < node->GetNDevices(); i++)
            {
                Ptr<NetDevice> localNetDevice = node->GetDevice(i);
                Ptr<Channel> channel = localNetDevice->GetChannel();
                if (!channel)
                {
                    continue;
                }
                // cut-off p2p links for partition
                if (localNetDevice->IsPointToPoint())
                {
                    TimeValue delay;
                    channel->GetAttribute("Delay", delay);
                    delays.push_back(delay.Get());
                }
            }
        }
        std::sort(delays.begin(), delays.end());
        if (delays.empty())
        {
            m_minLookahead = TimeStep(0);
        }
        else if (delays.size() % 2 == 1)
        {
            m_minLookahead = delays[delays.size() / 2];
        }
        else
        {
            m_minLookahead = (delays[delays.size() / 2 - 1] + delays[delays.size() / 2]) / 2;
        }
        NS_LOG_INFO("Min lookahead is set to " << m_minLookahead);
    }

    // perform a BFS on the whole network topo to assign each node a systemId
    for (auto it = nodes.Begin(); it != nodes.End(); it++)
    {
        Ptr<Node> node = *it;
        if (!visited[node->GetId()])
        {
            q.push(node);
            systemId++;
            while (!q.empty())
            {
                // pop from BFS queue
                node = q.front();
                q.pop();
                visited[node->GetId()] = true;
                // assign this node the current systemId
                node->SetSystemId(systemId);
                NS_LOG_INFO("node " << node->GetId() << " is set to system " << systemId);

                for (uint32_t i = 0; i < node->GetNDevices(); i++)
                {
                    Ptr<NetDevice> localNetDevice = node->GetDevice(i);
                    Ptr<Channel> channel = localNetDevice->GetChannel();
                    if (!channel)
                    {
                        continue;
                    }
                    // cut-off p2p links for partition
                    if (localNetDevice->IsPointToPoint())
                    {
                        TimeValue delay;
                        channel->GetAttribute("Delay", delay);
                        // if delay is below threshold, do not cut-off
                        if (delay.Get() >= m_minLookahead)
                        {
                            continue;
                        }
                    }
                    // grab the adjacent nodes
                    for (uint32_t j = 0; j < channel->GetNDevices(); j++)
                    {
                        Ptr<Node> remote = channel->GetDevice(j)->GetNode();
                        // if it's not visited, add it to the current partition
                        if (!visited[remote->GetId()])
                        {
                            q.push(remote);
                        }
                    }
                }
            }
        }
    }
    delete[] visited;

    // after the partition, we finally know the system count (# of LPs)
    const uint32_t systemCount = systemId;
    const uint32_t threadCount = std::min(m_maxThreads, systemCount);
    NS_LOG_INFO("Partition done! " << systemCount << " systems share " << threadCount
                                   << " threads");

    // create new LPs
    MtpInterface::EnableNew(threadCount, systemCount);

    // set scheduler
    ObjectFactory schedulerFactory;
    schedulerFactory.SetTypeId(m_schedulerTypeId);
    for (uint32_t i = 1; i <= systemCount; i++)
    {
        MtpInterface::GetSystem(i)->SetScheduler(schedulerFactory);
    }

    // remove old events in public LP
    const Ptr<Scheduler> oldEvents = MtpInterface::GetSystem()->GetPendingEvents();
    const Ptr<Scheduler> eventsToBeTransferred = schedulerFactory.Create<Scheduler>();
    while (!oldEvents->IsEmpty())
    {
        Scheduler::Event next = oldEvents->RemoveNext();
        eventsToBeTransferred->Insert(next);
    }

    // transfer events to new LPs
    while (!eventsToBeTransferred->IsEmpty())
    {
        Scheduler::Event ev = eventsToBeTransferred->RemoveNext();
        // invoke initialization events (at time 0) by their insertion order
        // since changing the execution order of these events may cause error,
        // they have to be invoked now rather than parallelly executed
        if (ev.key.m_ts == 0)
        {
            MtpInterface::GetSystem(ev.key.m_context == Simulator::NO_CONTEXT
                                        ? 0
                                        : NodeList::GetNode(ev.key.m_context)->GetSystemId())
                ->InvokeNow(ev);
        }
        else if (ev.key.m_context == Simulator::NO_CONTEXT)
        {
            Schedule(TimeStep(ev.key.m_ts), ev.impl);
        }
        else
        {
            ScheduleWithContext(ev.key.m_context, TimeStep(ev.key.m_ts), ev.impl);
        }
    }
}

} // namespace ns3
