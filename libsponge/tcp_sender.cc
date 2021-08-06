#include "tcp_sender.hh"

#include "tcp_config.hh"

#include <iostream>
#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPSender::remove_segments() {
    while (_outstanding_segments.size()) {
        auto now = _outstanding_segments.front();
        uint64_t end_idx =
            unwrap(WrappingInt32(now.header().seqno.raw_value() + now.length_in_sequence_space()), _isn, _next_seqno);
        if (end_idx <= _prev_ackno) {
            _outstanding_segments.pop();
            _bytes_in_flight -= now.length_in_sequence_space();
        } else {
            break;
        }
    }
    if (_outstanding_segments.empty())
        _timer.reset(0);
}

void TCPSender::send_segment(const TCPSegment segment) {
    if (_timer.end())
        _timer.reset(_current_rto);
    _segments_out.push(segment);
    _bytes_in_flight += segment.length_in_sequence_space();
    _next_seqno += segment.length_in_sequence_space();
    _outstanding_segments.push(segment);
}

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity)
    , _current_rto(retx_timeout) {}

TCPSegment TCPSender::read_data(size_t size) {
    TCPSegment segment;
    segment.header().seqno = wrap(stream_in().bytes_read() + 1, _isn);
    segment.payload() = Buffer(stream_in().read(min(size, TCPConfig::MAX_PAYLOAD_SIZE)));
    if (stream_in().eof() && segment.length_in_sequence_space() + 1 <= size)
        segment.header().fin = true;
    return segment;
}

void TCPSender::fill_window() {
    if (_next_seqno == 0) {
        // CLOSED
        // send SYN
        TCPSegment segment;
        segment.header().seqno = wrap(0, _isn);
        segment.header().syn = true;
        send_segment(segment);
        return;
    }
    if (_next_seqno == bytes_in_flight()) {
        // SYN_SENT
        return;
    }
    if (_current_window_size == 0 && _outstanding_segments.empty()) {
        send_segment(read_data(1));
        return;
    }
    while (bytes_in_flight() < _current_window_size && !stream_in().eof()) {
        size_t size = _current_window_size - bytes_in_flight();
        TCPSegment segment = read_data(size);
        if (segment.length_in_sequence_space())
            send_segment(segment);
        else
            break;
    }
    if (stream_in().eof()) {
        if (_next_seqno < stream_in().bytes_written() + 2) {
            if (bytes_in_flight() < _current_window_size) {
                TCPSegment segment;
                segment.header().seqno = wrap(stream_in().bytes_read() + 1, _isn);
                segment.header().fin = true;
                send_segment(segment);
            }
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    _current_window_size = window_size;
    uint64_t absolute_ackno = unwrap(ackno, _isn, _next_seqno);
    if (absolute_ackno > _prev_ackno) {
        _prev_ackno = absolute_ackno;
        _current_rto = _initial_retransmission_timeout;
        _consecutive_retransmissions = 0;
        remove_segments();
        fill_window();
        if (_outstanding_segments.size()) {
            _timer.reset(_current_rto);
        }
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    _timer.minus(ms_since_last_tick);
    if (_timer.end()) {
        if (_current_window_size) {
            _current_rto *= 2;
            _consecutive_retransmissions++;
        }
        if (_outstanding_segments.size()) {
            auto now = _outstanding_segments.front();
            _segments_out.push(now);
            _timer.reset(_current_rto);
        }
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment segment;
    _segments_out.push(segment);
}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }
