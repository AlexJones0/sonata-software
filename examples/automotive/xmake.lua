-- Copyright lowRISC Contributors.
-- SPDX-License-Identifier: Apache-2.0

--compartment("modified_snake_temp")
--  add_deps("lcd", "debug")
--  add_files("modified_snake_temp.cc")

compartment("bad")
  add_deps("debug")
  add_files("bad.cc")

compartment("automotive") 
  add_deps("lcd", "debug", "bad") --"modified_snake_temp")
  add_files("automotive.cc")
