// 
//      SPECTRUM-WATERFALL.cpp
//
//  This is an example file how to use the Spectrum_RA887x library to display a custom
//  sized window containing a sepctrum display and a waterfall display.
//  It takes advantage of the RA887x controller's ability to move blocks of display memory
//  with very few CPU commands enabling rather high resolution scrolling spectrum and waterfall
//  displays possible without undue burden on the host CPU.   
//  This allows CPU cycles to handle other tasks like audio processing and encoders.
//

#include "Spectrum-Waterfall.h"

void I2C_Scanner(void);
void printHelp(void);
void printCPUandMemory(unsigned long curTime_millis, unsigned long updatePeriod_millis);
void respondToByte(char c);
const char* formatVFO(uint32_t vfo);
void Change_FFT_Size(uint16_t new_size, float new_sample_rate_Hz);
void Zoom(uint8_t _zoom_Level);

// -------------------------------------------------------------------------------------------
// Audio Library setup stuff to provide FFT data with optional Test tone
// -------------------------------------------------------------------------------------------
//float32_t sample_rate_Hz = 11000.0f;  //43Hz /bin  5K spectrum
//float32_t sample_rate_Hz = 22000.0f;  //21Hz /bin 6K wide
//float32_t sample_rate_Hz = 44100.0f;  //43Hz /bin  12.5K spectrum
float32_t sample_rate_Hz = 48000.0f;  //46Hz /bin  24K spectrum for 1024.   ***** CESSB limited to 48Khz or so *********
//float32_t sample_rate_Hz = 51200.0f;  // 50Hz/bin for 1024, 200Hz/bin for 256 FFT. 20Khz span at 800 pixels 2048 FFT
//float32_t sample_rate_Hz = 96000.0f;   // <100Hz/bin at 1024FFT, 50Hz at 2048, 40Khz span at 800 pixels and 2048FFT
//float32_t sample_rate_Hz = 102400.0f;   // 100Hz/bin at 1024FFT, 50Hz at 2048, 40Khz span at 800 pixels and 2048FFT
//float32_t sample_rate_Hz = 192000.0f; // 190Hz/bin - does
//float32_t sample_rate_Hz = 204800.0f; // 200/bin at 1024 FFT

const int audio_block_samples = 128;          // do not change this!
AudioSettings_F32 audio_settings(sample_rate_Hz, audio_block_samples);

// used for spectrum object
//#define FFT_SIZE                  4096           // set in .h
uint16_t        fft_size            = FFT_SIZE;       // This value will be passed to the init function.
int16_t         fft_bins            = (int16_t) fft_size;     // Number of FFT bins which is FFT_SIZE/2 for real version or FFT_SIZE for iq version
float32_t       fft_bin_size        = sample_rate_Hz/(fft_size*2);   // Size of FFT bin in HZ.  From sample_rate_Hz/FFT_SIZE for iq
int16_t         spectrum_preset     = 0;                    // Specify the default layout option for spectrum window placement and size.
extern Metro    spectrum_waterfall_update;          // Timer used for controlling the Spectrum module update rate.


// The main program should define at least 1 layout record based on this structure.
// Here is a working example usually placed in your main program header files.
#define PRESETS 1 // number of struct records for Sp_Parms_Def, the defaults table. You can override this with a table in EEPROM someday
struct Spectrum_Parms Sp_Parms_Def[PRESETS] = { // define default sets of spectrum window parameters, mostly for easy testing but could be used for future custom preset layout options
        //W        LE  RE CG                                                    x   y   w  h  c sp st clr sc mode      scal reflvl wfrate
    #ifdef USE_RA8875
        {798,0, 0,  0, 798,398,14,8,157,179,179,408,400,110,111,289,289,  0,153,799,256,50,20,6,240,1.0,0.9,1,20, 5, 30}      // Default layout for 4.3" RA8875 800 x480
    #else
        {1022,1,1,  1,1022,510,14,8,143,165,165,528,520,142,213,307,307,  0,139,1023,390,40,20,6,890,1.5,0.9,1,20,10, 30}   // Default layout for 7" RA8876 1024 x 600
    #endif        
};
//extern struct   Spectrum_Parms Sp_Parms_Def[];
struct New_Spectrum_Layout   Custom_Layout[1];


// Display type set in .h file
Spectrum_RA887x spectrum_RA887x(fft_size, fft_bins, fft_bin_size);   // initialize the Spectrum Library
#ifdef USE_RA8875
  RA8875 tft = RA8875(RA8875_CS,RA8875_RESET); //initiate the display object
#else
  RA8876_t3 tft = RA8876_t3(RA8876_CS,RA8876_RESET); //initiate the display object
  FT5206 cts = FT5206(CTP_INT); 
#endif
//
//============================================ End of Spectrum Setup Section =====================================================
//

const int myInput = AUDIO_INPUT_LINEIN;
//const int myInput = AUDIO_INPUT_MIC;

#ifdef BETATEST
  DMAMEM  float32_t  fftOutput[4096];  // Array used for FFT Output to the INO program
  DMAMEM  float32_t  window[2048];     // Windows reduce sidelobes with FFT's *Half Size*
  DMAMEM  float32_t  fftBuffer[8192];  // Used by FFT, 4096 real, 4096 imag, interleaved
  DMAMEM  float32_t  sumsq[4096];      // Required ONLY if power averaging is being done
#endif

#ifdef FFT_4096
  #ifndef BETATEST
    DMAMEM  AudioAnalyzeFFT4096_IQ_F32  myFFT_4096;  // choose which you like, set FFT_SIZE accordingly.
  #else
    AudioAnalyzeFFT4096_IQEM_F32 myFFT_4096(fftOutput, window, fftBuffer, sumsq);  // with power averaging array 
  #endif
#endif
#ifdef FFT_2048   
  DMAMEM AudioAnalyzeFFT2048_IQ_F32  myFFT_2048;
#endif
#ifdef FFT_1024
  DMAMEM AudioAnalyzeFFT1024_IQ_F32  myFFT_1024;
#endif

// cessb parameters
struct levelsZ* pLevelData;
uint32_t writeOne = 0;
uint32_t cntFFT = 0;
radioCESSB_Z_transmit_F32   cessb1(audio_settings);

// regular flow
AudioInputI2S_F32           Input(audio_settings);
AudioMixer4_F32             I_Switch(audio_settings);
AudioMixer4_F32             Q_Switch(audio_settings);
AudioOutputI2S_F32          Output(audio_settings);
AudioSwitch4_OA_F32         FFT_OutSwitch_I(audio_settings); // Select a source to send to the FFT engine
AudioSwitch4_OA_F32         FFT_OutSwitch_Q(audio_settings);
DMAMEM AudioFilterFIR_F32   RX_Hilbert_Plus_45(audio_settings);
DMAMEM AudioFilterFIR_F32   RX_Hilbert_Minus_45(audio_settings);
AudioMixer4_F32             RX_Summer(audio_settings);
AudioAlignLR_F32            TwinPeak(SIGNAL_HARDWARE, PIN_FOR_TP, false, audio_settings);

// Test tones we can use
AudioSynthWaveformSine_F32  sinewave1; // for audible alerts like touch beep confirmations
AudioSynthWaveformSine_F32  sinewave2; // for audible alerts like touch beep confirmations

// Line Input
AudioConnection_F32     patchCord_RX_In_L(Input,0,                           TwinPeak,0); // correct i2s phase imbalance
AudioConnection_F32     patchCord_RX_In_R(Input,1,                           TwinPeak,1);
AudioConnection_F32     patchCord_RX_Ph_L(TwinPeak,0,                        I_Switch,0);  // route raw input audio to the FFT display
AudioConnection_F32     patchCord_RX_Ph_R(TwinPeak,1,                        Q_Switch,0);

// Send the test tone through a CESSB direct to the I and Q outputs 
AudioConnection_F32         patchCord_Audio_Tone(sinewave1,0,     cessb1,0);    // CE SSB compression
AudioConnection_F32         patchCord_Audio_Filter_L(cessb1,0,    I_Switch,1); // fixed 2800Hz TX filter 
AudioConnection_F32         patchCord_Audio_Filter_R(cessb1,1,    Q_Switch,1); // output is at -1350 from source so have to shift it up.

//AudioConnection_F32         patchCord8a(sinewave1,0,         I_Switch,0);
//AudioConnection_F32         patchCord8b(sinewave1,0,         Q_Switch,0);
AudioConnection_F32         patchCord8c(I_Switch,0,         FFT_OutSwitch_I,0);  
AudioConnection_F32         patchCord8d(Q_Switch,0,         FFT_OutSwitch_Q,0);

// One or more of these FFT pipelines can be used, most likely for pan and zoom.  Normally just 1 is used.
#ifdef FFT_4096
    AudioConnection_F32     patchCord_FFT_L_4096(FFT_OutSwitch_I,0,             myFFT_4096,0);   // Route selected audio source to the FFT
    AudioConnection_F32     patchCord_FFT_R_4096(FFT_OutSwitch_Q,0,             myFFT_4096,1);
#endif
#ifdef FFT_2048
    AudioConnection_F32     patchCord_FFT_L_2048(FFT_OutSwitch_I,1,             myFFT_2048,0);        // Route selected audio source to the FFT
    AudioConnection_F32     patchCord_FFT_R_2048(FFT_OutSwitch_Q,1,             myFFT_2048,1);
#endif
#ifdef FFT_1024
    AudioConnection_F32     patchCord_FFT_L_1024(FFT_OutSwitch_I,2,             myFFT_1024,0);        // Route selected audio source to the FFT
    AudioConnection_F32     patchCord_FFT_R_1024(FFT_OutSwitch_Q,2,             myFFT_1024,1);
#endif

AudioConnection_F32         patchCord11a(I_Switch,0,                            RX_Hilbert_Plus_45,0);
AudioConnection_F32         patchCord11b(Q_Switch,0,                            RX_Hilbert_Minus_45,0);
AudioConnection_F32         patchCord2c(RX_Hilbert_Plus_45,0,                   RX_Summer,0);  // phase shift +45 deg
AudioConnection_F32         patchCord2d(RX_Hilbert_Minus_45,0,                  RX_Summer,1);  // phase shift -45 deg

// patch through the audio input to the headphones/lineout
AudioConnection_F32         patchCord_Audio_ToneL(RX_Summer,0,                  Output,0);
AudioConnection_F32         patchCord_Audio_ToneR(RX_Summer,0,                  Output,1); 

AudioControlSGTL5000    codec1;

int32_t rit_offset = 0; // account for RIT freqwuency offset over VFO base value
int32_t ModeOffset = 1; // pitch value for CW, 600 for example centers the shaded filter over the VFO
                        // -1 for LSB, +1 for USB, 0 for centered (AM)
uint16_t filterBandwidth = 2700;
uint16_t filterCenter = filterBandwidth/2 + 100;
float pan = 0;

void setup()
{
    Serial.begin(115200);
    delay(500);
    Serial.println(F("Initializing SDR_RA887x Program\n"));
    Serial.println(F("**** Running I2C Scanner ****"));

    // ---------------- Setup our basic display and comms ---------------------------
    Wire.begin();
    Wire.setClock(100000UL); // Keep at 100K I2C bus transfer data rate for I2C Encoders to work right
    I2C_Scanner();

    #ifdef USE_RA8875
        Serial.println(F("Initializing RA8875 Display"));
        tft.begin(RA8875_800x480);
        tft.setRotation(SCREEN_ROTATION); // 0 is normal, 1 is 90, 2 is 180, 3 is 270 degrees
        delay(20);
        #if defined(USE_FT5206_TOUCH)
          tft.useCapINT(RA8875_INT);
          tft.setTouchLimit(MAXTOUCHLIMIT);
          tft.enableCapISR(true);
          tft.setTextColor(WHITE, BLACK);
        #else
          #ifdef USE_RA8875
              tft.print("you should open RA8875UserSettings.h file and uncomment USE_FT5206_TOUCH!");
          #endif  // USE_RA8875
        #endif // USE_FT5206_TOUCH
    #else 
        Serial.println(F("Initializing RA8876 Display"));   
        tft.begin(50000000UL);
        cts.begin();
        cts.setTouchLimit(MAXTOUCHLIMIT);
        tft.touchEnable(false);   // Ensure the resitive ocntroller, if any is off
        tft.displayImageStartAddress(PAGE1_START_ADDR); 
        tft.displayImageWidth(SCREEN_WIDTH);
        tft.displayWindowStartXY(0,0);
        // specify the page 2 for the current canvas
        tft.canvasImageStartAddress(PAGE2_START_ADDR);
        // specify the page 1 for the current canvas
        tft.canvasImageStartAddress(PAGE1_START_ADDR);
        tft.canvasImageWidth(SCREEN_WIDTH);
        //tft.activeWindowXY(0,0);
        //tft.activeWindowWH(SCREEN_WIDTH,SCREEN_HEIGHT);
        spectrum_RA887x.setActiveWindow_default();
        tft.graphicMode(true);
        tft.clearActiveScreen();
        tft.selectScreen(0);  // Select screen page 0
        tft.fillScreen(BLACK);
        tft.setBackGroundColor(BLACK);
        tft.setTextColor(WHITE, BLACK);
        tft.backlight(true);
        tft.displayOn(true);
        tft.setRotation(SCREEN_ROTATION); // 0 is normal, 1 is 90, 2 is 180, 3 is 270 degrees.  
                        // RA8876 touch controller is upside down compared to the RA8875 so correcting for it there.
    #endif
    
    spectrum_RA887x.initSpectrum(spectrum_preset); // Call before initDisplay() to put screen into Layer 1 mode before any other text is drawn!

    //--------------------------   Setup our Audio System -------------------------------------

    AudioNoInterrupts();
    AudioMemory_F32(150, audio_settings);
    codec1.enable(); // MUST be before inputSelect()
    delay(5);
    //codec1.dacVolumeRampDisable(); // Turn off the sound for now
    codec1.inputSelect(myInput);
    codec1.lineInLevel(10);  //codec line in max level. Range 0 to 15.  0 => 3.12Vp-p, 15 => 0.24Vp-p high sensitivity
    codec1.lineOutLevel(15); // range 13 to 31.  13 => 3.16Vp-p, 31=> 1.16Vp-p
    codec1.autoVolumeControl(2, 0, 0, -36.0, 12, 6);                   // add a compressor limiter
    //codec1.autoVolumeControl( 0-2, 0-3, 0-1, 0-96, 3, 3);
    //autoVolumeControl(maxGain, response, hardLimit, threshold, attack, decay);
    codec1.autoVolumeEnable(); // let the volume control itself..... poor mans agc
    //codec1.autoVolumeDisable();// Or don't let the volume control itself
    codec1.muteLineout(); //mute the audio output until we finish thumping relays 
    codec1.adcHighPassFilterDisable();
    codec1.audioPreProcessorEnable();  // AVC on Line-In level
    codec1.audioPostProcessorEnable(); // AVC on Line-Out level
    //codec1.dacVolume(0); // set the "dac" volume (extra control)
    codec1.volume(0.1f); // set max full scale volume
    // Insert a test tone to see something on the display.
    
    Serial.println("Start W7PUA AutoI2S Error Correction");
    TwinPeaks(); // W7PUA auto detect and correct. Requires 100K resistors on the LineIn pins to a common Teensy GPIO pin

    float sinewave_vol = 0.001f;
    sinewave1.amplitude(sinewave_vol);  // tone volume (0 to 1.0)
    sinewave1.frequency(1000.0f); // Tone Frequency in Hz
    //sinewave2.amplitude(sinewave_vol);  // tone volume (0 to 1.0)
    //sinewave2.frequency(4000.0f); // Tone Frequency in Hz
    
    // Select our sources for the FFT.  mode.h will change this so CW uses the output (for now as an experiment)
    #ifdef TEST_SINE
      // shut off Inputs
      I_Switch.gain(0, 0.0f); //  1.0f is Input source before filtering, 0.0f is off,
      Q_Switch.gain(0, 0.0f); //  1.0f is Input source before filtering, 0.0f is off,
      // Turn on Tone  // This is a single tone fed into the CESSB compressor/mixer and output as I and Q
      I_Switch.gain(1, 1.0f); //  1.0f  Sinewave1 to FFT for test cal, 0.0f is off
      Q_Switch.gain(1, 1.0f); //  -1 for USB, +1 for LSB -  Sinewave1 to FFT for test cal, 0.0f is off  Tx is inverted in the FFT so USB looks like LSB
    #else
      // Turn On Inputs
      I_Switch.gain(0, 1.0f); //  1.0f is Input source before filtering, 0.0f is off,
      Q_Switch.gain(0, 1.0f); //  1.0f is Input source before filtering, 0.0f is off,
      // Turn OFF Tone
      I_Switch.gain(1, 0.0f); //  1.0f  Sinewave1 to FFT for test cal, 0.0f is off
      Q_Switch.gain(1, 0.0f); //  1.0f  Sinewave2 to FFT for test cal, 0.0f is off
    #endif
    
    AudioInterrupts();

    float32_t Pre_CESSB_Gain = 1.5f; // Sets the amount of clipping, 1.0 to 2.0f, 3 is excessive
    //pLevelData = cessb1.getLevels(0);  // Gets pointer to struct
    //Build the CESSB SSB transmitter
    cessb1.setSampleRate_Hz(sample_rate_Hz);  // Required
    // Set input, correction, and output gains
    cessb1.setGains(Pre_CESSB_Gain, 2.0f, 1.0f);
    //pLevelData = cessb1.getLevelsZ(0);  // Gets pointer to struct
    cessb1.setSideband(true);   // true reverses the sideband

    #ifdef IQ_CORRECTION_WITH_CESSB
      // Small corrections at the output end of this object can patch up hardware flaws.
      // _gI should be close to 1.0, _gXIQ and _gXQI should be close to 0.0.
      // xrossIQ and crossQI can be either + or -.  If you gainI -1.0, it will reverse the sidebands!!
      // One of either xrossIQ or crossQI should end up as 0.0 or you are fighting yourself.
      // If you set useIQCorrection to false, there is no processing used, so this is harmless
      // Only available in transmit audio flows with CESSB enabled.
      // bool _useCor, float32_t _gI, float32_t _gXIQ, float32_t _gXQI)
      //cessb1.setIQCorrections(true, 1.0, 0.0, 0.0);  // no correction, decide what you want to try ands edit
    #else
      //cessb1.setIQCorrections(false, 1.0, 0.0, 0.0);
    #endif 
    
    RX_Hilbert_Plus_45.begin(Hilbert_Plus45_40K, 151);   // Left channel Rx
    RX_Hilbert_Minus_45.begin(Hilbert_Minus45_40K, 151); // Right channel Rx
    RX_Summer.gain(0, 0.5f); // Set Beep Tone ON or Off and Volume
    RX_Summer.gain(1, 0.5f); // Set Beep Tone ON or Off and Volume

    Zoom(ZOOMx1);  // Zoom level for UI control - these are defined in the .h file, placed here for reference
                  //#define ZOOMx1      0       // Zoom out the most (fft1024 @ 48K) (aka OFF)  x1 reference
                  //#define ZOOMx2      1       // in between (fft2048)  is x2 of 1024
                  //#define ZOOMx4      2       // Zoom in the most (fft4096 at 96K)   is x4 of 1024
                  //#define ZOOM_NUM    3       // Number of zoom level choiced for menu system

    // -------------------- Setup our radio settings and UI layout --------------------------------

    spectrum_preset = 0;    
    spectrum_RA887x.Spectrum_Parm_Generator(0, 0, fft_bins); // use this to generate new set of params for the current window size values. 
                                                              // 1st arg is target, 2nd arg is current value
                                                              // calling generator before drawSpectrum() will create a new set of values based on the globals
                                                              // Generator only reads the global values, it does not change them or the database, just prints the new params
    spectrum_RA887x.drawSpectrumFrame(spectrum_preset); // Call after initSpectrum() to draw the spectrum object.  Arg is 0 PRESETS to load a preset record
                                                              // DrawSpectrum does not read the globals but does update them to match the current preset.
                                                              // Therefore always call the generator before drawSpectrum() to create a new set of params you can cut anmd paste.
                                                              // Generator never modifies the globals so never affects the layout itself.
                                                              // Print out our starting frequency for testing
    //sp.drawSpectrumFrame(6);   // for 2nd window using a new paramer record
    
    Serial.print(F("\nInitial Dial Frequency is "));
    Serial.print(formatVFO(VFOA));
    Serial.println(F("MHz"));
   
    // ---------------------------- Setup speaker on or off and unmute outputs --------------------------------
    if (1)
    {
        codec1.volume(1.0); // Set to full scale.  RampVolume will then scale it up or down 0-100, scaled down to 0.0 to 1.0
        // 0.7 seemed optimal for K7MDL with QRP_Labs RX board with 15 on line input and 20 on line output
        codec1.unmuteHeadphone();
        codec1.unmuteLineout(); //unmute the audio output
    }
    else
    {
        //codec1.muteHeadphone();
    }

    //changeBands(0);     // Sets the VFOs to last used frequencies, sets preselector, active VFO, other last-used settings per band.
                        // Call changeBands() here after volume to get proper startup volume

    //------------------Finish the setup by printing the help menu to the serial connections--------------------
    printHelp();
    InternalTemperature.begin(TEMPERATURE_NO_ADC_SETTING_CHANGES);
}

static uint32_t delta = 0;

//
// __________________________________________ Main Program Loop  _____________________________________
//
void loop()
{
    static uint32_t time_old = 0;

    // Update spectrum and waterfall based on timer
    if (spectrum_waterfall_update.check() == 1) // The update rate is set in drawSpectrumFrame() with spect_wf_rate from table
    {
        Freq_Peak = spectrum_RA887x.spectrum_update(
            0,                 // index to spectrum profile records, using record 0 here
            1,                 // VFOA active = 1, VFOB is 0 (deleted in modern version)
            VFOA + rit_offset, // for onscreen freq info
            VFOB,              // Not really needed today, spectrum never shows B, only the active
            ModeOffset,        // Move spectrum cursor to center or offset it by pitch value when in CW modes
            filterCenter,      // Center the on screen filter shaded area
            filterBandwidth,   // Display the filter width on screen
            pan,               // Pannng offset from center frequency
            fft_size,          // use this size to display for simple zoom effect
            fft_bin_size,      // pass along the calculated bin size
            fft_bins           // pass along the number of bins.  FOr IQ FFTs, this is fft_size, else fft_size/2
        ); 
      //Spectrum_RA887x::spectrum_update(int16_t s, int16_t VFOA_YES, int32_t VfoA, int32_t VfoB, int32_t Offset, uint16_t filterCenter, uint16_t filterBandwidth, float pan, uint16_t fft_sz, float fft_bin_sz, int16_t fft_binc)
    }
    
    // Time stamp our program loop time for performance measurement
    uint32_t time_n = millis() - time_old;

    if (time_n > delta)
    {
        delta = time_n;
        Serial.print(F("Tms="));
        Serial.println(delta);
    }
    time_old = millis();
    
    //respond to Serial commands
    while (Serial.available())
    {
        if (Serial.peek())
            respondToByte((char)Serial.read());
    }

    //check to see whether to print the CPU and Memory Usage
    if (enable_printCPUandMemory)
        printCPUandMemory(millis(), 3000); //print every 3000 msec

    //check to see whether to print the CPU and Memory Usage
    if (enable_printCPUandMemory)
        printCPUandMemory(millis(), 3000); //print every 3000 msec
}

