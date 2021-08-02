#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void ByteStream::buffer_resize(size_t capacity) { this->buffer.resize(capacity); }

bool ByteStream::putc(char c) {
    if (length == buffer_capacity)
        return false;
    size_t nxt = begin + length;
    if (nxt >= buffer_capacity)
        nxt -= buffer_capacity;
    buffer[nxt] = c;
    length++;
    bytes_written_size++;
    return true;
}

ByteStream::ByteStream(const size_t capacity)
    : buffer_capacity(capacity), buffer(vector<char>()), begin(0), length(0), bytes_written_size(0), is_end(false) {
    buffer_resize(capacity);
}

size_t ByteStream::write(const string &data) {
    size_t ret = 0;
    for (auto x : data) {
        if (putc(x)) {
            ret++;
        } else {
            break;
        }
    }
    return ret;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    string ret;
    size_t now = begin;
    const size_t seek = min(len, length);
    for (size_t i = 0; i < seek; i++) {
        ret += buffer[now];
        now++;
        if (now == buffer_capacity)
            now = 0;
    }
    return ret;
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    const size_t seek = min(len, length);
    begin += seek;
    if (begin >= buffer_capacity)
        begin -= buffer_capacity;
    length -= seek;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    string ret = peek_output(len);
    pop_output(len);
    return ret;
}

void ByteStream::end_input() { is_end = true; }

bool ByteStream::input_ended() const { return is_end; }

size_t ByteStream::buffer_size() const { return length; }

bool ByteStream::buffer_empty() const { return length == 0; }

bool ByteStream::eof() const { return input_ended() && buffer_empty(); }

size_t ByteStream::bytes_written() const { return bytes_written_size; }

size_t ByteStream::bytes_read() const { return bytes_written() - length; }

size_t ByteStream::remaining_capacity() const { return buffer_capacity - length; }
