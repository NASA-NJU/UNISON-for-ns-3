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
 *  Declaration of classes ns3::MtpInterface
 */

#ifndef MTP_INTERFACE_H
#define MTP_INTERFACE_H

#include "logical-process.h"

#include "ns3/atomic-counter.h"
#include "ns3/global-value.h"
#include "ns3/nstime.h"
#include "ns3/simulator.h"

#include <pthread.h>

namespace ns3
{

/**
 * @brief
 * Implementation of the interface for multithreaded parallel simulation.
 */
class MtpInterface
{
  public:
    /**
     * @brief
     * Implementation of the critical section based on spln lock via
     * atomic store & exchange.
     */
    class CriticalSection
    {
      public:
        /** Default constructor, using a globally shared atomic variable */
        inline CriticalSection()
            : m_spinLock(&g_inCriticalSection)
        {
            while (m_spinLock->exchange(true, std::memory_order_acquire))
            {
            };
        }

        /**
         * @brief Construct a new critical section object using a custom
         * atomic variable.
         *
         * @param lock Custom boolean atomic variable act as a spin lock
         */
        inline CriticalSection(std::atomic<bool>* lock)
            : m_spinLock(lock)
        {
            while (m_spinLock->exchange(true, std::memory_order_acquire))
            {
            };
        }

        /** Destructor */
        inline ~CriticalSection()
        {
            m_spinLock->store(false, std::memory_order_release);
        }

      private:
        std::atomic<bool>* m_spinLock;
    };

    /**
     * @brief Enable the multithreaded simulation, the number of threads
     * will be automatically chosen and the partition is also automatic.
     */
    static void Enable();

    /**
     * @brief Enable the multithreaded simulation, the number of threads
     * will be manually set and the partition is automatic.
     *
     * @param threadCount The number of threads to be used.
     */
    static void Enable(const uint32_t threadCount);

    /**
     * @brief Enable the multithreaded simulation, the number of threads
     * will be manually set and the partition is also done manually (by
     * assigning each node a systemId).
     *
     * @param threadCount The number of threads to be used.
     * @param systemCount The number of partitions.
     */
    static void Enable(const uint32_t threadCount, const uint32_t systemCount);

    /**
     * @brief Create new LPs and enable them.
     *
     * This method can be used to dynamically create LPs for dynamically
     * created nodes. After this operation, newly added LP must set their
     * scheduler before running.
     *
     * @param newSystemCount The number of newly to-be-created LPs.
     */
    static void EnableNew(const uint32_t newSystemCount);

    /**
     * @brief Create new LPs and enable them, while adjusting number of
     * threads the simulator will use.
     *
     * This method is called after the automatic partition. Before the
     * automatic partition, there is only one LP, and we do not know the
     * number of threads to be used since it is related to the number of
     * LPs. Therefore, we have to adjust the number of threads and create
     * new LPs simultaneously.
     *
     * @param threadCount
     * @param newSystemCount
     */
    static void EnableNew(const uint32_t threadCount, const uint32_t newSystemCount);

    /**
     * @brief Disable the multithreaded simulation and free the memory
     * space of LPs and threads.
     *
     * This method is called by the multithreaded simulator and you do
     * not have to call it manually.
     */
    static void Disable();

    /**
     * @brief Running the LPs and threads.
     *
     * This method is called by Simulator::Run.
     */
    static void Run();

    /**
     * @brief Preparation before running the LPs and threads.
     *
     * This method is called by MtpInterface::Run. It will actually create
     * threads and prepare them to process LPs.
     */
    static void RunBefore();

    /**
     * @brief Process all events of all LPs in the current round.
     *
     * This method is called by MtpInterface::Run.
     */
    static void ProcessOneRound();

    /**
     * @brief Calculate the global smallest time to determine the next
     * time window of each LP.
     *
     * This method is called by MtpInterface::Run.
     */
    static void CalculateSmallestTime();

    /**
     * @brief Post actions after all LPs are finished.
     *
     * This method is called by MtpInterface::Run. It will let threads know
     * that we have done everything, and terminates them.
     */
    static void RunAfter();

    /**
     * @brief Whether this interface is enabled.
     *
     * @return true if it is enabled
     * @return false if it is not enabled
     */
    static bool isEnabled();

    /**
     * @brief Whether the topology is already partitioned.
     *
     * This method is called by the constructor of the multithreaded simulator
     * to check whether user has already manually partitioned the topology.
     *
     * @return true if it is partitioned
     * @return false if it is not partitioned
     */
    static bool isPartitioned();

    /**
     * @brief Calculate the lookahead value of every LP.
     *
     * This method is called by MtpInterface::RunBefore.
     */
    static void CalculateLookAhead();

    /**
     * @brief Get the running logical process of the current thread.
     *
     * @return The curretly running logical process of the
     * current thread
     */
    inline static LogicalProcess* GetSystem()
    {
        return static_cast<LogicalProcess*>(pthread_getspecific(g_key));
    }

    /**
     * @brief Get the a logical process based on its ID.
     *
     * @param systemId The given ID of the logical process to be got
     * @return The corresponding logical process
     */
    inline static LogicalProcess* GetSystem(const uint32_t systemId)
    {
        return &g_systems[systemId];
    }

    /**
     * @brief Set the running logical process of the current thread.
     *
     * @param systemId The given ID of the logical process to be set
     */
    inline static void SetSystem(const uint32_t systemId)
    {
        pthread_setspecific(g_key, &g_systems[systemId]);
    }

    /**
     * @brief Get the total number of logical processes.
     *
     * @return The total number of logical processes, including
     * the public LP (whose ID is zero)
     */
    inline static uint32_t GetSize()
    {
        return g_systemCount + 1;
    }

    /**
     * @brief Get how many rounds are passed since the simulation starts.
     *
     * @return The number of rounds
     */
    inline static uint32_t GetRound()
    {
        return g_round;
    }

    /**
     * @brief Get the smallest timestamp of every to-be-processed event
     * of every LP.
     *
     * The smalles timestamp is used to calculate LBTS.
     *
     * @return The smallest timestamp.
     */
    inline static Time GetSmallestTime()
    {
        return g_smallestTime;
    }

    /**
     * @brief Set the smallest timestamp of every LP.
     *
     * This method is called by the hybrid simulator, where global MPI
     * communication may resulting in a smaller timestamp than the local
     * smallest timestamp, so we have to update the current smallest timestamp.
     *
     * @param smallestTime The new smallest timestamp
     */
    inline static void SetSmallestTime(const Time smallestTime)
    {
        g_smallestTime = smallestTime;
    }

    /**
     * @brief Get the timestamp of the next global event.
     *
     * The next global event's timestamp is also used to calculate LBTS.
     *
     * @return The timestamp of the next global event
     */
    inline static Time GetNextPublicTime()
    {
        return g_nextPublicTime;
    }

    /**
     * @brief Whether all LPs are finished all rounds (or terminated by
     * Simulator::Stop).
     *
     * @return true if all finished
     * @return false if not all finished
     */
    inline static bool isFinished()
    {
        return g_globalFinished;
    }

    /**
     * @brief Schedule a global event right after the current round is finished.
     */
    template <
        typename FUNC,
        typename std::enable_if<!std::is_convertible<FUNC, Ptr<EventImpl>>::value, int>::type,
        typename std::enable_if<!std::is_function<typename std::remove_pointer<FUNC>::type>::value,
                                int>::type,
        typename... Ts>
    inline static void ScheduleGlobal(FUNC f, Ts&&... args)
    {
        CriticalSection cs;
        g_systems[0].ScheduleAt(Simulator::NO_CONTEXT,
                                Min(g_smallestTime, g_nextPublicTime),
                                MakeEvent(f, std::forward<Ts>(args)...));
    }

    /**
     * @brief Schedule a global event right after the current round is finished.
     */
    template <typename... Us, typename... Ts>
    inline static void ScheduleGlobal(void (*f)(Us...), Ts&&... args)
    {
        CriticalSection cs;
        g_systems[0].ScheduleAt(Simulator::NO_CONTEXT,
                                Min(g_smallestTime, g_nextPublicTime),
                                MakeEvent(f, std::forward<Ts>(args)...));
    }

  private:
    /**
     * @brief The actual function each thread will run.
     *
     * In this function, each thread repeatedly get the next unprocessed LP,
     * execute it and wait until all LPs are processed.
     */
    static void* ThreadFunc(void* arg);

    /**
     * @brief Determine logical process priority by execution time.
     */
    static bool SortByExecutionTime(const uint32_t& i, const uint32_t& j);

    /**
     * @brief Determine logical process priority by event count.
     */
    static bool SortByEventCount(const uint32_t& i, const uint32_t& j);

    /**
     * @brief Determine logical process priority by pending event count.
     */
    static bool SortByPendingEventCount(const uint32_t& i, const uint32_t& j);

    /**
     * @brief Determine logical process priority by simulation time.
     */
    static bool SortBySimulationTime(const uint32_t& i, const uint32_t& j);

    static bool (*g_sortFunc)(const uint32_t&, const uint32_t&);
    static GlobalValue g_sortMethod;
    static GlobalValue g_sortPeriod;
    static uint32_t g_period;

    static pthread_t* g_threads;
    static LogicalProcess* g_systems;
    static uint32_t g_threadCount;
    static uint32_t g_systemCount;

    static uint32_t* g_sortedSystemIndices;
    static std::atomic<uint32_t> g_systemIndex;
    static std::atomic<uint32_t> g_finishedSystemCount;

    static uint32_t g_round;
    static Time g_smallestTime;
    static Time g_nextPublicTime;
    static bool g_recvMsgStage;
    static bool g_globalFinished;
    static bool g_enabled;

    static pthread_key_t g_key;
    static std::atomic<bool> g_inCriticalSection;
};

} // namespace ns3

#endif /* MTP_INTERFACE_H */