//
// ----------------------- SUPPORT FUNCTIONS -------------------------------------------------------
//
//  Added in some useful tools for measuring main program loop time, memory usage, and an I2C scanner.
//  These are also used in my SDR program.
//
// -------------------------------------------------------------------------------------------------
//  Scans for any I2C connected devices and reports them to the serial terminal.  Usually done early in startup.
//
void I2C_Scanner(void)
{
  byte error, address; //variable for error and I2C address
  int nDevices;

  // uncomment these to use alternate pins
  //WIRE.setSCL(37);
  //WIRE.setSDA(36);
  //WIRE.begin();
  
  Serial.println(F("Scanning..."));

  nDevices = 0;
  for (address = 1; address < 127; address++ )
  {
    // The i2c_scanner uses the return value of
    // the Write.endTransmisstion to see if
    // a device did acknowledge to the address.
    Wire.beginTransmission(address);
    error = Wire.endTransmission();

    if (error == 0)
    {
      Serial.print(F("I2C device found at address 0x"));
      if (address < 16)
        Serial.print("0");
      Serial.print(address, HEX);
      Serial.print("  (");
      printKnownChips(address);
      Serial.println(")");
      //Serial.println("  !");
      nDevices++;
    }
    else if (error == 4)
    {
      Serial.print(F("Unknown error at address 0x"));
      if (address < 16)
        Serial.print("0");
      Serial.println(address, HEX);
    }
  }
  if (nDevices == 0)
    Serial.println(F("No I2C devices found\n"));
  else
    Serial.println(F("done\n"));

  //delay(500); // wait 5 seconds for the next I2C scan
}

