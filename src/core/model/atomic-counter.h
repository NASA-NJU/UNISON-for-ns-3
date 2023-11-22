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

#ifndef ATOMIC_COUNTER_H
#define ATOMIC_COUNTER_H

#include <atomic>

namespace ns3
{

/**
 * @brief
 * The implementation of the atomic counter used for reference counting.
 *
 * It overrides operators of existing atomic variables with a more relaxed
 * memory order to improve reference counting performance.
 */
class AtomicCounter
{
  public:
    /** Constructor */
    inline AtomicCounter()
    {
        m_count.store(0, std::memory_order_release);
    }

    /**
     * @brief Construct a new atomic counter object.
     * 
     * @param count The initialization count number
     */
    inline AtomicCounter(uint32_t count)
    {
        m_count.store(count, std::memory_order_release);
    }

    /**
     * @brief Read the counter value with a more relaxed memory order.
     * 
     * @return The counter value.
     */
    inline operator uint32_t() const
    {
        return m_count.load(std::memory_order_acquire);
    }

    /**
     * @brief Set the counter value with a more relaxed memory order.
     * 
     * @param count The counter value to be set
     * @return The counter value to be set
     */
    inline uint32_t operator=(const uint32_t count)
    {
        m_count.store(count, std::memory_order_release);
        return count;
    }

    /**
     * @brief Increment the counter by one with a more relaxed memory order.
     * 
     * @return The old counter value
     */
    inline uint32_t operator++(int)
    {
        return m_count.fetch_add(1, std::memory_order_relaxed);
    }

    /**
     * @brief Decrement the counter by one with a more relaxed memory order.
     * 
     * @return The old counter value
     */
    inline uint32_t operator--(int)
    {
        return m_count.fetch_sub(1, std::memory_order_release);
    }

  private:
    std::atomic<uint32_t> m_count;
};

} // namespace ns3

#endif /* ATOMIC_COUNTER_H */
