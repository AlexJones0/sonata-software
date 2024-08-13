// Copyright lowRISC Contributors.
// SPDX-License-Identifier: Apache-2.0

#include <stdint.h>
#include <stdbool.h>

typedef struct TaskOne {
    uint64_t acceleration;
    uint64_t braking;
    uint64_t speed;
} TaskOne;

typedef struct TaskTwo {
    uint64_t write[100];
} TaskTwo;

static TaskOne *task_one_mem;
static TaskTwo *task_two_mem;

static uint64_t wait_time;

static void (*uart_callback)(const char *__restrict__ __format, ...);
static uint64_t (*wait_callback)(const uint64_t wait_for);
static void (*loop_callback)(void);
static void (*ethernet_transmit_callback)(const uint64_t *buffer, uint16_t length);

void init_mem(TaskOne *task_one, TaskTwo *task_two) {
    task_one_mem = task_one;
    task_two_mem = task_two;
}

void init_uart_callback(void (*callback)(const char *__restrict__ __format, ...)) {
    uart_callback = callback;
}

void init_wait_callback(uint64_t duration, uint64_t (*callback)(const uint64_t wait_for)) {
    wait_time = duration;
    wait_callback = callback;
}

void init_loop_callback(void (*callback)(void)) {
    loop_callback = callback;
}

void init_ethernet_transmit_callback(void (*callback)(const uint64_t *buffer, uint16_t length)) {
    ethernet_transmit_callback = callback;
}

bool task_one() {
    if (task_one_mem->acceleration > 0) {
        task_one_mem->speed += 1;
    }
    if (task_one_mem->braking > 0) {
        task_one_mem->speed -= 1;
    }
    uart_callback("Sending pedal data: acceleration=%u, braking=%u.\n", 
        (unsigned int) task_one_mem->acceleration, 
        (unsigned int) task_one_mem->braking);
    const uint64_t transmit_buf[2] = {
        task_one_mem->acceleration, 
        task_one_mem->braking
    };
    ethernet_transmit_callback(transmit_buf, 2);
    uart_callback("Car is at a new speed of %u\n", (unsigned int) task_one_mem->speed);
    return true;
}

bool task_two() {
    static uint64_t counter;
    counter += 1;
    uart_callback("Calling task two update, count = %u\n", (unsigned int) counter);
    if (counter == 100) {
        uart_callback("counter == 100: triggering a bug\n");
    }
    if (counter <= 100) {
        // uart_callback("Writing to mem location %u\n", (unsigned int) &task_two_mem->write[counter]);
        task_two_mem->write[counter] = 1000;
    }
    return true;
}

void run(uint64_t init_time)
{
	uart_callback("Automotive demo started!\n");
    uint64_t last_elapsed_time = init_time;
    for (uint32_t i = 0; i < 128; i++) {
        task_one();
        task_two();
        last_elapsed_time = wait_callback(last_elapsed_time + wait_time);
        loop_callback();
    }
    uart_callback("Automotive demo ended!\n");
}