// prints a list of known i2C devices that match a discovered address
void printKnownChips(byte address)
{
  // Is this list missing part numbers for chips you use?
  // Please suggest additions here:
  // https://github.com/PaulStoffregen/Wire/issues/new
  switch (address) {
    case 0x00: Serial.print(F("AS3935")); break;
    case 0x01: Serial.print(F("AS3935")); break;
    case 0x02: Serial.print(F("AS3935")); break;
    case 0x03: Serial.print(F("AS3935")); break;
    case 0x0A: Serial.print(F("SGTL5000")); break; // MCLK required
    case 0x0B: Serial.print(F("SMBusBattery?")); break;
    case 0x0C: Serial.print(F("AK8963")); break;
    case 0x10: Serial.print(F("CS4272")); break;
    case 0x11: Serial.print(F("Si4713")); break;
    case 0x13: Serial.print(F("VCNL4000,AK4558")); break;
    case 0x18: Serial.print(F("LIS331DLH")); break;
    case 0x19: Serial.print(F("LSM303,LIS331DLH")); break;
    case 0x1A: Serial.print(F("WM8731")); break;
    case 0x1C: Serial.print(F("LIS3MDL")); break;
    case 0x1D: Serial.print(F("LSM303D,LSM9DS0,ADXL345,MMA7455L,LSM9DS1,LIS3DSH")); break;
    case 0x1E: Serial.print(F("LSM303D,HMC5883L,FXOS8700,LIS3DSH")); break;
    case 0x20: Serial.print(F("MCP23017,MCP23008,PCF8574,FXAS21002,SoilMoisture")); break;
    case 0x21: Serial.print(F("MCP23017,MCP23008,PCF8574")); break;
    case 0x22: Serial.print(F("MCP23017,MCP23008,PCF8574")); break;
    case 0x23: Serial.print(F("MCP23017,MCP23008,PCF8574")); break;
    case 0x24: Serial.print(F("MCP23017,MCP23008,PCF8574")); break;
    case 0x25: Serial.print(F("MCP23017,MCP23008,PCF8574")); break;
    case 0x26: Serial.print(F("MCP23017,MCP23008,PCF8574")); break;
    case 0x27: Serial.print(F("MCP23017,MCP23008,PCF8574,LCD16x2,DigoleDisplay")); break;
    case 0x28: Serial.print(F("BNO055,EM7180,CAP1188")); break;
    case 0x29: Serial.print(F("TSL2561,VL6180,TSL2561,TSL2591,BNO055,CAP1188")); break;
    case 0x2A: Serial.print(F("SGTL5000,CAP1188")); break;
    case 0x2B: Serial.print(F("CAP1188")); break;
    case 0x2C: Serial.print(F("MCP44XX ePot")); break;
    case 0x2D: Serial.print(F("MCP44XX ePot")); break;
    case 0x2E: Serial.print(F("MCP44XX ePot")); break;
    case 0x2F: Serial.print(F("MCP44XX ePot")); break;
    case 0x33: Serial.print(F("MAX11614,MAX11615")); break;
    case 0x34: Serial.print(F("MAX11612,MAX11613")); break;
    case 0x35: Serial.print(F("MAX11616,MAX11617")); break;
    case 0x38: Serial.print(F("RA8875,FT6206")); break;
    case 0x39: Serial.print(F("TSL2561, APDS9960")); break;
    case 0x3C: Serial.print(F("SSD1306,DigisparkOLED")); break;
    case 0x3D: Serial.print(F("SSD1306")); break;
    case 0x40: Serial.print(F("PCA9685,Si7021")); break;
    case 0x41: Serial.print(F("STMPE610,PCA9685")); break;
    case 0x42: Serial.print(F("PCA9685")); break;
    case 0x43: Serial.print(F("PCA9685")); break;
    case 0x44: Serial.print(F("PCA9685, SHT3X")); break;
    case 0x45: Serial.print(F("PCA9685, SHT3X")); break;
    case 0x46: Serial.print(F("PCA9685")); break;
    case 0x47: Serial.print(F("PCA9685")); break;
    case 0x48: Serial.print(F("ADS1115,PN532,TMP102,PCF8591")); break;
    case 0x49: Serial.print(F("ADS1115,TSL2561,PCF8591")); break;
    case 0x4A: Serial.print(F("ADS1115")); break;
    case 0x4B: Serial.print(F("ADS1115,TMP102")); break;
    case 0x50: Serial.print(F("EEPROM")); break;
    case 0x51: Serial.print(F("EEPROM")); break;
    case 0x52: Serial.print(F("Nunchuk,EEPROM")); break;
    case 0x53: Serial.print(F("ADXL345,EEPROM")); break;
    case 0x54: Serial.print(F("EEPROM")); break;
    case 0x55: Serial.print(F("EEPROM")); break;
    case 0x56: Serial.print(F("EEPROM")); break;
    case 0x57: Serial.print(F("EEPROM")); break;
    case 0x58: Serial.print(F("TPA2016,MAX21100")); break;
    case 0x5A: Serial.print(F("MPR121")); break;
    case 0x60: Serial.print(F("MPL3115,MCP4725,MCP4728,TEA5767,Si5351")); break;
    case 0x61: Serial.print(F("MCP4725,AtlasEzoDO,DuPPaEncoder")); break;
    case 0x62: Serial.print(F("LidarLite,MCP4725,AtlasEzoORP,DuPPaEncoder")); break;
    case 0x63: Serial.print(F("MCP4725,AtlasEzoPH,DuPPaEncoder")); break;
    case 0x64: Serial.print(F("AtlasEzoEC,DuPPaEncoder")); break;
    case 0x66: Serial.print(F("AtlasEzoRTD,DuPPaEncoder")); break;
    case 0x67: Serial.print(F("DuPPaEncoder")); break;
    case 0x68: Serial.print(F("DS1307,DS3231,MPU6050,MPU9050,MPU9250,ITG3200,ITG3701,LSM9DS0,L3G4200D,DuPPaEncoder")); break;
    case 0x69: Serial.print(F("MPU6050,MPU9050,MPU9250,ITG3701,L3G4200D")); break;
    case 0x6A: Serial.print(F("LSM9DS1")); break;
    case 0x6B: Serial.print(F("LSM9DS0")); break;
    case 0x70: Serial.print(F("HT16K33")); break;
    case 0x71: Serial.print(F("SFE7SEG,HT16K33")); break;
    case 0x72: Serial.print(F("HT16K33")); break;
    case 0x73: Serial.print(F("HT16K33")); break;
    case 0x76: Serial.print(F("MS5607,MS5611,MS5637,BMP280")); break;
    case 0x77: Serial.print(F("BMP085,BMA180,BMP280,MS5611")); break;
    default: Serial.print(F("unknown chip"));
  }
}

