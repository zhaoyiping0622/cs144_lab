#include "tcp_connection.hh"

#include <iostream>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

void TCPConnection::add_ack_win(TCPSegment &segment) {
    optional<WrappingInt32> ackno = _receiver.ackno();
    uint16_t winsize =
        static_cast<uint16_t>(min(_receiver.window_size(), static_cast<size_t>(numeric_limits<uint16_t>::max())));
    if (ackno) {
        segment.header().ackno = ackno.value();
        segment.header().ack = true;
    }
    segment.header().win = winsize;
}

void TCPConnection::send_rst() {
    _active = false;
    if (_sender.segments_out().empty())
        _sender.send_empty_segment();
    auto now = _sender.segments_out().front();
    _sender.segments_out().pop();
    add_ack_win(now);
    now.header().rst = true;
    _segments_out.push(now);
}

void TCPConnection::send_segments(bool send_empty) {
    _sender.fill_window();
    auto &q = _sender.segments_out();
    if (q.empty() && send_empty) {
        _sender.send_empty_segment();
    }
    while (q.size()) {
        auto now = q.front();
        q.pop();
        add_ack_win(now);
        _segments_out.push(now);
    }
}

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

bool TCPConnection::active() const { return _active; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    _time_since_last_segment_received = 0;
    if (!_connected && !seg.header().syn)
        return;
    _connected = true;
    if (seg.header().rst) {
        _active = false;
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        return;
    }
    if (seg.header().ack)
        _sender.ack_received(seg.header().ackno, seg.header().win);
    _receiver.segment_received(seg);
    if (_receiver.stream_out().eof()) {
        if (!_inbound_eof) {
            if (!(_sender.stream_in().eof() &&
                  _sender.next_seqno_absolute() == _sender.stream_in().bytes_written() + 2))
                _linger_after_streams_finish = false;
        }
        _inbound_eof = true;
    }
    // not only an ack
    if (seg.length_in_sequence_space())
        send_segments(true);
}

size_t TCPConnection::write(const string &data) {
    size_t ret = _sender.stream_in().write(data);
    send_segments();
    return ret;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    if (!_connected)
        return;
    _time_since_last_segment_received += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        _active = false;
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        send_rst();
        return;
    }
    if ((_receiver.stream_out().eof()) &&
        (_sender.stream_in().eof() && _sender.next_seqno_absolute() == _sender.stream_in().bytes_written() + 2 &&
         _sender.bytes_in_flight() == 0)) {
        if (!_linger_after_streams_finish || _time_since_last_segment_received >= 10 * _cfg.rt_timeout) {
            _active = false;
            return;
        }
    }
    send_segments();
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    send_segments();
}

void TCPConnection::connect() {
    _connected = true;
    send_segments(true);
}

TCPConnection::~TCPConnection() {
    try {
        if (!_connected && active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            _active = false;
            _sender.stream_in().set_error();
            _receiver.stream_out().set_error();
            send_rst();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}
