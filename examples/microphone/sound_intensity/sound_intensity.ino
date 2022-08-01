/*************************************************************
PDM Microphone example
By JV March 2019 Version 1.00

running volume calculation for VU-Meter usage.
Use 128 decimation for PCM signal.
Auto Gain the volume (keep track of everge max/min volume)

Turn on PLOTTER !!

*************************************************************/
#include <Adafruit_ZeroPDM.h>
#define DBG 1  // Debug info

// Definitions for Audio PDM Microphone
#define SAMPLERATE_HZ 9375
#define DECIMATION    128
#define PCMTIME 15           // sample evaluatiuon time in milliseconds
#define PCMTRESHOLD 4000
uint16_t sincfilter[DECIMATION] = {0,  0,  1,  2,  4,  7,  10, 15, 19, 25, 31, 39, 47, 56, 66, 77, 89, 103,  118,  134,  151,  170,  189,  211,  233,  258,  282,  309,  337,  366,  396,  428,  460,  493,  527,  562,  598,  634,  670,  707,  743,  780,  816,  852,  888,  922,  956,  988,  1021, 1050, 1079, 1105, 1131, 1153, 1176, 1193, 1211, 1224, 1237, 1245, 1253, 1255, 1258, 1255, 1253, 1245, 1237, 1224, 1211, 1193, 1176, 1153, 1131, 1105, 1079, 1050, 1021, 988,  956,  922,  888,  852,  816,  780,  743,  707,  670,  634,  598,  562,  527,  493,  460,  428,  396,  366,  337,  309,  282,  258,  233,  211,  189,  170,  151,  134,  118,  103,  89, 77, 66, 56, 47, 39, 31, 25, 19, 15, 10, 7,  4,  3,  2,  1,  1,  0,  0,  0};
Adafruit_ZeroPDM Mypdm = Adafruit_ZeroPDM(10, 7);   // Create PDM receiver object, with Clock and Data pins used clock=> PA10 = ~2  data => PA07 = A6  MKR1000 / MKR1010
#define PDM_REPEAT_LOOP_16(X) X X X X X X X X X X X X X X X X // a manual loop-unroller!

uint16_t avgl= 0x8000,tavgl = 0x8000, avgr= 0x8000, tavgr = 0x8000,Acoustic_Counter=0; // Global Avarege rolling Audio signal level
uint32_t minvoll=0xffffffff,minvolr=0xffffffff,maxvoll=0,maxvolr=0,avgvolume=0x00001000; // lowest volume level... probably Noise level
uint32_t VolumeLeft = 0, VolumeRight = 0;


void setup()
{

/*********** Serial SETUP  **********/
int t=10;  //Initialize serial and wait for port to open, max 10 second waiting
  Serial.begin(921600UL);
  while (!Serial) {
    ; delay(1000);
    if ( (t--)== 0 ) break;
  }
/*********** PDM over I2S Setup ************/
if (!Mypdm.begin()) {
    Serial.println("Failed to initialize I2S/PDM!");
    while (1);
   }
if (!Mypdm.configure(SAMPLERATE_HZ * DECIMATION/16, true)) {
    Serial.println("Failed to configure PDM");
    while (1);
  }
while ( Mypdm.read() ==0) Serial.print("."); // read till first sample are avaialble
Serial.println("PDM Test starts");            
}



void loop()
{
uint16_t vu_left=0,vu_right=0;             // vuMeter Variable
Read_AudioVolume(&vu_left,&vu_right);
Serial.println(vu_left);                   // turn on your plotter !!
}

// Read audio levels and convert to running Volume
// Rease I2S Samples, Reduce them by Decimator, calcualte volume over Ms Samples read
void Read_AudioVolume(uint16_t* leftchannel,uint16_t* rightchannel) {
uint16_t  t,peakl,peakr;
uint16_t runningsuml, runningsumr;

VolumeLeft=0;VolumeRight=0; // Relative volume per Sampled Time slot PCMTIME

  for (t = 0; t < (SAMPLERATE_HZ * PCMTIME) / 1000; ++t)
  {
    runningsuml=0; runningsumr=0;
    uint16_t *sinc_ptr = sincfilter; // pointer to 64bit FIR-filer cooeficients

    for (uint8_t samplenum = 0; samplenum < (DECIMATION / 16) ; samplenum++) {
      uint32_t sampler = Mypdm.read();    // we read 32 bits at a time, high and low 16bit stereo
      uint32_t samplel = 0xFFFF & (sampler >> 16);
      samplel = samplel & 0xFFFF;

      PDM_REPEAT_LOOP_16(      // manually unroll loop: for (int8_t b=0; b<16; b++)
      {
        if (samplel & 0x1) {            // start at the LSB which is the 'first' bit to come down the line, chronologically
          runningsuml += *sinc_ptr;     // do the convolution Left channel
        }
        if (sampler & 0x1) {            // start at the LSB which is the 'first' bit to come down the line, chronologically
          runningsumr += *sinc_ptr;     // do the convolution Right channel
        }
        sinc_ptr++;
        samplel >>= 1;                  // right shift :  I2S_SERCTRL_BITREV ,last input bit is at MSB
        sampler >>= 1;                  // right shift :  I2S_SERCTRL_BITREV last input bit is at MSB
      }
      )
     
    } // end decimation loop
    // we are only interested in the volume part: summarize the  Sample offset - average
   if ( runningsuml!=0 && runningsumr!=0 ) {
    avgl = (avgl * t + runningsuml) / (t + 1); // calculate running average
    avgr = (avgr * t + runningsumr) / (t + 1);
    peakl = abs(runningsuml - avgl);           // calcualte peak vs average
    peakr = abs(runningsumr - avgr);
       VolumeLeft += peakl;                     // summarize volume by adding all peaks
       VolumeRight += peakr;
   
    } // end sample process !=0
   
  } // end Sample For loop
 
  // process abs volume to relative volume
  if (VolumeLeft!=0 &&  VolumeRight!=0 ){
  avgvolume = (avgvolume*126 +VolumeLeft+VolumeRight)/128;               // average over 32 last samples = 32* 20ms = 1 seconde (uncontinous!)

      if ( maxvoll <= (2*avgvolume) ) maxvoll = 2*avgvolume;           // keep gap high enough : 100% vs Avg
      else  maxvoll = (255*maxvoll)/256;
      maxvolr = maxvoll;
      minvoll = (256*minvoll)/255;  // 1% increase
      minvolr= minvoll;
 
    // housekeeping min max and average
    if (VolumeLeft < minvoll ) minvoll = VolumeLeft;
    if (VolumeRight < minvolr ) minvolr = VolumeRight;
    if (VolumeLeft > maxvoll ) maxvoll = VolumeLeft;
    if (VolumeRight > maxvolr ) maxvolr = VolumeRight;  
    if(VolumeLeft>=PCMTRESHOLD ){*leftchannel =  (uint8_t) ( (100*(VolumeLeft-minvoll)) / (maxvoll-minvoll) ) ;  }
    else{ *leftchannel =  0x00; }
    if(VolumeRight>=PCMTRESHOLD ){ *rightchannel = (uint8_t) ( (100*(VolumeRight-minvolr)) / (maxvolr-minvolr) ) ; }
    else{ *rightchannel = 0x00 ; }
  }
//Serial.print(VolumeLeft);Serial.print(",");
//Serial.print(minvoll);Serial.print(",");
//Serial.print(maxvoll);Serial.print(",");
Serial.print(avgvolume);Serial.print(",");
//Serial.print((*leftchannel)*32);Serial.print(",");
//Serial.print((*rightchannel)*32);Serial.println(",");
}
