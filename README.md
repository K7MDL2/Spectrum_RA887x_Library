# Spectrum_RA887x_Library
Teensy 4 SDR Hi-Res Spectrum and Waterfall Library

This library was created for the SDR_RA887x SDR radio project based on a Teensy 4 CPU and SGTL5000 audio backpack card.
https://github.com/K7MDL2/KEITHSDR

Udpated Feb 18, 2023.  Minimal changes to the lib files.  The Example was very out of date.  I have updated the example program
1. Added Twin Peaks correction. Ff you have the 2x 10K resistors on the Line In L and R connected to Pin 22.
2. Added Hilbert filters for USB/LSB.  There is no tuning or Rf hardware assumptions in this example.  You can supply I and Q from external source, or any audio tone (no SSB though), or turn on the test tone to see and hear something other than noise.  Manually chanbge the mixer to change RX and TX sidebands.  It is set to USB by default.  The 14Mhz in teh display is fake since there is no real hardware.
3. Added Zoom.  I use 3 diffent FFTsizes to feed the spectrum to create an easy zoom.  There is a function called Zoom(_zoom_level) wher eyou can input 1 of 3 zoom levels.  x1 (48KHz), x2 (24KHz), and x4 (12Khz).
4. Added CESSB on the tone generatoin.  This has no effect but shows how to use it for a zero IF radio, and is also a 90 mixer so was handy to create a SSB signal out of the single tone.  You will see this on the display
5. You can use on eithe the 4.3" RA8875 or 7" RA8876.  Again, no RF hardware required.  No UI is presented, this is purley the specrum window you would incorporate into your own program.  I use a version of this in my SDR_887x program.  This does not have any bug fixes or improvements that the program version does, but it is very close and works.  The note below about the defines needing to match are still true.

Things to pay attention to in Spectrum_RA8875.h and your main program:

1. Set the #define USE_RA8875 accordingly.  Do this in Spectrum-Waterfall.h as well.
2. Do not define BETATEST
3. You can choose what FFT size you want.  The default is 4096.  For Zoom I change this in the main program to 2048 or 1024.  (x1, x2, x4). The changes are passed along in each call to spectrum update().  I updated the audio flows to match.  Uses 4096IQ by default.
4. I added the PCB board type definition into both the .h files.  Set them both (lib and example program .h files) the same. This gets the touch INT pin correct for your hardware.
5. Set your screen rotation in the main .h file as needed for your screen orientation  I usually have 0 for Ra8875 and 2 for RA8876.
6. Set #define TEST_SINE if you want a test tone instead of Line In audio source.
7. Test Trick.  I run a working SDR program that sets the PLL and RF hardware up.  Then, without power interruption, I upload the example program which does nto disturb any hardware so you wil hear and see the audio coming.  Add your pown tuning code and UI elements to make this a complete radio.



**** NOTE ***** As of May 7, 2022 builds of SDR_RA887x project, I have moved these files into the project sketch folder to get better control of #defines for board, display and FFT types. Previously you had to set the same #defines in both the RadioConfig.h and this library. These files are now in regular .h and .ccp form distributed with SDR_RA8875 on Github here.  You do not need this library any more (FOR BUILDSs May 7, 2022 AND LATER).

This evolved under the KeithsSDR project as a hi-res alternative to the original simpler version still supported in that project. It has been transformed into a separate library.  Teh example program has nto been upadted yet for the expanded args list using in varios functions.  Lots of opportunity to simplify some things like RA8875 vs Ra8876 display selection. 

Zoom is accomplished internally by selecting the FFT size (1024, 2048, 4096).  Each FFT bin is mapped 1:1 to a display pixel.  The RA8876 7" display used is 1024px wide, the RA8875 4.3" display used is 800px wide, minus a few px for borders.  Pan slides a 1024px (or 800px) window over array of FFT data (4096bins for 4x zoom, 2048 for 2x zoom or 1024 for 1X zoom).

It uses:
1. OpenAudio_Library 32bit floating point functions
2. Supports 1024IQ, 2048IQ and 4096IQ FFTs with various sample rates of your choice.
3. RA8875 and RA8876 displays.
4. A data table sizes the display areas for spectrum and waterfall sections.  
5. A window generator function spits out settings you can paste into your window table.  Sample windows definitions are included. 
6. Usage instructions for this library are in the code comments and in the KeithsSDR forum.  
7. I have been able to run 2 windows at the same time with different sizes though there are plenty of things that need to be fixed to make that usable, but it showed the resizing ability.
8. Most information about this library is found within the SDR_RA887x project so not much will be posted here for some time.
9. A frame draw function is called, then just one function is called thereafter to update the data, generally every 60-90ms.
10. The RA887x controllers have a Bit Move Engine that is leveraged to minimize CPU time to draw and scroll the spectrum and waterfall graphics sections. Only 2 or 3 draw commands are used in each section and takes relatively little time and no memory. 

Up