const char* formatVFO(uint32_t vfo)
{
  static char vfo_str[25];
  
  uint16_t MHz = (vfo/1000000 % 1000000);
  uint16_t Hz  = (vfo % 1000);
  uint16_t KHz = ((vfo % 1000000) - Hz)/1000;
  sprintf(vfo_str, "%6d.%03d.%03d", MHz, KHz, Hz);
  //sprintf(vfo_str, "%13s", "45.123.123");
  //Serial.print("New VFO: ");Serial.println(vfo_str);
  return vfo_str;
}

//
// _______________________________________ Print CPU Stats, Adjsut Dial Freq ____________________________
//
//This routine prints the current and maximum CPU usage and the current usage of the AudioMemory that has been allocated
void printCPUandMemory(unsigned long curTime_millis, unsigned long updatePeriod_millis)
{
    //static unsigned long updatePeriod_millis = 3000; //how many milliseconds between updating gain reading?
    static unsigned long lastUpdate_millis = 0;

    //has enough time passed to update everything?
    if (curTime_millis < lastUpdate_millis)
        lastUpdate_millis = 0; //handle wrap-around of the clock
    if ((curTime_millis - lastUpdate_millis) > updatePeriod_millis)
    { //is it time to update the user interface?
        Serial.print(F("\nCPU Cur/Peak: "));
        Serial.print(audio_settings.processorUsage());
        Serial.print(F("%/"));
        Serial.print(audio_settings.processorUsageMax());
        Serial.println(F("%"));
        Serial.print(F("CPU Temperature:"));
        Serial.print(InternalTemperature.readTemperatureF(), 1);
        Serial.print(F("F "));
        Serial.print(InternalTemperature.readTemperatureC(), 1);
        Serial.println(F("C"));
        Serial.print(F(" Audio MEM Float32 Cur/Peak: "));
        Serial.print(AudioMemoryUsage_F32());
        Serial.print(F("/"));
        Serial.println(AudioMemoryUsageMax_F32());
        Serial.println(F("*** End of Report ***"));

        lastUpdate_millis = curTime_millis; //we will use this value the next time around.
        delta = 0;
        #ifdef I2C_ENCODERS
          //blink_MF_RGB();
          //blink_AF_RGB();
        #endif // I2C_ENCODERS
    }
}
//
// _______________________________________ Console Parser ____________________________________
//
//switch yard to determine the desired action
void respondToByte(char c)
{
    char s[2];
    s[0] = c;
    s[1] = 0;
    if (!isalpha((int)c) && c != '?')
        return;
    switch (c)
    {
    case 'h':
    case '?':
        printHelp();
        break;
    case 'C':
    case 'c':
        Serial.println(F("Toggle printing of memory and CPU usage."));
        togglePrintMemoryAndCPU();
        break;
    case 'M':
    case 'm':
        Serial.println(F("\nMemory Usage (FlexInfo)"));
        //flexRamInfo();
        Serial.println(F("*** End of Report ***"));
        break;
    default:
        Serial.print(F("You typed "));
        Serial.print(s);
        Serial.println(F(".  What command?"));
    }
}
//
// _______________________________________ Print Help Menu ____________________________________
//
void printHelp(void)
{
    Serial.println();
    Serial.println(F("Help: Available Commands:"));
    Serial.println(F("   h: Print this help"));
    Serial.println(F("   C: Toggle printing of CPU and Memory usage"));
    Serial.println(F("   M: Print Detailed Memory Region Usage Report"));
    Serial.println(F("   T+10 digits: Time Update. Enter T and 10 digits for seconds since 1/1/1970"));
}

