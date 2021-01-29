/***********************************************************
     Testing slide show transitions.
     This version converts input .bmp file to RGB565 bitmap
     in buffer with line-by-line read.  This avoids the
     requirement for a buffer to hold the complete RGB888
     bitmap, which is 230,400 bytes.  If the PXP was used to
     convert the file, it would need a buffer of 307,200 bytes
     since the PXP cannot accept packed RGB888 24-bit values
     directly and they must be expanded to 32-bit ARGB8888 format.
*/


#include <SD.h>  // wants the latest version from TeensyDuino 1.54B4
#include <Arduino.h>
#include <stdint.h>
#include <stdio.h>
#include <cstdlib>
#include "Slideshow.h"
#include <PXP.h>
#include <Adafruit_GFX.h>
#include <ILI9341_t3n.h>
#include "ili9341_t3n_font_Arial.h"
//#include <PXP_Defs.h> // needed if Teensy core not updated

//Specify the pins used for Non-SPI functions
#define TFT_CS   10  // AD_B0_02
#define TFT_DC   9  // AD_B0_03
#define TFT_RST  8

ILI9341_t3n tft = ILI9341_t3n(TFT_CS, TFT_DC, TFT_RST);

uint32_t lastsample;
uint32_t imagesize = 230400;  // QVGA RGB888

elapsedMillis bltimer = 0;
elapsedMicros cvtimer;
uint32_t cvmicros;



#define NUMFILES 5
const char *fname0 = "BC Sunset.bmp";
const char *fname1 = "CortezAK.bmp";
const char *fname2 = "DSound.bmp";
const char *fname3 = "OrcasLanding.bmp";
const char *fname4 = "Tolumne.bmp";

const char *fnames[NUMFILES] = {fname0, fname1, fname2, fname3, fname4};

const char compileTime [] = " Compiled on " __DATE__ " " __TIME__;

/***************************************************************
//  set up  buffers of 164KB, since all bitmaps are QVGA RGB565
//  You can move these buffers to different memory areas to see the
//  effect on slide show timing.
//  If you do not have an attached PSRMA chip, use DMAMEM, DMAMEM DTCM
//  (DTCM  means no memory specifier after the buffer definition.)
//  !!!WARNING!!! putting everything  in EXTMEM causes a hang in slide show!
//  This bug is under investigation as of 1/28/2021
******************************************************************/
uint8_t abits[1024 * 164] DMAMEM;
uint8_t pbits[1024 * 164] DMAMEM;
uint8_t outbits[1024 * 164];

tKBStruct myKBs[10];  // allocate  10 KBstructs, but only using 0..5

enum timagetype  {CUSTOM, QQVGA, QVGA, VGA};

clSlideshow myshow;

// allocate Bitmap structures for output, processing and alpha surfaces
tBitmap outmap;
tBitmap psmap;
tBitmap asmap;


void setup() {
  Serial.begin(9600);
  delay(400);

  Serial.printf("\n\nPXP Slide Show Test %s\n", compileTime);
  Serial.println("Initializing.");
  // Start ILI9341 
  tft.begin();
  tft.setRotation(3);
  InitBitmaps();
  StartSDCard();
  InitMyKBs();

  myshow.SetFullFrame(); // set full frame 
  Serial.println("Starting Slide Show");
  DoShow(4); // 4 seconds per slide

}

void loop() {
  char ch;
  if(Serial.available()){
    ch = Serial.read();
    if(ch == 's') myshow.ShowStats();
    if(ch == 'd') CMDI(NULL);
    DoShow(4);
  }

}


// initialze start and end of pan and zoom--the Ken Burns Effect.
void InitMyKBs(void) {
  tKBStruct *kbp;
  kbp = &myKBs[0]; // zoom in to anchored boats
  kbp->startTop = 0; kbp->startLeft = 0;
  kbp->endTop = 18; kbp->endLeft = 20;
  kbp->startWidth = 319; kbp->endWidth = 230;
  kbp->displaySeconds = 8;

  kbp = &myKBs[1]; // pan down
  kbp->startTop = 00; kbp->startLeft = 76;
  kbp->endTop = 60; kbp->endLeft = 80;
  kbp->startWidth = 240; kbp->endWidth = 240;
  kbp->displaySeconds = 8;
  
  kbp = &myKBs[2]; // zoom out from  center
  kbp->startTop = 00; kbp->startLeft = 80;
  kbp->endTop = 30; kbp->endLeft = 30;
  kbp->startWidth = 319; kbp->endWidth = 200;

  kbp = &myKBs[3]; // zoom out from ferry
  kbp->startTop = 80; kbp->startLeft = 80;
  kbp->endTop = 00; kbp->endLeft = 00;
  kbp->startWidth = 200; kbp->endWidth = 319;
  kbp->displaySeconds = 6;

  kbp = &myKBs[4]; // Tolumne  Pan right
  kbp->startTop = 00; kbp->startLeft = 80;
  kbp->endTop = 00; kbp->endLeft = 00;
  kbp->startWidth = 260; kbp->endWidth = 260;

  kbp = &myKBs[5]; // zoom in to ferry
  kbp->startTop = 00; kbp->startLeft = 00;
  kbp->endTop = 40; kbp->endLeft = 40;
  kbp->startWidth = 319; kbp->endWidth = 240;

}


