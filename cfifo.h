#pragma once
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>

using cursorSeq_t = uint64_t;

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
    private:
        std::vector<T> buffer_;
        std::unordered_map<uint32_t,cursorSeq_t> cursor_map_;
        uint32_t size_;                 //size of the queue (all occupied slots)
        uint32_t capacity_;             // global capacity (bounded buffer)
        cursorSeq_t headSeq_;              //global headSeq
        cursorSeq_t tailSeq_;             //global tailseq used for writing into the queue

        bool claimSlots(); //TBD
        uint32_t credit() const
        {
            return capacity_ - size_;
        }

        bool full() const
        {
            return (size_ == capacity_);
        }
        bool cursorEmpty(cursorSeq_t readSeq) const
        {
            return (readSeq == tailSeq_);
        }
        uint32_t pendingMessage(cursorSeq_t readSeq) const
        {
            return tailSeq_ - readSeq;
        } 

    public:
        CFIFOWriteResult write(T &msg)
        {
            if(full())
                return { CFIFOWriteStatus::Q_FULL, 0};
            cursorSeq_t writeIdx = tailSeq_ % capacity_;
            buffer_[writeIdx] = msg;
            tailSeq_++;
            size_++;
            return { CFIFOWriteStatus::SUCCESS,credit()};
            
        }
        CFIFOReadResult Read(uint32_t idx, T& msg)
        {
            msg.clear();            // clear stray
            
            auto it = cursor_map_.find(idx);

            if (it == cursor_map_.end())
                return {CFIFOReadStatus::NO_CURSOR, 0};
            if(cursorEmpty())
                return { CFIFOReadStatus::NO_PENDING_MESSAGE,0};
            cursorSeq_t& readIdx = it->second;
            msg = buffer_[readIdx % capacity_];
            readIdx++;
            return { CFIFOReadStatus::SUCCESS,pendingMessage()};


        }
        bool AddCursor(uint32_t idx)
        {
            cursor_map_[idx] = tailSeq_;   // read from the latest
            return true;
        }
        bool HasCursor(uint32_t idx)
        {
            if(cursor_map_.find(idx) != cursor_map_.end())
                return true;
            return false;
        }
};