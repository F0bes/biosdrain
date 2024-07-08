# biosdrain

## **Instructions**

Download the latest version of biosdrain under the GitHub releases page.

### - **For PS2LINK / PS2CLIENT users**

Execute biosdrain on the EE. For example: `ps2client execee host:biosdrain.elf`

biosdrain should automatically detect a host filesystem, and dump the files directly to your computer.

### - **For USB users (using uLaunchELF)**
Put the biosdrain.elf file onto a FAT32 (or exFAT!) formatted USB drive.

In uLaunchELF, navigate to `mass:` and execute the biosdrain.elf file you just transferred.

biosdrain should automatically detect a USB drive, and dump the files to the root of the USB drive.

**In the case that biosdrain does not detect a USB drive, try creating a smaller 1GB partition and
leaving the rest unformatted.**

### **Using the BIOS with PCSX2**
Simply copy the files (if you've used a USB drive) back to your computer, and keep them somewhere safe so you don't have to do this process again.

<br/>

## **I politely ask that you do not share your BIOS files!**

<br/>

If the path to your bios was `D:\ps2bios\dump` you'd see one of the below.

**WX**

![image](https://user-images.githubusercontent.com/29295048/180281013-7e142c3a-2762-4987-885f-27d622029d64.png)

**QT**

![image](https://user-images.githubusercontent.com/29295048/180281541-a402877c-c8b7-4eea-9e36-710d3f48b0e2.png)


## **Uninteresting info**

### Currently detects and dumps
 - ROM0
 - ROM1 *size fixed at 4096kb
 - ROM2
 - NVM
 - MEC

### Notes

This software has been based off of the work from [PS2Ident](https://github.com/ps2homebrew/PS2Ident) licensed under AFL license-3.0 . The Sysman and Romdrv module source have been taken from there as well.

Check out https://fobes.dev for some interesting PS2 programming blogs.
You can support me on KoFi [here](Ko-fi.com/f0bes).
