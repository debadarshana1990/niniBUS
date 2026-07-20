#pragma once
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>


enum class CFIFOWriteStatus
{
    SUCCESS,
    Q_FULL,
    FAILED  //kept for future
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
    uint32_t credit;
};

struct CFIFOReadResult
{
    CFIFOReadStatus status;
    uint32_t PendingMessage;
};

template <typename T>
class cfifo
{
    public:
        using value_type = T;
        using size_type = std::uint32_t;
        using sequence_type = std::uint64_t;
        using cursor_type = std::uint32_t;

        explicit cfifo(size_type capacity):
        buffer_(capacity_),
        size_(0),
        capacity_(capacity),
        headSeq_(0),
        tailSeq_(0)
        {}

        CFIFOWriteResult write(const T& val)
        {
            if(full())
                return { CFIFOWriteStatus::Q_FULL, 0};
            sequence_type writeIdx = tailSeq_ % capacity_;
            buffer_[writeIdx] = val;
            tailSeq_++;
            size_++;
            return { CFIFOWriteStatus::SUCCESS,credit()};
            
        }
        CFIFOReadResult read(cursor_type idx, T& msg)
        {
            
            
            auto it = cursor_map_.find(idx);

            if (it == cursor_map_.end())
                return {CFIFOReadStatus::NO_CURSOR, 0};
            if(caught_up(it->second))
                return { CFIFOReadStatus::NO_PENDING_MESSAGE,0};
            sequence_type& readIdx = it->second;
            msg = buffer_[readIdx % capacity_];
            readIdx++;
            return { CFIFOReadStatus::SUCCESS,pending(readIdx)};


        }
        bool add_cursor(cursor_type idx)
        {
            cursor_map_[idx] = tailSeq_;   // read from the latest
            return true;
        }
        bool contains_cursor(cursor_type idx)
        {
            if(cursor_map_.find(idx) != cursor_map_.end())
                return true;
            return false;
        }

        /* global attribute of the buffer Q*/
        bool full() const
        {
            return (size_ == capacity_);
        }
        bool empty() const
        {
            return (size_ == 0);
        }
        uint32_t credit() const
        {
            return capacity_ - size_;
        }
        size_type size() const 
        {
            return size_;
        }
        size_type capacity() const
        {
            return capacity_;
        }
    private:
        std::vector<T> buffer_;
        std::unordered_map<cursor_type,sequence_type> cursor_map_;
        size_type size_;                 //size of the queue (all occupied slots)
        size_type capacity_;             // global capacity (bounded buffer)
        sequence_type headSeq_;              //global headSeq
        sequence_type tailSeq_;             //global tailseq used for writing into the queue

        bool claimSlots(); //TBD


        bool caught_up(sequence_type readSeq) const
        {
            return (readSeq == tailSeq_);
        }
        uint32_t pending(sequence_type readSeq) const
        {
            return tailSeq_ - readSeq;
        } 



};