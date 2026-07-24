#include <cassert>
#include <cstdint>
#include <iostream>
#include <string>

#include "niniBUS.h"

namespace
{

void print_pass(const std::string& name)
{
    std::cout << "[PASS] " << name << '\n';
}

ReceiveResult receive_and_expect(
    niniBUS& bus,
    laneID_t lane_id,
    subscriberID_t subscriber_id,
    std::string& message,
    ReceiveStatus expected_status,
    std::uint32_t expected_pending)
{
    const auto result = bus.receive(lane_id, subscriber_id, message);
    assert(result.status == expected_status);
    assert(result.pendingMessages == expected_pending);
    return result;
}

void test_create_lane_and_subscribe_statuses()
{
    niniBUS bus;

    const auto missing = bus.subscribe(1, 100);
    assert(missing.status == SubscribeStatus::LaneNotExist);
    assert(missing.nextSequenceId == 0);
    assert(bus.createLane(1, 3) == CreateLaneStatus::Ok);
    assert(bus.createLane(1, 8) == CreateLaneStatus::LaneExists);
    assert(bus.createLane(2, 0) == CreateLaneStatus::InvalidCapacity);
    assert(bus.createLane(2, 1) == CreateLaneStatus::Ok);
    const auto subscribed = bus.subscribe(1, 100);
    assert(subscribed.status == SubscribeStatus::Ok);
    assert(subscribed.nextSequenceId == 0);

    print_pass("lane creation and subscription statuses");
}

void test_publish_and_receive_sequence()
{
    niniBUS bus;
    std::string message;

    assert(bus.createLane(20, 4) == CreateLaneStatus::Ok);
    assert(bus.subscribe(20, 200).status == SubscribeStatus::Ok);

    const auto first = bus.publish(20, "first");
    const auto second = bus.publish(20, "second");
    const auto third = bus.publish(20, "third");

    assert(first.sequenceId == 0);
    assert(first.credit == 3);
    assert(second.sequenceId == 1);
    assert(second.credit == 2);
    assert(third.sequenceId == 2);
    assert(third.credit == 1);

    const auto received_first = receive_and_expect(
        bus, 20, 200, message, ReceiveStatus::SUCCESS, 2);
    assert(received_first.sequenceId == 0);
    assert(received_first.skippedMessages == 0);
    assert(message == "first");

    const auto received_second = receive_and_expect(
        bus, 20, 200, message, ReceiveStatus::SUCCESS, 1);
    assert(received_second.sequenceId == 1);
    assert(message == "second");

    const auto received_third = receive_and_expect(
        bus, 20, 200, message, ReceiveStatus::SUCCESS, 0);
    assert(received_third.sequenceId == 2);
    assert(message == "third");

    message = "stale";
    receive_and_expect(
        bus, 20, 200, message, ReceiveStatus::NO_PENDING_MESSAGE, 0);
    assert(message.empty());

    print_pass("publish and receive preserve sequence and pending counts");
}

void test_multiple_subscribers_receive_same_messages()
{
    niniBUS bus;
    std::string message;

    assert(bus.createLane(30, 3) == CreateLaneStatus::Ok);
    assert(bus.subscribe(30, 300).status == SubscribeStatus::Ok);
    assert(bus.subscribe(30, 301).status == SubscribeStatus::Ok);
    assert(bus.publish(30, "A").sequenceId == 0);
    assert(bus.publish(30, "B").sequenceId == 1);

    const auto subscriber_300 = receive_and_expect(
        bus, 30, 300, message, ReceiveStatus::SUCCESS, 1);
    assert(subscriber_300.sequenceId == 0);
    assert(message == "A");

    const auto subscriber_301 = receive_and_expect(
        bus, 30, 301, message, ReceiveStatus::SUCCESS, 1);
    assert(subscriber_301.sequenceId == 0);
    assert(message == "A");

    receive_and_expect(bus, 30, 300, message, ReceiveStatus::SUCCESS, 0);
    assert(message == "B");
    receive_and_expect(bus, 30, 301, message, ReceiveStatus::SUCCESS, 0);
    assert(message == "B");

    print_pass("multiple subscribers independently receive shared messages");
}

void test_late_subscriber_starts_at_current_tail()
{
    niniBUS bus;
    std::string message;

    assert(bus.publish(40, "old").sequenceId == 0);
    assert(bus.subscribe(40, 400).status == SubscribeStatus::Ok);
    receive_and_expect(
        bus, 40, 400, message, ReceiveStatus::NO_PENDING_MESSAGE, 0);

    assert(bus.publish(40, "new").sequenceId == 1);
    const auto result = receive_and_expect(
        bus, 40, 400, message, ReceiveStatus::SUCCESS, 0);
    assert(result.sequenceId == 1);
    assert(message == "new");

    print_pass("late subscribers receive only future messages");
}

void test_receive_missing_lane_or_cursor()
{
    niniBUS bus;
    std::string message = "stale";

    receive_and_expect(bus, 50, 500, message, ReceiveStatus::NO_CURSOR, 0);
    assert(message.empty());

    assert(bus.createLane(50, 2) == CreateLaneStatus::Ok);
    message = "stale-again";
    receive_and_expect(bus, 50, 500, message, ReceiveStatus::NO_CURSOR, 0);
    assert(message.empty());

    print_pass("receive reports missing lanes and subscribers as NO_CURSOR");
}

void test_reclaim_reports_skips_and_preserves_faster_subscriber()
{
    niniBUS bus;
    std::string message;

    assert(bus.createLane(60, 3) == CreateLaneStatus::Ok);
    assert(bus.subscribe(60, 600).status == SubscribeStatus::Ok);
    assert(bus.subscribe(60, 601).status == SubscribeStatus::Ok);
    assert(bus.publish(60, "A").sequenceId == 0);
    assert(bus.publish(60, "B").sequenceId == 1);
    assert(bus.publish(60, "C").sequenceId == 2);

    receive_and_expect(bus, 60, 600, message, ReceiveStatus::SUCCESS, 2);
    assert(message == "A");

    const auto newest = bus.publish(60, "D");
    assert(newest.sequenceId == 3);
    assert(newest.credit == 0);

    const auto faster = receive_and_expect(
        bus, 60, 600, message, ReceiveStatus::SUCCESS, 2);
    assert(faster.sequenceId == 1);
    assert(faster.skippedMessages == 0);
    assert(message == "B");

    const auto reclaimed = receive_and_expect(
        bus, 60, 601, message, ReceiveStatus::SUCCESS, 0);
    assert(reclaimed.sequenceId == 3);
    assert(reclaimed.skippedMessages == 3);
    assert(message == "D");

    print_pass("reclaim reports skips and preserves faster subscribers");
}

void test_capacity_one_lane_keeps_latest_message()
{
    niniBUS bus;
    std::string message;

    assert(bus.createLane(70, 1) == CreateLaneStatus::Ok);
    assert(bus.subscribe(70, 700).status == SubscribeStatus::Ok);
    assert(bus.publish(70, "old").sequenceId == 0);
    assert(bus.publish(70, "latest").sequenceId == 1);

    const auto result = receive_and_expect(
        bus, 70, 700, message, ReceiveStatus::SUCCESS, 0);
    assert(result.sequenceId == 1);
    assert(result.skippedMessages == 1);
    assert(message == "latest");

    print_pass("capacity-one lane keeps the latest message");
}

void test_full_lane_without_subscribers_reclaims_history()
{
    niniBUS bus;
    std::string message;

    assert(bus.createLane(80, 2) == CreateLaneStatus::Ok);
    assert(bus.publish(80, "A").sequenceId == 0);
    assert(bus.publish(80, "B").sequenceId == 1);

    const auto newest = bus.publish(80, "C");
    assert(newest.sequenceId == 2);
    assert(newest.credit == 1);

    assert(bus.subscribe(80, 800).status == SubscribeStatus::Ok);
    receive_and_expect(
        bus, 80, 800, message, ReceiveStatus::NO_PENDING_MESSAGE, 0);

    assert(bus.publish(80, "D").sequenceId == 3);
    const auto result = receive_and_expect(
        bus, 80, 800, message, ReceiveStatus::SUCCESS, 0);
    assert(result.sequenceId == 3);
    assert(message == "D");

    print_pass("full lanes without subscribers reclaim retained history");
}


void test_duplicate_subscribe_does_not_reset_cursor()
{
    niniBUS bus;
    std::string message;
    assert(bus.createLane(90, 3) == CreateLaneStatus::Ok);
    const auto first_subscription = bus.subscribe(90, 900);
    assert(first_subscription.status == SubscribeStatus::Ok);
    assert(first_subscription.nextSequenceId == 0);
    assert(bus.publish(90, "before-duplicate").sequenceId == 0);
    const auto duplicate_subscription = bus.subscribe(90, 900);
    assert(duplicate_subscription.status == SubscribeStatus::Ok);
    assert(duplicate_subscription.nextSequenceId == 0);
    const auto result = receive_and_expect(
        bus, 90, 900, message, ReceiveStatus::SUCCESS, 0);
    assert(result.sequenceId == 0);
    assert(message == "before-duplicate");
    print_pass("duplicate subscribe preserves existing cursor progress");
}

void test_repeated_empty_receive_is_stable()
{
    niniBUS bus;
    std::string message;
    assert(bus.createLane(91, 2) == CreateLaneStatus::Ok);
    assert(bus.subscribe(91, 910).status == SubscribeStatus::Ok);
    for (int attempt = 0; attempt < 3; ++attempt)
    {
        message = "stale";
        const auto result = receive_and_expect(
            bus, 91, 910, message, ReceiveStatus::NO_PENDING_MESSAGE, 0);
        assert(result.sequenceId == 0);
        assert(result.skippedMessages == 0);
        assert(message.empty());
    }
    print_pass("repeated empty receives remain stable and clear output");
}

void test_invalid_lane_creation_does_not_reserve_lane()
{
    niniBUS bus;
    std::string message;
    assert(bus.createLane(92, 0) == CreateLaneStatus::InvalidCapacity);
    assert(bus.subscribe(92, 920).status == SubscribeStatus::LaneNotExist);
    receive_and_expect(bus, 92, 920, message, ReceiveStatus::NO_CURSOR, 0);
    const auto published = bus.publish(92, "created-later");
    assert(published.sequenceId == 0);
    assert(published.credit == DEFAULT_LANE_CAPACITY - 1);
    print_pass("invalid creation does not reserve or corrupt a lane ID");
}

void test_duplicate_create_lane_preserves_original_capacity()
{
    niniBUS bus;
    assert(bus.createLane(93, 2) == CreateLaneStatus::Ok);
    assert(bus.publish(93, "A").credit == 1);
    assert(bus.publish(93, "B").credit == 0);
    assert(bus.createLane(93, 5) == CreateLaneStatus::LaneExists);
    const auto result = bus.publish(93, "C");
    assert(result.sequenceId == 2);
    assert(result.credit == 1);
    print_pass("duplicate createLane does not replace capacity");
}

void test_empty_message_is_valid_payload()
{
    niniBUS bus;
    std::string message = "not-empty";
    assert(bus.createLane(94, 2) == CreateLaneStatus::Ok);
    assert(bus.subscribe(94, 940).status == SubscribeStatus::Ok);
    assert(bus.publish(94, "").sequenceId == 0);
    const auto result = receive_and_expect(
        bus, 94, 940, message, ReceiveStatus::SUCCESS, 0);
    assert(result.sequenceId == 0);
    assert(message.empty());
    print_pass("empty strings remain valid published messages");
}

void test_same_subscriber_id_is_independent_across_lanes()
{
    niniBUS bus;
    std::string message;
    assert(bus.createLane(95, 2) == CreateLaneStatus::Ok);
    assert(bus.createLane(96, 2) == CreateLaneStatus::Ok);
    assert(bus.subscribe(95, 950).status == SubscribeStatus::Ok);
    assert(bus.subscribe(96, 950).status == SubscribeStatus::Ok);
    assert(bus.publish(95, "lane-95").sequenceId == 0);
    assert(bus.publish(96, "lane-96").sequenceId == 0);
    receive_and_expect(bus, 95, 950, message, ReceiveStatus::SUCCESS, 0);
    assert(message == "lane-95");
    receive_and_expect(bus, 96, 950, message, ReceiveStatus::SUCCESS, 0);
    assert(message == "lane-96");
    print_pass("same subscriber ID remains independent across lanes");
}

void test_skipped_messages_accumulate_and_clear_once()
{
    niniBUS bus;
    std::string message;
    assert(bus.createLane(97, 1) == CreateLaneStatus::Ok);
    assert(bus.subscribe(97, 970).status == SubscribeStatus::Ok);
    assert(bus.publish(97, "A").sequenceId == 0);
    assert(bus.publish(97, "B").sequenceId == 1);
    assert(bus.publish(97, "C").sequenceId == 2);
    const auto reclaimed = receive_and_expect(
        bus, 97, 970, message, ReceiveStatus::SUCCESS, 0);
    assert(reclaimed.sequenceId == 2);
    assert(reclaimed.skippedMessages == 2);
    assert(message == "C");
    assert(bus.publish(97, "D").sequenceId == 3);
    const auto next = receive_and_expect(
        bus, 97, 970, message, ReceiveStatus::SUCCESS, 0);
    assert(next.skippedMessages == 0);
    assert(message == "D");
    print_pass("skipped-message count accumulates and is reported once");
}



void test_unsubscribe_missing_lane_or_subscriber()
{
    niniBUS bus;

    assert(!bus.unsubscribe(110, 1100));
    assert(bus.createLane(110, 2) == CreateLaneStatus::Ok);
    assert(!bus.unsubscribe(110, 1100));

    print_pass("unsubscribe rejects missing lanes and subscribers");
}

void test_unsubscribe_removes_cursor_and_is_not_repeatable()
{
    niniBUS bus;
    std::string message = "stale";

    assert(bus.createLane(111, 2) == CreateLaneStatus::Ok);
    assert(bus.subscribe(111, 1110).status == SubscribeStatus::Ok);
    assert(bus.publish(111, "pending").sequenceId == 0);

    assert(bus.unsubscribe(111, 1110));
    assert(!bus.unsubscribe(111, 1110));

    const auto result = receive_and_expect(
        bus, 111, 1110, message, ReceiveStatus::NO_CURSOR, 0);
    assert(result.sequenceId == 0);
    assert(result.skippedMessages == 0);
    assert(message.empty());

    print_pass("unsubscribe removes a cursor exactly once");
}

void test_unsubscribe_one_subscriber_preserves_another()
{
    niniBUS bus;
    std::string message;

    assert(bus.createLane(112, 3) == CreateLaneStatus::Ok);
    assert(bus.subscribe(112, 1120).status == SubscribeStatus::Ok);
    assert(bus.subscribe(112, 1121).status == SubscribeStatus::Ok);
    assert(bus.publish(112, "shared").sequenceId == 0);

    assert(bus.unsubscribe(112, 1120));
    receive_and_expect(
        bus, 112, 1120, message, ReceiveStatus::NO_CURSOR, 0);

    const auto active = receive_and_expect(
        bus, 112, 1121, message, ReceiveStatus::SUCCESS, 0);
    assert(active.sequenceId == 0);
    assert(message == "shared");

    print_pass("unsubscribe does not affect other subscribers");
}

void test_unsubscribe_then_resubscribe_starts_at_current_tail()
{
    niniBUS bus;
    std::string message;

    assert(bus.createLane(113, 4) == CreateLaneStatus::Ok);
    assert(bus.subscribe(113, 1130).status == SubscribeStatus::Ok);
    assert(bus.publish(113, "before-unsubscribe").sequenceId == 0);
    assert(bus.unsubscribe(113, 1130));
    assert(bus.publish(113, "while-unsubscribed").sequenceId == 1);

    assert(bus.subscribe(113, 1130).status == SubscribeStatus::Ok);
    receive_and_expect(
        bus, 113, 1130, message, ReceiveStatus::NO_PENDING_MESSAGE, 0);

    assert(bus.publish(113, "after-resubscribe").sequenceId == 2);
    const auto result = receive_and_expect(
        bus, 113, 1130, message, ReceiveStatus::SUCCESS, 0);
    assert(result.sequenceId == 2);
    assert(result.skippedMessages == 0);
    assert(message == "after-resubscribe");

    print_pass("re-subscribed cursor starts at the current tail");
}

void test_unsubscribe_last_subscriber_enables_no_cursor_reclaim()
{
    niniBUS bus;

    assert(bus.createLane(114, 2) == CreateLaneStatus::Ok);
    assert(bus.subscribe(114, 1140).status == SubscribeStatus::Ok);
    assert(bus.publish(114, "A").credit == 1);
    assert(bus.publish(114, "B").credit == 0);
    assert(bus.unsubscribe(114, 1140));

    const auto newest = bus.publish(114, "C");
    assert(newest.sequenceId == 2);
    assert(newest.credit == 1);

    print_pass("removing the final subscriber enables no-cursor reclaim");
}


} // namespace

