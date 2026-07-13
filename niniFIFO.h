#pragma once
#include <cstdint>
#include <array>
#include <stdexcept>

enum class FIFOStatus
{
    SUCCESS,
    FULL,
    EMPTY
};

template <typename T, uint32_t CAPACITY>
class niniFIFO_t
{
    private:
        std::array<T, CAPACITY> buffer; // Fixed-size array for FIFO
        uint32_t head;
        uint32_t tail;
        uint32_t currSize;
    public:





    //public API
    niniFIFO_t() : head(0), tail(0), currSize(0)
    {
        buffer.fill(T());
    }
    ~niniFIFO_t() = default;

    FIFOStatus push_back(const T& message)
    {
        if(isFull())
            return FIFOStatus::FULL;
        buffer[tail] = message;
        tail = (tail + 1) % CAPACITY;
        currSize++;
        return FIFOStatus::SUCCESS;
    }

    void pop_front()
    {
        if(isEmpty())
            throw std::runtime_error("FIFO is empty");
        head = (head + 1) % CAPACITY;
        currSize--;
    }
    
    T& front() const
    {
        if (isEmpty())
            throw std::runtime_error("FIFO is empty");
        return const_cast<T&>(buffer[head]);
    }
    
    //helper functions
    bool isEmpty() const {return (currSize == 0);}
    bool isFull() const {return (currSize == CAPACITY);}
    uint32_t size() const {return currSize;}
    uint32_t getCapacity() const { return CAPACITY; }
};
