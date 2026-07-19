#pragma once
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>


enum class CFIFOStatus
{
    SUCCESS,
    FULL,
    EMPTY
};


template <typename T>
class niniCFIFO
{
    private:
        struct CursorState
        {
            uint32_t position;
            uint32_t messages_read;
        };

        std::vector<T> buffer_;
        uint32_t tail_;
        uint32_t capacity_;
        uint32_t size_;
        std::unordered_map<uint32_t, CursorState> cursor_map_;
    public:
        explicit niniCFIFO(uint32_t capacity) : buffer_(capacity), tail_(0), capacity_(capacity), size_(0) {}
        CFIFOStatus push(const T& message)
        {
            if(full())
                return CFIFOStatus::FULL;
            buffer_[tail_] = message;
            tail_ = (tail_ + 1) % capacity_;
            size_++;
            return CFIFOStatus::SUCCESS;
        }

        CFIFOStatus read(uint32_t id, T& message)
        {
            auto [it, inserted] = cursor_map_.try_emplace(id, CursorState{0, 0});
            (void)inserted;

            if(it->second.messages_read >= size_)
                return CFIFOStatus::EMPTY;

            message = buffer_[it->second.position];
            it->second.position = (it->second.position + 1) % capacity_;
            it->second.messages_read++;
            return CFIFOStatus::SUCCESS;
        }

        bool hasCursor(uint32_t id) const
        {
            return cursor_map_.find(id) != cursor_map_.end();
        }
        bool addCursor(uint32_t id)
        {
            if(hasCursor(id))
                return false;
            cursor_map_[id] = CursorState{0, 0};
            return true;
        }


        bool full() const { return size_ == capacity_; }
        bool empty() const { return size_ == 0; }
        uint32_t size() const { return size_; }
        uint32_t capacity() const { return capacity_; }
        uint32_t pending(uint32_t id) const
        {
            auto it = cursor_map_.find(id);
            if (it == cursor_map_.end())
                throw std::runtime_error("Cursor not found");
            return size_ - it->second.messages_read;
        }
};
