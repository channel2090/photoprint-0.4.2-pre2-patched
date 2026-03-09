# PhotoPrint 0.4.2-pre2 (Patched)

PhotoPrint 0.4.2-pre2 patched to compile and run on modern Linux distributions.

This repository contains a patched and buildable version of **PhotoPrint 0.4.2-pre2**.

The original project had build issues on modern Linux distributions due to outdated dependencies.  
This version includes fixes that allow the software to compile and run successfully.

## Original Project

PhotoPrint is a utility designed to assist in printing digital photographs on Linux and UNIX-like systems.

Original features include:

- Printing photographs in multiple layouts
- Poster printing across several pages
- Image cropping and borders
- ICC color profile support
- 16-bit printing support
- Basic image effects (sharpen, desaturate, temperature adjustment)

## Changes Made in This Repository

The following changes were made to make the project compile on modern systems:

- Fixed linking issues with **LCMS**
- Adjusted build configuration for modern compilers
- Resolved dependency issues
- Cleaned build process
- Successfully compiled and tested on modern Linux

## Build Instructions

Install dependencies first.

Example (Debian/Ubuntu based systems):

sudo apt install build-essential libgtk2.0-dev liblcms2-dev gutenprint libjpeg-dev

Then compile:

./configure
make
sudo make install

## Maintainer

This patched version was prepared and compiled by:

**Deiby Herrera (channel2090)**  
Costa Rica

GitHub:  
https://github.com/channel2090
