#include "stream_reassembler.hh"

#include <iostream>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void StreamReassembler::putc(char c, uint64_t index) {
    if (index - begin_idx >= _capacity)
        return;
    if (index >= max_idx)
        return;
    size_t idx = index - begin_idx + begin;
    if (idx >= _capacity)
        idx -= _capacity;
    if (!valid[idx]) {
        buffer[idx] = c;
        valid_bytes++;
        valid[idx] = true;
    }
}

bool StreamReassembler::getc(char &c, const bool pop) {
    if (!valid[begin])
        return false;
    c = buffer[begin];
    if (pop) {
        valid_bytes--;
        valid[begin] = false;
        begin++;
        begin_idx++;
        if (begin == _capacity)
            begin = 0;
    }
    return true;
}

void StreamReassembler::flush_buffer() {
    size_t canhold = _output.remaining_capacity();
    string s;
    for (size_t i = 0; i < canhold; i++) {
        char c;
        if (!getc(c, true))
            break;
        s += c;
    }
    _output.write(s);
    if (begin_idx == max_idx)
        _output.end_input();
}

StreamReassembler::StreamReassembler(const size_t capacity)
    : _output(capacity)
    , _capacity(capacity)
    , buffer(capacity)
    , begin(0)
    , begin_idx(0)
    , valid_bytes(0)
    , max_idx(UINT64_MAX)
    , valid(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (eof) {
        max_idx = data.size() + index;
    }
    for (size_t i = 0; i < data.size(); i++) {
        putc(data[i], index + i);
    }
    flush_buffer();
}

size_t StreamReassembler::unassembled_bytes() const { return valid_bytes; }

bool StreamReassembler::empty() const { return valid_bytes == 0; }
