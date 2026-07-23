#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>

#include "cfifo.h"

using nbus::CFIFOReadResult;
using nbus::CFIFOReadStatus;
using nbus::CFIFOWriteResult;
using nbus::cfifo;

void print_pass(const std::string& test_name)
{
    std::cout << "[PASS] " << test_name << '\n';
}

void test_constructor_and_global_state()
{
    cfifo<std::string> queue(3);

    assert(queue.capacity() == 3);
    assert(queue.size() == 0);
    assert(queue.credit() == 3);
    assert(queue.empty());
    assert(!queue.full());

    bool threw = false;
    try
    {
        cfifo<std::string> invalid_queue(0);
    }
    catch (const std::invalid_argument&)
    {
        threw = true;
    }
    assert(threw);

    print_pass("constructor and global queue state");
}

void test_cursor_registration()
{
    cfifo<std::string> queue(3);

    assert(!queue.contains_cursor(10));
    assert(queue.create_cursor(10));
    assert(queue.contains_cursor(10));
    assert(!queue.create_cursor(10));

    print_pass("cursor registration and duplicate detection");
}

void test_read_without_cursor()
{
    cfifo<std::string> queue(2);
    std::string message = "unchanged";

    CFIFOReadResult result = queue.read(20, message);
    assert(result.status == CFIFOReadStatus::NO_CURSOR);
    assert(result.skipped_messages == 0);
    assert(result.pending_messages == 0);
    assert(message == "unchanged");

    print_pass("read without a registered cursor");
}

void test_cursor_starts_at_current_tail()
{
    cfifo<std::string> queue(3);
    std::string message;

    assert(queue.write("old-message").sequence_id == 0);
    assert(queue.create_cursor(30));

    CFIFOReadResult initially_caught_up = queue.read(30, message);
    assert(initially_caught_up.status ==
           CFIFOReadStatus::NO_PENDING_MESSAGE);

    assert(queue.write("new-message").sequence_id == 1);

    CFIFOReadResult received = queue.read(30, message);
    assert(received.status == CFIFOReadStatus::SUCCESS);
    assert(received.pending_messages == 0);
    assert(received.sequence_id == 1);
    assert(message == "new-message");

    print_pass("new cursor starts at the current tail");
}

void test_multiple_cursors_read_same_messages()
{
    cfifo<std::string> queue(3);
    std::string message;

    assert(queue.create_cursor(40));
    assert(queue.create_cursor(41));
    assert(queue.write("A").sequence_id == 0);
    assert(queue.write("B").sequence_id == 1);

    CFIFOReadResult cursor_40_a = queue.read(40, message);
    assert(cursor_40_a.status == CFIFOReadStatus::SUCCESS);
    assert(cursor_40_a.pending_messages == 1);
    assert(cursor_40_a.sequence_id == 0);
    assert(message == "A");

    CFIFOReadResult cursor_40_b = queue.read(40, message);
    assert(cursor_40_b.status == CFIFOReadStatus::SUCCESS);
    assert(cursor_40_b.pending_messages == 0);
    assert(cursor_40_b.sequence_id == 1);
    assert(message == "B");

    CFIFOReadResult cursor_41_a = queue.read(41, message);
    assert(cursor_41_a.status == CFIFOReadStatus::SUCCESS);
    assert(cursor_41_a.pending_messages == 1);
    assert(cursor_41_a.sequence_id == 0);
    assert(message == "A");

    CFIFOReadResult cursor_41_b = queue.read(41, message);
    assert(cursor_41_b.status == CFIFOReadStatus::SUCCESS);
    assert(cursor_41_b.pending_messages == 0);
    assert(cursor_41_b.sequence_id == 1);
    assert(message == "B");

    assert(queue.read(40, message).status ==
           CFIFOReadStatus::NO_PENDING_MESSAGE);
    assert(queue.read(41, message).status ==
           CFIFOReadStatus::NO_PENDING_MESSAGE);

    print_pass("multiple cursors independently read shared messages");
}

void test_write_credit_and_no_cursor_reclaim()
{
    cfifo<std::string> queue(2);

    CFIFOWriteResult first = queue.write("first");
    assert(first.sequence_id == 0);
    assert(first.credit == 1);

    CFIFOWriteResult second = queue.write("second");
    assert(second.sequence_id == 1);
    assert(second.credit == 0);
    assert(queue.full());
    assert(queue.size() == 2);

    CFIFOWriteResult overflow = queue.write("overflow");
    assert(overflow.sequence_id == 2);
    assert(overflow.credit == 1);
    assert(queue.size() == 1);
    assert(!queue.full());

    print_pass("full queue without cursors reclaims retained messages");
}

void test_cursor_removal()
{
    cfifo<std::string> queue(2);

    assert(queue.create_cursor(50));
    assert(queue.contains_cursor(50));
    assert(queue.remove_cursor(50));
    assert(!queue.contains_cursor(50));
    assert(!queue.remove_cursor(50));

    print_pass("cursor removal");
}

