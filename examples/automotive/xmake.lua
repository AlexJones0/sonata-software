-- Copyright lowRISC Contributors.
-- SPDX-License-Identifier: Apache-2.0

compartment("automotive")
    add_deps("lcd", "debug")
    add_files("main.cc")

compartment("automotive_receive")
    add_deps("lcd", "debug")
    add_files("receive.cc")
