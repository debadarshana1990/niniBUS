#pragma once
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>


using size_type = std::uint32_t;

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
    size_type credit;
};

struct CFIFOReadResult
{
    CFIFOReadStatus status;
    size_type pendingMessage;
};

template <typename T>
class cfifo
{
    public:
        using value_type = T;

        using sequence_type = std::uint64_t;
        using cursor_type = std::uint32_t;

        explicit cfifo(size_type capacity):
        buffer_(capacity),
        size_(0),
        capacity_(capacity),
        headSeq_(0),
        tailSeq_(0)
        {
            if (capacity == 0)
            {
                throw std::invalid_argument("cfifo capacity must be greater than zero");
            }
        }

        CFIFOWriteResult write(const T& val)
        {
            if(full())
                return { CFIFOWriteStatus::Q_FULL, 0};
            const size_type write_index = static_cast<size_type>(tailSeq_ % capacity_);
            buffer_[write_index] = val;
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
            sequence_type& read_seq = it->second;
            const sequence_type read_index = static_cast<size_type> (read_seq % capacity_);
            msg = buffer_[read_index];
            read_seq++;
            return { CFIFOReadStatus::SUCCESS,pending(read_seq)};


        }
        bool add_cursor(cursor_type idx)
        {
            auto [_,inserted] = cursor_map_.try_emplace(idx);
            return inserted;
        }
        bool contains_cursor(cursor_type idx) const
        {
            return cursor_map_.find(idx) != cursor_map_.end();
  
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
        size_type credit() const
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
        sequence_type headSeq_;              //// oldest retained sequence; advanced by future reclaim()
        sequence_type tailSeq_;             //global tailseq used for writing into the queue

        bool reclaim(); //TBD


        bool caught_up(sequence_type readSeq) const
        {
            return (readSeq == tailSeq_);
        }
        uint32_t pending(sequence_type readSeq) const
        {
            return tailSeq_ - readSeq;
        } 



};