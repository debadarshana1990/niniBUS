#pragma once

#include <array>
#include <cstdint>
#include <iterator>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace nbus
{

using SizeType = std::uint32_t;
using SequenceType = std::uint64_t;

enum class CFIFOReadStatus
{
    SUCCESS,
    NO_PENDING_MESSAGE,
    NO_CURSOR
};

struct CFIFOWriteResult
{
    SequenceType sequence_id{0};
    SizeType credit{0};
};

struct CFIFOReadResult
{
    CFIFOReadStatus status{CFIFOReadStatus::NO_PENDING_MESSAGE};
    SizeType pending_messages{0};
    SequenceType sequence_id{0};
    SequenceType skipped_messages{0};
};

template <typename T>
class cfifo
{
    public:
        using value_type = T;
        using subscriber_type = std::uint32_t;

        explicit cfifo(SizeType capacity)
            : capacity_(validate_capacity(capacity)),
#ifndef NBUS_CFIFO_USE_ARRAY
              buffer_(capacity_),
#endif
              size_(0),
              head_sequence_(0),
              tail_sequence_(0)
        {
        }

        CFIFOWriteResult write(const T& value)
        {
            if (full())
                reclaim();

            const SequenceType sequence_id = tail_sequence_;
            const SizeType write_index =
                static_cast<SizeType>(sequence_id % capacity_);

            buffer_[write_index] = value;
            ++tail_sequence_;
            ++size_;
            return {sequence_id, credit()};
        }

        CFIFOReadResult read(subscriber_type id, T& message)
        {
            auto it = cursor_map_.find(id);

            if (it == cursor_map_.end())
                return {CFIFOReadStatus::NO_CURSOR, 0, 0, 0};
            if (caught_up(it->second.read_sequence))
                return {CFIFOReadStatus::NO_PENDING_MESSAGE, 0, 0, 0};

            auto& read_cursor = it->second;
            SequenceType& read_sequence = read_cursor.read_sequence;
            const SizeType read_index =
                static_cast<SizeType>(read_sequence % capacity_);
            message = buffer_[read_index];
            const SequenceType sequence_id = read_sequence;
            ++read_sequence;

            const CFIFOReadResult result{
                CFIFOReadStatus::SUCCESS,
                pending(read_sequence),
                sequence_id,
                read_cursor.skipped_messages
            };

            // Skipped messages are reported once, with the next successful read.
            read_cursor.skipped_messages = 0;
            return result;
        }

        SequenceType create_cursor(subscriber_type id)
        {
            // New subscribers receive only messages written after registration.
            const auto cursor =
                cursor_map_.try_emplace(id, tail_sequence_);
            return cursor.first->second.read_sequence;
        }

        bool contains_cursor(subscriber_type id) const
        {
            return cursor_map_.find(id) != cursor_map_.end();
        }

        bool remove_cursor(subscriber_type id)
        {
            // Storage is reclaimed lazily by the next full-queue write.
            return cursor_map_.erase(id) != 0;
        }

        // These accessors describe shared retained storage, not one cursor.
        bool full() const
        {
            return size_ == capacity_;
        }

        bool empty() const
        {
            return size_ == 0;
        }

        SizeType credit() const
        {
            return capacity_ - size_;
        }

        SizeType size() const
        {
            return size_;
        }

        SizeType capacity() const
        {
            return capacity_;
        }

    private:
        static SizeType validate_capacity(SizeType capacity)
        {
            if (capacity == 0)
            {
                throw std::invalid_argument(
                    "cfifo capacity must be greater than zero");
            }
            return capacity;
        }

        struct CursorState
        {
            explicit CursorState(SequenceType sequence)
                : read_sequence(sequence)
            {
            }

            SequenceType read_sequence;
            SequenceType skipped_messages{0};
        };

        SizeType capacity_;  // Fixed number of physical buffer slots.
#ifdef NBUS_CFIFO_USE_ARRAY
        std::array<T, 10000000> buffer_;
#else
        std::vector<T> buffer_;
#endif
        // Subscriber ID -> next unread global sequence.
        std::unordered_map<subscriber_type, CursorState> cursor_map_;
        SizeType size_;  // Number of retained shared-buffer slots.
        SequenceType head_sequence_;  // Oldest retained global sequence.
        SequenceType tail_sequence_;  // Next global sequence to write.

        void reclaim()
        {
            // Without subscribers, no cursor can reference retained history.
            if (cursor_map_.empty())
            {
                head_sequence_ = tail_sequence_;
                size_ = 0;
                return;
            }

            const subscriber_type minimum_subscriber =
                get_slowest_cursor_id();
            const SequenceType minimum_sequence =
                cursor_map_.at(minimum_subscriber).read_sequence;

            // If the minimum cursor is at the tail, every cursor is caught up.
            if (minimum_sequence == tail_sequence_)
            {
                head_sequence_ = tail_sequence_;
                size_ = 0;
                return;
            }

            // Advance every cursor tied at the oldest sequence so the tied
            // group cannot keep the shared queue full.
            for (auto& entry : cursor_map_)
            {
                auto& cursor_state = entry.second;
                if (cursor_state.read_sequence == minimum_sequence)
                {
                    // Accumulate skips until this subscriber reads again.
                    cursor_state.skipped_messages +=
                        tail_sequence_ - cursor_state.read_sequence;
                    cursor_state.read_sequence = tail_sequence_;
                }
            }

            // A more advanced cursor may now define the oldest retained data.
            const subscriber_type next_minimum_subscriber =
                get_slowest_cursor_id();
            head_sequence_ =
                cursor_map_.at(next_minimum_subscriber).read_sequence;
            size_ = static_cast<SizeType>(
                tail_sequence_ - head_sequence_);
        }

        bool caught_up(SequenceType read_sequence) const
        {
            return read_sequence == tail_sequence_;
        }

        SizeType pending(SequenceType read_sequence) const
        {
            return static_cast<SizeType>(
                tail_sequence_ - read_sequence);
        }

        subscriber_type get_slowest_cursor_id() const
        {
            // Precondition: cursor_map_ is not empty.
            auto slowest = cursor_map_.begin();
            for (auto it = std::next(cursor_map_.begin());
                 it != cursor_map_.end();
                 ++it)
            {
                if (it->second.read_sequence <
                    slowest->second.read_sequence)
                {
                    slowest = it;
                }
            }
            return slowest->first;
        }
};

} // namespace nbus