int main()
{
    std::cout << "Starting niniBUS tests.\n";
    test_create_lane_and_subscribe_statuses();
    test_publish_and_receive_sequence();
    test_multiple_subscribers_receive_same_messages();
    test_late_subscriber_starts_at_current_tail();
    test_receive_missing_lane_or_cursor();
    test_reclaim_reports_skips_and_preserves_faster_subscriber();
    test_capacity_one_lane_keeps_latest_message();
    test_full_lane_without_subscribers_reclaims_history();
    test_duplicate_subscribe_does_not_reset_cursor();
    test_repeated_empty_receive_is_stable();
    test_invalid_lane_creation_does_not_reserve_lane();
    test_duplicate_create_lane_preserves_original_capacity();
    test_empty_message_is_valid_payload();
    test_same_subscriber_id_is_independent_across_lanes();
    test_skipped_messages_accumulate_and_clear_once();
    test_unsubscribe_missing_lane_or_subscriber();
    test_unsubscribe_removes_cursor_and_is_not_repeatable();
    test_unsubscribe_one_subscriber_preserves_another();
    test_unsubscribe_then_resubscribe_starts_at_current_tail();
    test_unsubscribe_last_subscriber_enables_no_cursor_reclaim();
    std::cout << "All niniBUS tests passed.\n";
    return 0;
}
