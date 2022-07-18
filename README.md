# biosdrain
Builds are under the Action tabs, and are also provided as incremental releases.

### Currently detects and dumps
 - ROM0
 - ROM1
 - EROM
 - NVM
 - MEC

### Needs to be tested / doesn't work
- ROM2

### Notes

This software has been based off of the work from [PS2Ident](https://github.com/ps2homebrew/PS2Ident) licensed under AFL license-3.0 . The Sysman and Romdrv module source have been taken from there as well.

Proper erom driver loading is not available on PCSX2. Thereby making erom dectection return a false negative. (Not that it really matters).

Issues and pull requests are very much welcome. The issue tracker may be used by users to report successful dumping as well, as I only have a single 39K to test on. Please make sure your model doesn't already have a report though.
