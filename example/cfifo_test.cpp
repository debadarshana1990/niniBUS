#include <cassert>
#include <iostream>
#include <stdexcept>
#include <string>

#include "cfifo.h"

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
    assert(result.movedBy == 0);
    assert(result.pendingMessage == 0);
    assert(message == "unchanged");

    print_pass("read without a registered cursor");
}

void test_cursor_starts_at_current_tail()
{
    cfifo<std::string> queue(3);
    std::string message;

    assert(queue.write("old-message").status == CFIFOWriteStatus::SUCCESS);
    assert(queue.create_cursor(30));

    CFIFOReadResult initially_caught_up = queue.read(30, message);
    assert(initially_caught_up.status ==
           CFIFOReadStatus::NO_PENDING_MESSAGE);

    assert(queue.write("new-message").status == CFIFOWriteStatus::SUCCESS);

    CFIFOReadResult received = queue.read(30, message);
    assert(received.status == CFIFOReadStatus::SUCCESS);
    assert(received.pendingMessage == 0);
    assert(message == "new-message");

    print_pass("new cursor starts at the current tail");
}

void test_multiple_cursors_read_same_messages()
{
    cfifo<std::string> queue(3);
    std::string message;

    assert(queue.create_cursor(40));
    assert(queue.create_cursor(41));
    assert(queue.write("A").status == CFIFOWriteStatus::SUCCESS);
    assert(queue.write("B").status == CFIFOWriteStatus::SUCCESS);

    CFIFOReadResult cursor_40_a = queue.read(40, message);
    assert(cursor_40_a.status == CFIFOReadStatus::SUCCESS);
    assert(cursor_40_a.pendingMessage == 1);
    assert(message == "A");

    CFIFOReadResult cursor_40_b = queue.read(40, message);
    assert(cursor_40_b.status == CFIFOReadStatus::SUCCESS);
    assert(cursor_40_b.pendingMessage == 0);
    assert(message == "B");

    CFIFOReadResult cursor_41_a = queue.read(41, message);
    assert(cursor_41_a.status == CFIFOReadStatus::SUCCESS);
    assert(cursor_41_a.pendingMessage == 1);
    assert(message == "A");

    CFIFOReadResult cursor_41_b = queue.read(41, message);
    assert(cursor_41_b.status == CFIFOReadStatus::SUCCESS);
    assert(cursor_41_b.pendingMessage == 0);
    assert(message == "B");

    assert(queue.read(40, message).status ==
           CFIFOReadStatus::NO_PENDING_MESSAGE);
    assert(queue.read(41, message).status ==
           CFIFOReadStatus::NO_PENDING_MESSAGE);

    print_pass("multiple cursors independently read shared messages");
}

void test_write_credit_and_full_queue()
{
    cfifo<std::string> queue(2);

    CFIFOWriteResult first = queue.write("first");
    assert(first.status == CFIFOWriteStatus::SUCCESS);
    assert(first.credit == 1);

    CFIFOWriteResult second = queue.write("second");
    assert(second.status == CFIFOWriteStatus::SUCCESS);
    assert(second.credit == 0);
    assert(queue.full());
    assert(queue.size() == 2);

    CFIFOWriteResult overflow = queue.write("overflow");
    assert(overflow.status == CFIFOWriteStatus::Q_FULL);
    assert(overflow.credit == 0);
    assert(queue.size() == 2);

    print_pass("write credit and full queue rejection");
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
    assert(queue.write("").status == CFIFOWriteStatus::SUCCESS);

    CFIFOReadResult result = queue.read(60, message);
    assert(result.status == CFIFOReadStatus::SUCCESS);
    assert(result.pendingMessage == 0);
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
    assert(queue.write("A").status == CFIFOWriteStatus::SUCCESS);
    assert(queue.write("B").status == CFIFOWriteStatus::SUCCESS);

    CFIFOReadResult first = queue.read(70, message);
    assert(first.status == CFIFOReadStatus::SUCCESS);
    assert(message == "A");

    CFIFOWriteResult reclaimed_write = queue.write("C");
    assert(reclaimed_write.status == CFIFOWriteStatus::SUCCESS);
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
    assert(queue.write("A").status == CFIFOWriteStatus::SUCCESS);
    assert(queue.write("B").status == CFIFOWriteStatus::SUCCESS);

    assert(queue.read(80, message).status == CFIFOReadStatus::SUCCESS);
    assert(message == "A");

    assert(queue.write("C").status == CFIFOWriteStatus::SUCCESS);

    CFIFOReadResult after_reclaim = queue.read(80, message);
    assert(after_reclaim.status == CFIFOReadStatus::SUCCESS);
    assert(after_reclaim.movedBy == 1);
    assert(after_reclaim.pendingMessage == 0);
    assert(message == "C");

    CFIFOReadResult caught_up = queue.read(80, message);
    assert(caught_up.status == CFIFOReadStatus::NO_PENDING_MESSAGE);
    assert(caught_up.movedBy == 0);

    print_pass("reclaim reports skipped messages once");
}

void test_reclaim_preserves_other_cursor_progress()
{
    cfifo<std::string> queue(2);
    std::string message;

    assert(queue.create_cursor(90));
    assert(queue.create_cursor(91));
    assert(queue.write("A").status == CFIFOWriteStatus::SUCCESS);
    assert(queue.write("B").status == CFIFOWriteStatus::SUCCESS);

    // Cursor 90 is ahead; cursor 91 is the slowest cursor.
    assert(queue.read(90, message).status == CFIFOReadStatus::SUCCESS);
    assert(message == "A");

    assert(queue.write("C").status == CFIFOWriteStatus::SUCCESS);

    CFIFOReadResult fast_cursor = queue.read(90, message);
    assert(fast_cursor.status == CFIFOReadStatus::SUCCESS);
    assert(fast_cursor.movedBy == 0);
    assert(message == "B");

    CFIFOReadResult reclaimed_cursor = queue.read(91, message);
    assert(reclaimed_cursor.status == CFIFOReadStatus::SUCCESS);
    assert(reclaimed_cursor.movedBy == 2);
    assert(message == "C");

    print_pass("reclaim preserves non-selected cursor progress");
}

void test_reclaim_when_all_messages_were_consumed()
{
    cfifo<std::string> queue(2);
    std::string message;

    assert(queue.create_cursor(100));
    assert(queue.write("A").status == CFIFOWriteStatus::SUCCESS);
    assert(queue.write("B").status == CFIFOWriteStatus::SUCCESS);
    assert(queue.full());

    assert(queue.read(100, message).status == CFIFOReadStatus::SUCCESS);
    assert(message == "A");
    assert(queue.read(100, message).status == CFIFOReadStatus::SUCCESS);
    assert(message == "B");

    CFIFOWriteResult result = queue.write("C");
    assert(result.status == CFIFOWriteStatus::SUCCESS);
    assert(result.credit == 1);
    assert(queue.size() == 1);
    assert(!queue.full());

    CFIFOReadResult received = queue.read(100, message);
    assert(received.status == CFIFOReadStatus::SUCCESS);
    assert(received.movedBy == 0);
    assert(received.pendingMessage == 0);
    assert(message == "C");

    print_pass("reclaim resets storage after every cursor catches up");
}

void test_repeated_reclaim_and_wraparound()
{
    cfifo<std::string> queue(2);
    std::string message;

    assert(queue.create_cursor(110));
    assert(queue.write("A").status == CFIFOWriteStatus::SUCCESS);
    assert(queue.write("B").status == CFIFOWriteStatus::SUCCESS);

    assert(queue.write("C").status == CFIFOWriteStatus::SUCCESS);
    CFIFOReadResult first_reclaim = queue.read(110, message);
    assert(first_reclaim.status == CFIFOReadStatus::SUCCESS);
    assert(first_reclaim.movedBy == 2);
    assert(message == "C");

    assert(queue.write("D").status == CFIFOWriteStatus::SUCCESS);
    CFIFOReadResult second_reclaim = queue.read(110, message);
    assert(second_reclaim.status == CFIFOReadStatus::SUCCESS);
    assert(second_reclaim.movedBy == 0);
    assert(message == "D");

    assert(queue.size() <= queue.capacity());
    assert(queue.credit() <= queue.capacity());

    print_pass("repeated reclaim survives circular-buffer wraparound");
}

void test_capacity_one_reclaim()
{
    cfifo<std::string> queue(1);
    std::string message;

    assert(queue.create_cursor(120));
    assert(queue.write("old").status == CFIFOWriteStatus::SUCCESS);
    assert(queue.write("latest").status == CFIFOWriteStatus::SUCCESS);

    CFIFOReadResult result = queue.read(120, message);
    assert(result.status == CFIFOReadStatus::SUCCESS);
    assert(result.movedBy == 1);
    assert(result.pendingMessage == 0);
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
    assert(queue.write("A").status == CFIFOWriteStatus::SUCCESS);
    assert(queue.write("B").status == CFIFOWriteStatus::SUCCESS);
    assert(queue.write("C").status == CFIFOWriteStatus::SUCCESS);

    assert(queue.read(131, message).status == CFIFOReadStatus::SUCCESS);
    assert(message == "A");
    assert(queue.remove_cursor(130));

    assert(queue.write("D").status == CFIFOWriteStatus::SUCCESS);

    CFIFOReadResult result = queue.read(131, message);
    assert(result.status == CFIFOReadStatus::SUCCESS);
    assert(result.movedBy == 2);
    assert(result.pendingMessage == 0);
    assert(message == "D");
    assert(queue.size() == 1);
    assert(queue.credit() == 2);

    print_pass("removed cursor is excluded from reclaim selection");
}

int main()
{
    std::cout << "Starting cfifo tests.\n";

    test_constructor_and_global_state();
    test_cursor_registration();
    test_read_without_cursor();
    test_cursor_starts_at_current_tail();
    test_multiple_cursors_read_same_messages();
    test_write_credit_and_full_queue();
    test_cursor_removal();
    test_empty_string_message();
    test_reclaim_keeps_size_and_credit_bounded();
    test_reclaim_reports_skipped_messages();
    test_reclaim_preserves_other_cursor_progress();
    test_reclaim_when_all_messages_were_consumed();
    test_repeated_reclaim_and_wraparound();
    test_capacity_one_reclaim();
    test_removing_slowest_cursor_changes_reclaim_candidate();

    std::cout << "All cfifo tests passed.\n";
    return 0;
}