// set up for 320 x 240 bitmaps
#define MWIDTH 320
#define MHEIGHT 240
#define MTYPE PXP_RGB565
void InitBitmaps(void) {
  psmap.pbits = pbits; psmap.width = MWIDTH; psmap.height = MHEIGHT; psmap.maptype = MTYPE;
  memset(psmap.pbits, 0,sizeof(pbits));
  asmap.pbits = abits; asmap.width = MWIDTH; asmap.height = MHEIGHT; asmap.maptype = MTYPE;
  memset(asmap.pbits, 0,sizeof(abits));
  outmap.pbits = outbits; outmap.width = MWIDTH; outmap.height = MHEIGHT; outmap.maptype = MTYPE;
  memset(outmap.pbits, 0,sizeof(outbits));
  myshow.begin(&psmap, &asmap, &outmap); // init the slide show--which also inits PXP
  myshow.AttachOutput(&ILICallback);
}

bool StartSDCard() {

  if (!SD.begin(BUILTIN_SDCARD)) {
    Serial.println("\nSD File initialization failed.\n");
    return false;
  } else  Serial.println("initialization done.");

  return true;
}


// This function is callled by the slideshow to display the slides (or labels)
// The use of a callback function allows the output to be sent to a user-defined
// device.
void ILICallback(void *bmp) {
  tBitmap *ptrmap = (tBitmap *)bmp;
  tft.writeRect(0, 0, tft.width(), tft.height(), (uint16_t *)ptrmap->pbits);
  //  Serial.printf("ILICallback at %lu\n",millis());
}

void CMDI(void *cp) {
  Serial.println("\nSD Card Directory");
  SD.sdfs.ls(LS_DATE | LS_SIZE);
  Serial.println();
}


// do a demo slide show.  input parameter is seconds per slide
void DoShow(uint32_t ss){
  if (ss < 1) ss = 1;
  if (ss > 20) ss = 20;

  myshow.ClearStats();
  do {
    myshow.FadeOut();
    SlideLabel("DEMO SLIDE SHOW"); delay(2000);
    myshow.FadeOut();
    SlideLabel("Gulf Islands, BC");
    myshow.DissolveInFile(fnames[0]);  delay(ss * 1000);
    if (Serial.available()) break;
    SlideLabel("Near Cortez, Alaska");
    myshow.PushInFile(fnames[1], WLEFT); delay(ss * 1000);
    SlideLabel("Prideaux Haven, Desolation Sound");
    myshow.WipeInFile(fnames[2], WRIGHT);  delay(ss * 1000);
    if (Serial.available()) break;
    SlideLabel("Orcas Hotel, Orcas Island, WA");
    myshow.DissolveInFile(fnames[3]);   delay(ss * 1000);
    if (Serial.available()) break;
    SlideLabel("Tolumne Meadows, Yosemite NP");
    myshow.DissolveInFile(fnames[4]);  delay(ss * 1000);
    myshow.PushInFile(fnames[2],WRIGHT); delay(ss * 1000);
    myshow.FadeOut();
    SlideLabel("Pan and zoom effects"); delay(2000);
    myshow.FadeOut();
    myshow.KBFile(fnames[1],&myKBs[1]);
    if (Serial.available()) break;
    myshow.FadeOut( );
    myshow.StretchInFile(fnames[0]); delay(ss * 1000);
    myshow.FadeOut();
    myshow.KBFile(fnames[2],&myKBs[2]);
    if (Serial.available()) break;
    myshow.FadeOut();
    myshow.KBFile(fnames[3],&myKBs[3]);//zoom out
    myshow.KBFile(fnames[3],&myKBs[5]); // zoom back in
    if (Serial.available()) break;
    myshow.KBFile(fnames[0],&myKBs[0]);
    myshow.FadeOut();
    // do a fade in
    //    memset(pbits, 0, sizeof(pbits));  // clear the processing background
    myshow.KBFile(fnames[4],&myKBs[4]);  
    myshow.FadeOut();
    SlideLabel("Thanks for watching");delay(ss * 1000);
  } while (Serial.available() == 0);
  Serial.println("Slide show stopped.");
  
}


#define CENTER ILI9341_t3n::CENTER
void SlideLabel(const char *label) {

  // Put the text into the processing buffer
  tft.setFrameBuffer((uint16_t*)&pbits);
  tft.useFrameBuffer(true);
  tft.fillScreen(ILI9341_BLACK);
  tft.setFont(Arial_14);
  tft.setCursor(CENTER, CENTER);
  tft.setTextColor(ILI9341_WHITE);
  tft.print(label);

  arm_dcache_flush_delete((void *)pbits, sizeof(pbits));
  memcpy(abits, pbits, sizeof(pbits)); // save the label buffer
  tft.useFrameBuffer(false);

  arm_dcache_flush_delete((void *)abits, sizeof(abits));
  memcpy(pbits, abits, sizeof(pbits));  // restore processing surface
  myshow.ShowLabel();
  delay(2000);
}
