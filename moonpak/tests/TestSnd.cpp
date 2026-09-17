// SPDX-License-Identifier: 0BSD
#include "TestSnd.hpp"

#include "moonpak/Snd.cpp"
#include "moonpak/Snd.iwram.cpp"
#include "moonpak/SndData.cpp"

TestSnd::TestSnd() {
  Test::name = "Snd";
}

int TestSnd::run(bool verbose) {
  return Snd::test(verbose);
}
