// SPDX-License-Identifier: 0BSD
#include "TestOam.hpp"

#include "moonpak/Oam.cpp"

TestOam::TestOam() {
  Test::name = "Oam";
}

int TestOam::run(bool verbose) {
  return Oam::test(verbose);
}
