/********************************************************************
 * Copyright (c) Robert Bosch GmbH
 * All Rights Reserved.
 *
 * This file may not be distributed without the file ’license.txt’.
 * This file is subject to the terms and conditions defined in file
 * ’license.txt’, which is part of this source code package.
 *********************************************************************/
#ifndef UTIL_HPP
#define UTIL_HPP

#include <condition_variable>
#include <deque>
#include <mutex>
#include <queue>
#include <string>

namespace iotea {
namespace core {

std::string GetEnv(const std::string& name, const std::string& defval = "");

std::string GenerateUUID();

template <typename T>
class SyncQueue {
   private:
    std::mutex m_;
    std::condition_variable c_;
    std::deque<T> q_;

   public:
    explicit SyncQueue(size_t size)
        : q_{std::deque<T>(size)} {}

    void Push(const T& item) {
        {
            std::unique_lock<std::mutex> lock(m_);

            while (q_.size() == q_.max_size()) {
                c_.wait(lock);
            }

            q_.push_back(item);
        }

        c_.notify_all();
    }

    T Pop() {
        T item;
        {
            std::unique_lock<std::mutex> lock(m_);
            while (q_.empty()) {
                c_.wait(lock);
            }

            item = q_.front();
            q_.pop_front();
        }

        c_.notify_all();

        return item;
    }

    bool Empty() {
        std::lock_guard<std::mutex> lock(m_);
        return q_.empty();
    }

    size_t Size() {
        std::lock_guard<std::mutex> lock(m_);
        return q_.size();
    }
};

}  // namespace core
}  // namespace iotea

#endif  // UTIL_HPP
