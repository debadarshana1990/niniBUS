#pragma once
#include <vector>
#include <cstdint>
#include <iterator>
#include <stdexcept>
#include <unordered_map>


using SizeType = std::uint32_t;
using SequenceType = std::uint64_t;

enum class CFIFOWriteStatus
{
    SUCCESS,
    Q_FULL,
    FAILED  //kept for futur
};
enum class CFIFOReadStatus
{
    SUCCESS,
    NO_PENDING_MESSAGE,
    NO_CURSOR,
    FAILED  //kept for future
};
struct CFIFOWriteResult
{
    CFIFOWriteStatus status;
    SizeType credit;
};

struct CFIFOReadResult
{
    CFIFOReadStatus status;
    SizeType movedBy;
    SizeType pendingMessage;
};
//cursor
struct cursor
{
    SequenceType read_sequence;
    SizeType movedBy;
    bool was_reclaimed;
    cursor(SequenceType readSeq):read_sequence(readSeq), movedBy(0),was_reclaimed(false)
    {};

};
template <typename T>
class cfifo
{
    public:
        using value_type = T;


        using subscriber_type= std::uint32_t;

        explicit cfifo(SizeType capacity):
        buffer_(capacity),
        size_(0),
        capacity_(capacity),
        head_sequence_(0),
        tail_sequence_(0)
        {
            if (capacity == 0)
            {
                throw std::invalid_argument("cfifo capacity must be greater than zero");
            }
        }

        CFIFOWriteResult write(const T& val)
        {
            if(full() && !reclaim())
                return { CFIFOWriteStatus::Q_FULL, 0};
            const SizeType write_index = static_cast<SizeType>(tail_sequence_ % capacity_);
            buffer_[write_index] = val;
            tail_sequence_++;
            size_++;
            return { CFIFOWriteStatus::SUCCESS,credit()};
            
        }
        CFIFOReadResult read(subscriber_type idx, T& msg)
        {
            
            
            auto it = cursor_map_.find(idx);

            if (it == cursor_map_.end())
                return {CFIFOReadStatus::NO_CURSOR,0, 0};
            if(caught_up(it->second.read_sequence))
                return { CFIFOReadStatus::NO_PENDING_MESSAGE,0,0};

            auto& readCursor = it->second;
            SequenceType& readSeq = readCursor.read_sequence;
            const SequenceType read_index = static_cast<SizeType> (readSeq % capacity_);
            msg = buffer_[read_index];
            readSeq++;
            CFIFOReadResult ret;
            ret.movedBy = readCursor.movedBy;
            ret.pendingMessage = pending(readSeq);
            ret.status = CFIFOReadStatus::SUCCESS;
            if(readCursor.was_reclaimed)
            {
                readCursor.was_reclaimed = true;
                readCursor.movedBy = 0;
            }
            return ret;


        }
        bool create_cursor(subscriber_type idx)
        {
            auto [_,inserted] = cursor_map_.try_emplace(idx,tail_sequence_);
            return inserted;
        }
        bool contains_cursor(subscriber_type idx) const
        {
            return cursor_map_.find(idx) != cursor_map_.end();
        }
        bool remove_cursor(subscriber_type idx)
        {
            if(cursor_map_.find(idx) == cursor_map_.end())
                return false;
            cursor_map_.erase(idx);
            // not reclaiming the cursr from the queue. the reclaim will do it anyway
            return true;
        }



        /* global attribute of the buffer Q*/
        bool full() const
        {
            return (size_ >= capacity_);
        }
        bool empty() const
        {
            return (size_ == 0);
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
        std::vector<T> buffer_;
        std::unordered_map<subscriber_type,cursor> cursor_map_; // sunbscriberID ->current readSeq
        SizeType size_;                 //size of the queue (all occupied slots)
        SizeType capacity_;             // global capacity (bounded buffer)
        SequenceType head_sequence_;              //// oldest retained sequence; advanced by future reclaim()
        SequenceType tail_sequence_;             //global tailseq used for writing into the queue

       bool reclaim()
        {
            //its interesting , we will find the minimum cursor value of the idx and claim the head till there.
            //the cursor value of the idx will go to tail_seq , which will now recive the latest data
            // old thing should not last forever. whats the point reading the old value, if world has moved beyond

            if (cursor_map_.empty())
                return false;

            while (size_ >= capacity_)
            {
                auto minIdx = get_slowest_cursor_id();
                auto& minCursor = cursor_map_.at(minIdx);

                minCursor.movedBy = static_cast<SizeType>(
                    tail_sequence_ - minCursor.read_sequence);
                minCursor.read_sequence = tail_sequence_;
                minCursor.was_reclaimed = true;

                auto nextMinIdx = get_slowest_cursor_id();
                head_sequence_ = cursor_map_.at(nextMinIdx).read_sequence;
                size_ = static_cast<SizeType>(
                    tail_sequence_ - head_sequence_);
            }

            return true;
        }


        bool caught_up(SequenceType readSeq) const
        {
            return (readSeq == tail_sequence_);
        }
        SizeType pending(SequenceType readSeq) const
        {
            return tail_sequence_ - readSeq;
        }

        subscriber_type get_slowest_cursor_id()
        {
            auto slowest = cursor_map_.begin();
            for (auto it = std::next(cursor_map_.begin());
                 it != cursor_map_.end();
                 ++it)
            {
                if (it->second.read_sequence < slowest->second.read_sequence)
                {
                    slowest = it;
                }
            }
            return slowest->first;
        }



};
