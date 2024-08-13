// Copyright lowRISC Contributors.
// SPDX-License-Identifier: Apache-2.0

#include <compartment.h>
#include <debug.hh>
#include <thread.h>
#include <platform-ethernet.hh>

#include "./automotive.c"

using Debug = ConditionalDebug<true, "Automotive-Receive">;
using namespace CHERI;

EthernetDevice *ethernet;

TaskOne car_info;

uint64_t wait(const uint64_t wait_for) {
    uint64_t cur_time = rdcycle64();
    while (cur_time < wait_for) {
        cur_time = rdcycle64();
        // Busy wait, TODO could change to thread sleep?
    }
    return cur_time;
}

void receive_ethernet_frame() {
    Debug::log("Polling for ethernet frame...");
    std::optional<EthernetDevice::Frame> maybeFrame = ethernet->receive_frame();
    if (!maybeFrame.has_value()) {
        return;
    }
    Debug::log("Received a frame with some value!");
    // TODO get length right and change to length, and do this nicer
    // TODO don't hardcode this
    //for (uint32_t i = 0; i < 16; ++i) {
    //    Debug::log("maybeFrame->buffer[{}] = {}", (int) i, maybeFrame->buffer[i]);
    //}
    car_info.acceleration =  ((uint64_t) maybeFrame->buffer[14]) << 56;
    car_info.acceleration |= ((uint64_t) maybeFrame->buffer[15]) << 48;
    car_info.acceleration |= ((uint64_t) maybeFrame->buffer[16]) << 40;
    car_info.acceleration |= ((uint64_t) maybeFrame->buffer[17]) << 32;
    car_info.acceleration |= ((uint64_t) maybeFrame->buffer[18]) << 24;
    car_info.acceleration |= ((uint64_t) maybeFrame->buffer[19]) << 16;
    car_info.acceleration |= ((uint64_t) maybeFrame->buffer[20]) << 8;
    car_info.acceleration |= ((uint64_t) maybeFrame->buffer[21]);
    car_info.braking =  ((uint64_t) maybeFrame->buffer[22]) << 56;
    car_info.braking |= ((uint64_t) maybeFrame->buffer[23]) << 48;
    car_info.braking |= ((uint64_t) maybeFrame->buffer[24]) << 40;
    car_info.braking |= ((uint64_t) maybeFrame->buffer[25]) << 32;
    car_info.braking |= ((uint64_t) maybeFrame->buffer[26]) << 24;
    car_info.braking |= ((uint64_t) maybeFrame->buffer[27]) << 16;
    car_info.braking |= ((uint64_t) maybeFrame->buffer[28]) << 8;
    car_info.braking |= ((uint64_t) maybeFrame->buffer[29]);
}

extern "C" ErrorRecoveryBehaviour
compartment_error_handler(ErrorState *frame, size_t mcause, size_t mtval)
{
	Debug::log(
	  "Unexpected CHERI Capability violation encountered. Stopping...");
	return ErrorRecoveryBehaviour::ForceUnwind;
}

// Thread entry point.
[[noreturn]] void __cheri_compartment("automotive_receive") entry() {
    // Initialise ethernet driver for use via callback
    ethernet = new EthernetDevice();
    ethernet->mac_address_set({0x01, 0x23, 0x45, 0x67, 0x89, 0xAB});
    //while (!ethernet->phy_link_status()) {
    //    thread_millisecond_wait(50);
    //}

    const uint32_t cyclesPerMillisecond = CPU_TIMER_HZ / 1000;
    const uint32_t waitTime = 80 * cyclesPerMillisecond;
    uint64_t prev_time = rdcycle64();
    while (true) {
        receive_ethernet_frame();
        Debug::log("Current pedal info - acceleration={}, speed={}", (int) car_info.acceleration, (int) car_info.braking);
        prev_time = wait(prev_time + waitTime);
    }

    // Cleanup
    delete ethernet;

}
