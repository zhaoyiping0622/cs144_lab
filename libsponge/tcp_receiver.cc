#include "tcp_receiver.hh"

#include <iostream>

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    WrappingInt32 seqno = seg.header().seqno;
    if (!isn) {
        if (!seg.header().syn)
            return;
        isn.emplace(seqno);
        seqno = WrappingInt32(seqno.raw_value() + 1);
    }
    uint64_t index = unwrap(seqno, isn.value(), _reassembler.stream_out().bytes_written()) - 1;
    bool eof = seg.header().fin;
    _reassembler.push_substring(seg.payload().copy(), index, eof);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    optional<WrappingInt32> ret;
    const ByteStream &stream = _reassembler.stream_out();
    if (isn) {
        if (stream.input_ended())
            ret.emplace(wrap(stream.bytes_written() + 2, isn.value()));
        else
            ret.emplace(wrap(stream.bytes_written() + 1, isn.value()));
    }
    return ret;
}

size_t TCPReceiver::window_size() const { return _reassembler.stream_out().remaining_capacity(); }
