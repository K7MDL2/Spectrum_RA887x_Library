# Spectrum_RA887x_Library
Teensy 4 SDR Hi-Res Spectrum and Waterfall Library

This library was created for the SDR_RA887x SDR radio project based on a Teensy 4 CPU and SGTL5000 audio backpack card.
https://github.com/K7MDL2/KEITHSDR

This evolved under the KeithsSDR project as a hi-res alternative to the original simpler version still supported in that project. It has been transformed into a separate library but it is lacking examples and still has some opportunity to simplify some things like RA8875 vs Ra8876 display selection. 

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