//  Change FFT data source for zoom effects
void Change_FFT_Size(uint16_t new_size, float new_sample_rate_Hz)
{
    fft_size       = new_size; //  change global size to use for audio and display
    sample_rate_Hz = new_sample_rate_Hz;
    fft_bin_size   = sample_rate_Hz / (fft_size * 2);

    AudioNoInterrupts();
    // Route selected FFT source to one of the possible many FFT processors - should save CPU time for unused FFTs
    if (fft_size == 4096)
    {
        FFT_OutSwitch_I.setChannel(0); //  1 is 4096, 0 is Off
        FFT_OutSwitch_Q.setChannel(0); //  1 is 4096, 0 is Off
    }
    else if (fft_size == 2048)
    {
        FFT_OutSwitch_I.setChannel(1); //  1 is 2048, 0 is Off
        FFT_OutSwitch_Q.setChannel(1); //  1 is 2048, 0 is Off
    }
    else if (fft_size == 1024)
    {
        FFT_OutSwitch_I.setChannel(2); //  1 is 1024, 0 is Off
        FFT_OutSwitch_Q.setChannel(2); //  1 is 1024, 0 is Off
    }
    AudioInterrupts();
}

void TwinPeaks(void)
    {    
        TPinfo* pData;
        uint32_t timeSquareWave = 0;   // Switch every twoPeriods microseconds
        uint32_t twoPeriods;
        uint32_t tMillis = millis();
        #if SIGNAL_HARDWARE==TP_SIGNAL_CODEC
           Serial.println(F("Using SGTL5000 Codec output for cross-correlation test signal."));
        #endif
        #if SIGNAL_HARDWARE==TP_SIGNAL_IO_PIN
            pinMode (PIN_FOR_TP, OUTPUT);    // Digital output pin
           Serial.println(F("Using I/O pin for cross-correlation test signal."));
        #endif
        
        //TwinPeak.setLRfilter(true);
        //TwinPeak.setThreshold(TP_THRESHOLD);   Not used
        TwinPeak.stateAlignLR(TP_MEASURE);  // Comes up TP_IDLE

        #if SIGNAL_HARDWARE==TP_SIGNAL_IO_PIN
            twoPeriods = (uint32_t)(0.5f + (2000000.0f / sample_rate_Hz));
            // Note that for this hardware, the INO is 100% in charge of the PIN_FOR_TP
            pData = TwinPeak.read();       // Base data to check error
            while (pData->TPerror < 0  &&  millis()-tMillis < 2000)  // with timeout
            {
                if(micros()-timeSquareWave >= twoPeriods && pData->TPstate==TP_MEASURE)
                {
                    static uint16_t squareWave = 0;
                    timeSquareWave = micros();
                    squareWave = squareWave^1;
                    digitalWrite(PIN_FOR_TP, squareWave);
                }
                pData = TwinPeak.read();
            }
            // The update has moved from Measure to Run. Ground the PIN_FOR_TP
            TwinPeak.stateAlignLR(TP_RUN);  // TP is done, not TP_MEASURE
            digitalWrite(PIN_FOR_TP, 0);    // Set pin to zero

           Serial.println("");
           Serial.println(F("Update  ------------ Outputs  ------------"));
           Serial.println(F("Number  xNorm     -1        0         1   Shift Error State"));// Column headings
           Serial.print(pData->nMeas);Serial.print(",  ");
           Serial.print(pData->xNorm, 6);Serial.print(", ");
           Serial.print(pData->xcVal[3], 6);Serial.print(", ");
           Serial.print(pData->xcVal[0], 6);Serial.print(", ");
           Serial.print(pData->xcVal[1], 6);Serial.print(", ");
           Serial.print(pData->neededShift);Serial.print(",   ");
           Serial.print(pData->TPerror);Serial.print(",    ");
           Serial.println(pData->TPstate);
           Serial.println("");

            // You can see the injected signal level by theSerial.printed variable, pData->xNorm
            // It is the sum of the 4 cross-correlation numbers and if it is below 0.0001 the
            // measurement is getting noisy and un-reliable.  Raise the injected signal level
            // to solve the problem.
        #endif        
    }

void Zoom(uint8_t _zoom_Level)
{
    switch (_zoom_Level)
    {
        #ifdef FFT_1024
                case ZOOMx1:
                    Change_FFT_Size(1024, sample_rate_Hz);
                    break; // Zoom farthest in
        #endif
        #ifdef FFT_2048
                case ZOOMx2:
                    Change_FFT_Size(2048, sample_rate_Hz);
                    break; // Zoom farthest in
        #endif
        #ifdef FFT_4096
                case ZOOMx4:
                    Change_FFT_Size(4096, sample_rate_Hz);
                    break; // Zoom farthest in
        #endif
                default:
                    Change_FFT_Size(FFT_SIZE, sample_rate_Hz);
                    break; // Zoom farthest in
    }
}