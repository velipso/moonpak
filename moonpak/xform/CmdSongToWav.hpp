// SPDX-License-Identifier: 0BSD
#pragma once
#include "xform.hpp"

struct CmdSongToWav : Command {
  CmdSongToWav();
  void help() override;
  int main(int argc, const char **argv) override;
};
