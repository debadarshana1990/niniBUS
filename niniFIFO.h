#pragma once
#include <cstdint>
#include <array>
#include <stdexcept>
#include <vector>

#define DEFAULT_LANE_CAPACITY (uint32_t)10 //niniFIFO should holds the default value of the queue capacity not the lane

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
        std::vector<T> buffer_;
        uint32_t capacity_;
        uint32_t head_;
        uint32_t tail_;
        uint32_t currSize_;
    public:
    //public API
    niniFIFO() : capacity_(DEFAULT_LANE_CAPACITY), head_(0), tail_(0), currSize_(0)
    {
        buffer_.resize(capacity_);
    }

    FIFOStatus push_back(const T& message)
    {
        if(isFull())
            return FIFOStatus::FULL;
        buffer_[tail_] = message;
        tail_ = (tail_ + 1) % capacity_;
        currSize_++;
        return FIFOStatus::SUCCESS;
    }

    FIFOStatus pop_front()
    {
        if(isEmpty())
            return FIFOStatus::EMPTY;
        head_ = (head_ + 1) % capacity_;
        currSize_--;
        return FIFOStatus::SUCCESS;
    }
    
    T& front()
    {
        if (isEmpty())
            throw std::runtime_error("FIFO is empty");
        return buffer_[head_];
    }
    
    //helper functions
    bool isEmpty() const {return (currSize_ == 0);}
    bool isFull() const {return (currSize_ == capacity_);}
    uint32_t size() const {return currSize_;}
    uint32_t getCapacity() const { return capacity_; }
};
