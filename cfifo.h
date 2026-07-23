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
    Q_EMPTY,
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
    SizeType pendingMessage;
    SequenceType sequenceID;
    SizeType movedBy;
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
            if (capacity <= 0)
            {
                throw std::invalid_argument("cfifo capacity must be greater than zero");
            }
        }

        CFIFOWriteResult write(const T& val)
        {
            if(full() && !reclaim())
                return { CFIFOWriteStatus::Q_EMPTY, 0}; //reclaim failed means there is data in Q., for future cases
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
                return {CFIFOReadStatus::NO_CURSOR, 0, 0, 0};
            if(caught_up(it->second.read_sequence))
                return {CFIFOReadStatus::NO_PENDING_MESSAGE, 0, 0, 0};

            auto& readCursor = it->second;
            SequenceType& readSeq = readCursor.read_sequence;
            const SequenceType read_index = static_cast<SizeType> (readSeq % capacity_);
            msg = buffer_[read_index];
            const SequenceType sequenceID = readSeq;
            readSeq++;
            CFIFOReadResult ret;
            ret.pendingMessage = pending(readSeq);
            ret.sequenceID = sequenceID;
            ret.movedBy = readCursor.movedBy;
            ret.status = CFIFOReadStatus::SUCCESS;
            if(readCursor.was_reclaimed)
            {
                readCursor.was_reclaimed = false;
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

            //if no subscriber <cursor>, which means we claim safely move head to tail
            if (cursor_map_.empty())
            {
                 head_sequence_ = tail_sequence_;
                 size_ = 0;
                 return true;
             }


            // 1. get the minCur ,if cursor is at tailindex, claim everything
             subscriber_type minSub = get_slowest_cursor_id();
             auto minCur = cursor_map_.at(minSub).read_sequence;


            if (minCur == tail_sequence_)
            {
                // claim everything
                head_sequence_ = tail_sequence_;
                size_ = 0;
                return true;
            }
            //2. get the minimum cursor anf find if all other cursor at same location, move there cursor to tail_sequence
            // and update the movedBy from current readseq to tailseq  and current cursor to tailsequence ,which is the latest
            for (auto& [id, cursor] : cursor_map_)
            {
                if ( cursor.read_sequence == minCur)
                {
                    // read_sequence is the next unread message and
                    // tail_sequence_ is the next write position. Therefore,
                    // [read_sequence, tail_sequence_) contains exactly this
                    // many skipped messages; no +1 is required.
                    cursor.movedBy = static_cast<SizeType>(
                        tail_sequence_ - cursor.read_sequence);
                    cursor.read_sequence = tail_sequence_;
                    cursor.was_reclaimed = true;
                }
            }
            //get the next miimum cursor and move my head sequence to readseq -1 ,menas claim till it and update the size
            subscriber_type nextMinSub = get_slowest_cursor_id();
            auto nextMinCur = cursor_map_.at(nextMinSub).read_sequence;
            head_sequence_ = nextMinCur;
            size_ = static_cast<SizeType>(
                tail_sequence_ - head_sequence_);
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