void test_empty_string_message()
{
    cfifo<std::string> queue(1);
    std::string message = "stale";

    assert(queue.create_cursor(60));
    assert(queue.write("").sequence_id == 0);

    CFIFOReadResult result = queue.read(60, message);
    assert(result.status == CFIFOReadStatus::SUCCESS);
    assert(result.pending_messages == 0);
    assert(message.empty());

    assert(queue.read(60, message).status ==
           CFIFOReadStatus::NO_PENDING_MESSAGE);

    print_pass("empty string is a valid message");
}

void test_reclaim_keeps_size_and_credit_bounded()
{
    cfifo<std::string> queue(2);
    std::string message;

    assert(queue.create_cursor(70));
    assert(queue.write("A").sequence_id == 0);
    assert(queue.write("B").sequence_id == 1);

    CFIFOReadResult first = queue.read(70, message);
    assert(first.status == CFIFOReadStatus::SUCCESS);
    assert(message == "A");

    CFIFOWriteResult reclaimed_write = queue.write("C");
    assert(reclaimed_write.sequence_id == 2);
    assert(queue.size() <= queue.capacity());
    assert(queue.size() == 1);
    assert(queue.credit() == 1);

    print_pass("reclaim keeps size and credit within capacity");
}

void test_reclaim_reports_skipped_messages()
{
    cfifo<std::string> queue(2);
    std::string message;

    assert(queue.create_cursor(80));
    assert(queue.write("A").sequence_id == 0);
    assert(queue.write("B").sequence_id == 1);

    assert(queue.read(80, message).status == CFIFOReadStatus::SUCCESS);
    assert(message == "A");

    assert(queue.write("C").sequence_id == 2);

    CFIFOReadResult after_reclaim = queue.read(80, message);
    assert(after_reclaim.status == CFIFOReadStatus::SUCCESS);
    assert(after_reclaim.skipped_messages == 1);
    assert(after_reclaim.pending_messages == 0);
    assert(after_reclaim.sequence_id == 2);
    assert(message == "C");

    CFIFOReadResult caught_up = queue.read(80, message);
    assert(caught_up.status == CFIFOReadStatus::NO_PENDING_MESSAGE);
    assert(caught_up.skipped_messages == 0);

    print_pass("reclaim reports skipped messages once");
}

void test_reclaim_preserves_other_cursor_progress()
{
    cfifo<std::string> queue(2);
    std::string message;

    assert(queue.create_cursor(90));
    assert(queue.create_cursor(91));
    assert(queue.write("A").sequence_id == 0);
    assert(queue.write("B").sequence_id == 1);

    // Cursor 90 is ahead; cursor 91 is the slowest cursor.
    assert(queue.read(90, message).status == CFIFOReadStatus::SUCCESS);
    assert(message == "A");

    assert(queue.write("C").sequence_id == 2);

    CFIFOReadResult fast_cursor = queue.read(90, message);
    assert(fast_cursor.status == CFIFOReadStatus::SUCCESS);
    assert(fast_cursor.skipped_messages == 0);
    assert(message == "B");

    CFIFOReadResult reclaimed_cursor = queue.read(91, message);
    assert(reclaimed_cursor.status == CFIFOReadStatus::SUCCESS);
    assert(reclaimed_cursor.skipped_messages == 2);
    assert(message == "C");

    print_pass("reclaim preserves non-selected cursor progress");
}

void test_reclaim_when_all_messages_were_consumed()
{
    cfifo<std::string> queue(2);
    std::string message;

    assert(queue.create_cursor(100));
    assert(queue.write("A").sequence_id == 0);
    assert(queue.write("B").sequence_id == 1);
    assert(queue.full());

    assert(queue.read(100, message).status == CFIFOReadStatus::SUCCESS);
    assert(message == "A");
    assert(queue.read(100, message).status == CFIFOReadStatus::SUCCESS);
    assert(message == "B");

    CFIFOWriteResult result = queue.write("C");
    assert(result.sequence_id == 2);
    assert(result.credit == 1);
    assert(queue.size() == 1);
    assert(!queue.full());

    CFIFOReadResult received = queue.read(100, message);
    assert(received.status == CFIFOReadStatus::SUCCESS);
    assert(received.skipped_messages == 0);
    assert(received.pending_messages == 0);
    assert(message == "C");

    print_pass("reclaim resets storage after every cursor catches up");
}

