# viper
A simple, lean and header only(!!!) command line parser inspired from [cobra](https://github.com/spf13/cobra).

Consider checking out the `examples` folder for examples!

## Overview
You can build a structure of commands (basically a tree like hierarchy) which each command being a child of some parent command (except the root command).

Special validators can also be set for positional args and flags. You can also create custom validators (example coming soon, in the meantime check out the source code - it's really simple I promise!).

## Usage
Just copy `viper.hpp`, paste it in your `include` folder, and do a simple `#include "viper.hpp"` (or custom path).
