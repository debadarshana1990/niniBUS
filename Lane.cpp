#include "Lane.h"

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

PublishResult Lane::push(const std::string& message)
{
    const auto result = content_.write(message);
    return {
        result.credit,
        static_cast<sequenceId_t>(result.sequence_id)
    };
}

ReceiveResult Lane::pop(
    subscriberID_t subscriberID,
    std::string& message)
{
    const auto result = content_.read(subscriberID, message);
    const auto status = to_receive_status(result.status);
    return {
        status,
        result.pending_messages,
        static_cast<sequenceId_t>(result.sequence_id),
        static_cast<std::uint64_t>(result.skipped_messages)
    };
}

SubscribeResult Lane::subscribe(subscriberID_t subscriberID)
{
    const auto sequenceId = content_.create_cursor(subscriberID);
    return {
        SubscribeStatus::Ok,
        static_cast<sequenceId_t>(sequenceId)
    };
}

bool Lane::unsubscribe(subscriberID_t subscriberID)
{
    return content_.remove_cursor(subscriberID);
}
