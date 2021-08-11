#include "network_interface.hh"

#include "arp_message.hh"
#include "ethernet_frame.hh"

#include <iostream>

// Dummy implementation of a network interface
// Translates from {IP datagram, next hop address} to link-layer frame, and from link-layer frame to IP datagram

// For Lab 5, please replace with a real implementation that passes the
// automated checks run by `make check_lab5`.

// You will need to add private members to the class declaration in `network_interface.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

//! \param[in] ethernet_address Ethernet (what ARP calls "hardware") address of the interface
//! \param[in] ip_address IP (what ARP calls "protocol") address of the interface
NetworkInterface::NetworkInterface(const EthernetAddress &ethernet_address, const Address &ip_address)
    : _ethernet_address(ethernet_address), _ip_address(ip_address) {
    cerr << "DEBUG: Network interface has Ethernet address " << to_string(_ethernet_address) << " and IP address "
         << ip_address.ip() << "\n";
}

void NetworkInterface::send_arp_request(uint32_t next_hop_ip) {
    for (auto x : _wait_reply) {
        if (x.ip_address == next_hop_ip)
            return;
    }
    EthernetFrame to_send;
    to_send.header().src = _ethernet_address;
    to_send.header().dst = ETHERNET_BROADCAST;
    to_send.header().type = EthernetHeader::TYPE_ARP;
    ARPMessage arp_message;
    arp_message.opcode = ARPMessage::OPCODE_REQUEST;
    arp_message.sender_ethernet_address = _ethernet_address;
    arp_message.sender_ip_address = _ip_address.ipv4_numeric();
    // arp_message.target_ethernet_address=ETHERNET_BROADCAST;
    arp_message.target_ip_address = next_hop_ip;
    to_send.payload().append(arp_message.serialize());
    _frames_out.push(to_send);
    ARPRecord arp_record;
    arp_record.time_out = _current_time + ARP_REQUEST_TIMEOUT;
    arp_record.ip_address = next_hop_ip;
    _wait_reply.push_back(arp_record);
}

void NetworkInterface::update_records(uint32_t ip_address, EthernetAddress ethernet_address) {
    ARPRecord arp_record;
    arp_record.ethernet_address = ethernet_address;
    arp_record.ip_address = ip_address;
    arp_record.time_out = _current_time + ARP_MAPPING_TIMEOUT;
    _records[arp_record.ip_address] = arp_record;
    for (auto it = _wait_to_send.begin(); it != _wait_to_send.end();) {
        if (it->first == ip_address) {
            send_datagram(it->second, Address::from_ipv4_numeric(it->first));
            it = _wait_to_send.erase(it);
        } else {
            it++;
        }
    }
}

void NetworkInterface::reply_arp_request(const ARPMessage &arp_message) {
    update_records(arp_message.sender_ip_address, arp_message.sender_ethernet_address);
    if (arp_message.target_ip_address == _ip_address.ipv4_numeric()) {
        if (arp_message.opcode == ARPMessage::OPCODE_REPLY) {
            for (auto it = _wait_reply.begin(); it != _wait_reply.end(); it++) {
                if (it->ip_address == arp_message.sender_ip_address) {
                    _wait_reply.erase(it);
                    break;
                }
            }
        } else if (arp_message.opcode == ARPMessage::OPCODE_REQUEST) {
            EthernetFrame to_send;
            to_send.header().src = _ethernet_address;
            to_send.header().dst = arp_message.sender_ethernet_address;
            to_send.header().type = EthernetHeader::TYPE_ARP;
            ARPMessage arp_message_reply;
            arp_message_reply.opcode = ARPMessage::OPCODE_REPLY;
            arp_message_reply.sender_ethernet_address = _ethernet_address;
            arp_message_reply.sender_ip_address = _ip_address.ipv4_numeric();
            arp_message_reply.target_ethernet_address = arp_message.sender_ethernet_address;
            arp_message_reply.target_ip_address = arp_message.sender_ip_address;
            to_send.payload().append(arp_message_reply.serialize());
            _frames_out.push(to_send);
        }
    }
}

//! \param[in] dgram the IPv4 datagram to be sent
//! \param[in] next_hop the IP address of the interface to send it to (typically a router or default gateway, but may also be another host if directly connected to the same network as the destination)
//! (Note: the Address type can be converted to a uint32_t (raw 32-bit IP address) with the Address::ipv4_numeric() method.)
void NetworkInterface::send_datagram(const InternetDatagram &dgram, const Address &next_hop) {
    // convert IP address of next hop to raw 32-bit representation (used in ARP header)
    const uint32_t next_hop_ip = next_hop.ipv4_numeric();

    auto it = _records.find(next_hop_ip);
    if (it == _records.end()) {
        send_arp_request(next_hop_ip);
        _wait_to_send.push_back({next_hop_ip, dgram});
    } else {
        EthernetFrame to_send;
        to_send.payload().append(dgram.serialize());
        to_send.header().type = EthernetHeader::TYPE_IPv4;
        to_send.header().src = _ethernet_address;
        to_send.header().dst = it->second.ethernet_address;
        _frames_out.push(to_send);
    }
}

//! \param[in] frame the incoming Ethernet frame
optional<InternetDatagram> NetworkInterface::recv_frame(const EthernetFrame &frame) {
    if (frame.header().type == EthernetHeader::TYPE_IPv4) {
        InternetDatagram ret;
        if (ParseResult::NoError == ret.parse(frame.payload().concatenate())) {
            if (frame.header().dst == _ethernet_address) {
                return ret;
            }
            return {};
        } else {
            return {};
        }
    } else {
        ARPMessage arp_message;
        if (ParseResult::NoError == arp_message.parse(frame.payload().concatenate())) {
            reply_arp_request(arp_message);
        }
        return {};
    }
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void NetworkInterface::tick(const size_t ms_since_last_tick) {
    _current_time += ms_since_last_tick;
    for (auto it = _records.begin(); it != _records.end();) {
        if (it->second.time_out < _current_time) {
            it = _records.erase(it);
        } else {
            it++;
        }
    }
    for (auto it = _wait_reply.begin(); it != _wait_reply.end();) {
        if (it->time_out < _current_time) {
            auto now = *it;
            it = _wait_reply.erase(it);
            send_arp_request(now.ip_address);
        } else {
            it++;
        }
    }
}