void test_repeated_reclaim_and_wraparound()
{
    cfifo<std::string> queue(2);
    std::string message;

    assert(queue.create_cursor(110));
    assert(queue.write("A").sequence_id == 0);
    assert(queue.write("B").sequence_id == 1);

    assert(queue.write("C").sequence_id == 2);
    CFIFOReadResult first_reclaim = queue.read(110, message);
    assert(first_reclaim.status == CFIFOReadStatus::SUCCESS);
    assert(first_reclaim.skipped_messages == 2);
    assert(message == "C");

    assert(queue.write("D").sequence_id == 3);
    CFIFOReadResult second_reclaim = queue.read(110, message);
    assert(second_reclaim.status == CFIFOReadStatus::SUCCESS);
    assert(second_reclaim.skipped_messages == 0);
    assert(message == "D");

    assert(queue.size() <= queue.capacity());
    assert(queue.credit() <= queue.capacity());

    print_pass("repeated reclaim survives circular-buffer wraparound");
}

void test_skipped_messages_accumulate_until_successful_read()
{
    cfifo<std::string> queue(2);
    std::string message;

    assert(queue.create_cursor(115));
    assert(queue.write("A").sequence_id == 0);
    assert(queue.write("B").sequence_id == 1);

    // The first reclaim skips A and B.
    assert(queue.write("C").sequence_id == 2);
    assert(queue.write("D").sequence_id == 3);

    // The second reclaim happens before the subscriber reads and skips C and D.
    assert(queue.write("E").sequence_id == 4);

    CFIFOReadResult result = queue.read(115, message);
    assert(result.status == CFIFOReadStatus::SUCCESS);
    assert(result.sequence_id == 4);
    assert(result.skipped_messages == 4);
    assert(message == "E");

    assert(queue.write("F").sequence_id == 5);
    CFIFOReadResult next = queue.read(115, message);
    assert(next.status == CFIFOReadStatus::SUCCESS);
    assert(next.skipped_messages == 0);
    assert(message == "F");

    print_pass("skipped messages accumulate until a successful read");
}

void test_write_sequence_ids_survive_buffer_wraparound()
{
    cfifo<int> queue(2);

    const auto first = queue.write(10);
    const auto second = queue.write(20);
    const auto third = queue.write(30);
    const auto fourth = queue.write(40);

    assert(first.sequence_id == 0);
    assert(second.sequence_id == 1);
    assert(third.sequence_id == 2);
    assert(fourth.sequence_id == 3);

    print_pass("write sequence IDs survive physical buffer wraparound");
}

void test_capacity_one_reclaim()
{
    cfifo<std::string> queue(1);
    std::string message;

    assert(queue.create_cursor(120));
    assert(queue.write("old").sequence_id == 0);
    assert(queue.write("latest").sequence_id == 1);

    CFIFOReadResult result = queue.read(120, message);
    assert(result.status == CFIFOReadStatus::SUCCESS);
    assert(result.skipped_messages == 1);
    assert(result.pending_messages == 0);
    assert(message == "latest");
    assert(queue.size() == 1);
    assert(queue.credit() == 0);

    print_pass("capacity-one queue reclaims the previous message");
}

void test_removing_slowest_cursor_changes_reclaim_candidate()
{
    cfifo<std::string> queue(3);
    std::string message;

    assert(queue.create_cursor(130));
    assert(queue.create_cursor(131));
    assert(queue.write("A").sequence_id == 0);
    assert(queue.write("B").sequence_id == 1);
    assert(queue.write("C").sequence_id == 2);

    assert(queue.read(131, message).status == CFIFOReadStatus::SUCCESS);
    assert(message == "A");
    assert(queue.remove_cursor(130));

    assert(queue.write("D").sequence_id == 3);

    CFIFOReadResult result = queue.read(131, message);
    assert(result.status == CFIFOReadStatus::SUCCESS);
    assert(result.skipped_messages == 2);
    assert(result.pending_messages == 0);
    assert(message == "D");
    assert(queue.size() == 1);
    assert(queue.credit() == 2);

    print_pass("removed cursor is excluded from reclaim selection");
}

void test_removed_cursor_reregisters_at_current_tail()
{
    cfifo<std::string> queue(3);
    std::string message;

    assert(queue.create_cursor(132));
    assert(queue.write("A").sequence_id == 0);
    assert(queue.remove_cursor(132));

    assert(queue.write("B").sequence_id == 1);
    assert(queue.create_cursor(132));

    const auto caught_up = queue.read(132, message);
    assert(caught_up.status == CFIFOReadStatus::NO_PENDING_MESSAGE);

    assert(queue.write("C").sequence_id == 2);
    const auto result = queue.read(132, message);
    assert(result.status == CFIFOReadStatus::SUCCESS);
    assert(result.sequence_id == 2);
    assert(message == "C");

    print_pass("removed cursor re-registers at the current tail");
}

void test_reclaim_does_not_treat_subscriber_id_as_sequence()
{
    cfifo<std::string> queue(2);
    std::string message;

    // These IDs deliberately do not match head, tail, or cursor sequences.
    assert(queue.create_cursor(5000));
    assert(queue.create_cursor(7000));
    assert(queue.write("A").sequence_id == 0);
    assert(queue.write("B").sequence_id == 1);

    assert(queue.read(5000, message).status == CFIFOReadStatus::SUCCESS);
    assert(message == "A");
    assert(queue.read(5000, message).status == CFIFOReadStatus::SUCCESS);
    assert(message == "B");

    assert(queue.read(7000, message).status == CFIFOReadStatus::SUCCESS);
    assert(message == "A");
    assert(queue.read(7000, message).status == CFIFOReadStatus::SUCCESS);
    assert(message == "B");

    // Every cursor is at tail, so the old retained queue can become empty
    // before C is written.
    CFIFOWriteResult write = queue.write("C");
    assert(write.sequence_id == 2);
    assert(queue.size() == 1);
    assert(queue.credit() == 1);

    print_pass("reclaim keeps subscriber IDs separate from sequences");
}

void test_reclaim_moves_every_cursor_tied_at_head()
{
    cfifo<std::string> queue(3);
    std::string message;

    assert(queue.create_cursor(200));
    assert(queue.create_cursor(300));
    assert(queue.write("A").sequence_id == 0);
    assert(queue.write("B").sequence_id == 1);
    assert(queue.write("C").sequence_id == 2);

    // Both cursors are tied at the old head and must be moved before the head
    // can advance.
    assert(queue.write("D").sequence_id == 3);
    assert(queue.size() == 1);
    assert(queue.credit() == 2);

    CFIFOReadResult cursor_200 = queue.read(200, message);
    assert(cursor_200.status == CFIFOReadStatus::SUCCESS);
    assert(cursor_200.skipped_messages == 3);
    assert(message == "D");

    CFIFOReadResult cursor_300 = queue.read(300, message);
    assert(cursor_300.status == CFIFOReadStatus::SUCCESS);
    assert(cursor_300.skipped_messages == 3);
    assert(message == "D");

    print_pass("reclaim moves all cursors tied at the head");
}

void test_reclaim_tied_slowest_cursors_preserves_advanced_cursor()
{
    cfifo<std::string> queue(3);
    std::string message;

    assert(queue.create_cursor(400));
    assert(queue.create_cursor(401));
    assert(queue.create_cursor(402));
    assert(queue.write("A").sequence_id == 0);
    assert(queue.write("B").sequence_id == 1);
    assert(queue.write("C").sequence_id == 2);

    // Cursor 402 is ahead. Cursors 400 and 401 are tied at the head.
    assert(queue.read(402, message).status == CFIFOReadStatus::SUCCESS);
    assert(message == "A");

    assert(queue.write("D").sequence_id == 3);
    assert(queue.size() == 3);
    assert(queue.credit() == 0);

    // The advanced cursor retains B and C.
    CFIFOReadResult advanced = queue.read(402, message);
    assert(advanced.status == CFIFOReadStatus::SUCCESS);
    assert(advanced.skipped_messages == 0);
    assert(advanced.pending_messages == 2);
    assert(message == "B");

    // Both tied slow cursors were moved to the old tail and next receive D.
    CFIFOReadResult slow_400 = queue.read(400, message);
    assert(slow_400.status == CFIFOReadStatus::SUCCESS);
    assert(slow_400.skipped_messages == 3);
    assert(message == "D");

    CFIFOReadResult slow_401 = queue.read(401, message);
    assert(slow_401.status == CFIFOReadStatus::SUCCESS);
    assert(slow_401.skipped_messages == 3);
    assert(message == "D");

    print_pass("reclaim preserves a cursor ahead of tied slow cursors");
}

int main()
{
    std::cout << "Starting cfifo tests.\n";

    test_constructor_and_global_state();
    test_cursor_registration();
    test_read_without_cursor();
    test_cursor_starts_at_current_tail();
    test_multiple_cursors_read_same_messages();
    test_write_credit_and_no_cursor_reclaim();
    test_cursor_removal();
    test_empty_string_message();
    test_reclaim_keeps_size_and_credit_bounded();
    test_reclaim_reports_skipped_messages();
    test_reclaim_preserves_other_cursor_progress();
    test_reclaim_when_all_messages_were_consumed();
    test_repeated_reclaim_and_wraparound();
    test_skipped_messages_accumulate_until_successful_read();
    test_write_sequence_ids_survive_buffer_wraparound();
    test_capacity_one_reclaim();
    test_removing_slowest_cursor_changes_reclaim_candidate();
    test_removed_cursor_reregisters_at_current_tail();
    test_reclaim_does_not_treat_subscriber_id_as_sequence();
    test_reclaim_moves_every_cursor_tied_at_head();
    test_reclaim_tied_slowest_cursors_preserves_advanced_cursor();

    std::cout << "All cfifo tests passed.\n";
    return 0;
}
