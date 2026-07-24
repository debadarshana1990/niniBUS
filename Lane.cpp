#include "Lane.h"
#include "cfifo.h"

#include <stdexcept>

namespace
{

ReceiveStatus to_receive_status(nbus::CFIFOReadStatus status)
{
    switch (status)
    {
        case nbus::CFIFOReadStatus::SUCCESS:
            return ReceiveStatus::SUCCESS;
        case nbus::CFIFOReadStatus::NO_PENDING_MESSAGE:
            return ReceiveStatus::NO_PENDING_MESSAGE;
        case nbus::CFIFOReadStatus::NO_CURSOR:
            return ReceiveStatus::NO_CURSOR;
    }

    throw std::logic_error("Unknown CFIFOReadStatus");
}

}

PublishResult lane::push(const std::string& message)
{
    auto result = content_.write(message);
    return { result.credit, static_cast<sequenceId_t>(result.sequence_id) };
}

ReceiveResult lane::pop(subscribeID_t subscribeID,std::string& message)
{
   
    auto result = content_.read(subscribeID, message);
    auto status = to_receive_status(result.status);
    return { status, result.pending_messages, 
        static_cast<sequenceId_t>(result.sequence_id), static_cast<uint64_t> (result.skipped_messages) };
}

SubscribeResult lane::subscribe(subscribeID_t subscribeID)
{
    auto sequenceId = content_.create_cursor(subscribeID);
    return { SubscribeStatus::Ok, static_cast<sequenceId_t>(sequenceId) };

}
bool lane::unsubscribe(subscribeID_t subscribeID)
{
    return content_.remove_cursor(subscribeID);
}
