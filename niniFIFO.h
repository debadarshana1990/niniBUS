#pragma once
#include <cstdint>
#include <stdexcept>
#include <vector>


enum class FIFOStatus
{
    SUCCESS,
    FULL,
    EMPTY
};

template <typename T>
class niniFIFO
{
    private:
        uint32_t capacity_;
        std::vector<T> buffer_;
        uint32_t head_;
        uint32_t tail_;
        uint32_t size_;
    public:
    //public API
    // Precondition: capacity > 0.
    // niniBUS::createLane() validates application-provided capacity.
    explicit niniFIFO(uint32_t capacity) : capacity_(capacity), buffer_(capacity), head_(0), tail_(0), size_(0)
    {
    }

    FIFOStatus push_back(const T& message)
    {
        if(full())
            return FIFOStatus::FULL;
        buffer_[tail_] = message;
        tail_ = (tail_ + 1) % capacity_;
        size_++;
        return FIFOStatus::SUCCESS;
    }

    FIFOStatus pop_front()
    {
        if(empty())
            return FIFOStatus::EMPTY;
        head_ = (head_ + 1) % capacity_;
        size_--;
        return FIFOStatus::SUCCESS;
    }
    
    T& front()
    {
        if (empty())
            throw std::runtime_error("FIFO is empty");
        return buffer_[head_];
    }
    
    //helper functions
    bool empty() const {return (size_ == 0);}
    bool full() const {return (size_ == capacity_);}
    uint32_t size() const {return size_;}
    uint32_t capacity() const { return capacity_; }
};
