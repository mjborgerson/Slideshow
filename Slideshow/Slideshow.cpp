//  Slide show transitions and display
//  M. Borgerson   1/29/2021
#include <Arduino.h>
#include "Slideshow.h"
#include "PXP.h"   // requires new PXP from GITHUB

void clSlideshow::begin(tBitmap *prmap, tBitmap *almap, tBitmap *omap) {
  Serial.println("In Slideshow.begin()");
  ClearStats();
  ptprmap = prmap;
  ptalmap = almap;
  ptoutmap =  omap;
  pxp.begin();
  pxp.SetFlip(0);
  // Now pass these on to the PXP driver
  prmap->pixelbytes = (uint32_t)prmap->width * prmap->height * pxp.BytesPerPixel(prmap->maptype);
  pxp.SetPS(prmap->pbits, prmap->width, prmap->height, prmap->maptype);
  Serial.printf("prmap at %p  pbits = %p   wd = %u  ht = %u  maptype = %u\n",
                prmap, prmap->pbits, prmap->width, prmap->height, prmap->maptype );
  almap->pixelbytes = (uint32_t)almap->width * almap->height * pxp.BytesPerPixel(almap->maptype);
  pxp.SetAS(almap->pbits, almap->width, almap->height, almap->maptype);
  Serial.printf("almap at %p  pbits = %p   wd = %u  ht = %u  maptype = %u\n",
                almap, almap->pbits, almap->width, almap->height, almap->maptype );
  omap->pixelbytes = (uint32_t)omap->width * omap->height * pxp.BytesPerPixel(omap->maptype);
  pxp.SetOutput(omap->pbits, omap->width, omap->height, omap->maptype);
  Serial.printf("omap at %p  pbits = %p   wd = %u  ht = %u  maptype = %u\n",
                omap, omap->pbits, omap->width, omap->height, omap->maptype );
  Serial.println("End of clSlideshow.begin()");
  delay( 5);
}


// Set up the PXP to convert RGB565 image in pbits  to QVGA RGB565 in outbits
void clSlideshow::SetFullFrame(void) {
  uint32_t framebytes;
  //  Serial.printf("ptoutmap = %p\n", ptoutmap);
  //  delay(5);
  framebytes  = ptprmap->pixelbytes;

  delay(5);
  arm_dcache_flush_delete((void *)ptprmap->pbits, framebytes);
  arm_dcache_flush_delete((void *)ptalmap->pbits, framebytes);
  // now set scaling for 1x H and V
  //  Serial.println("Caches cleared"); delay(5);
  PXP_PS_CTRL_CLR = PXP_PS_CTRL;  // Clear PS buffer format
  pxp.SetPS((uint16_t *)ptprmap->pbits, ptprmap->width, ptprmap->height, ptprmap->maptype);
  pxp.SetAS((uint16_t *)ptalmap->pbits, ptalmap->width, ptalmap->height, ptalmap->maptype);
  pxp.SetOutput((uint16_t *)ptoutmap->pbits, ptoutmap->width, ptoutmap->height, ptoutmap->maptype);
  ovalpha = 0;
  pxp.SetOVRAlpha(ovalpha);
  pxp.SetScale(1.0);

  PXP_PS_PITCH = ptprmap->width * pxp.BytesPerPixel(ptprmap->maptype);
  PXP_AS_PITCH = ptalmap->width * pxp.BytesPerPixel(ptalmap->maptype);
  PXP_OUT_PITCH = ptoutmap->width * pxp.BytesPerPixel(ptoutmap->maptype);//320 * 2;

}

void clSlideshow::SetFileSystem(SdFs *fsp) {
  fsptr = fsp; // set our local copy of file system pointer
}

bool clSlideshow::GoodQVGA_RGB888(tBMPHDR888 *phdr) {
  bool result = true;
  //  Serial.printf("biWidth: %lu   biHeight: %lu\n", phdr->biWidth, phdr->biHeight );
  //  Serial.printf("biSizeImage: %lu \n", phdr->biSizeImage);
  if (phdr->biWidth != 320) result = false;
  if (abs(phdr->biHeight) != 240) result = false;
  if (phdr->biSizeImage != 230400) result = false;
  //  if(result) Serial.println("Good QVGA Header");
  return result;
}

// a private function called by public functions to display files
bool clSlideshow::BMPFileToBuffer( tBitmap *inmap, const char *fname) {
  bool result;
  File bmpfile;
  uint16_t *bufptr = inmap->pbits;
  uint32_t numread, totalread, numpixels;
  uint8_t  bytes[3];
  uint16_t pixel565, *wrptr, ht, wd, x, y;
  eumicros = 0;
  tBMPHDR888 fhdr;
  bmpfile = SD.open(fname, FILE_READ);
  if (!bmpfile) {
    Serial.printf("Could not open %s\n", fname);
    return false;
  }
  //  File is open, check for proper .bmp header
  numread = bmpfile.read(&fhdr, sizeof(fhdr));
  //Serial.printf(" and read %u header bytes\n", numread);
  if (!GoodQVGA_RGB888(&fhdr)) {
    Serial.printf("%s is not a properly formatted QVGA RGB888 .bmp file.\n", fname);
    return false;
  }
  totalread = 0;
  wd = fhdr.biWidth;
  ht = abs(fhdr.biHeight);
  // Try simple pixel-by-pixel conversion and see how long it takes before
  // resorting to more complex buffering schemes
  // pixels are saved in reverse order to correct image orientation
  for (y = ht; y > 0; y--) {
    wrptr = (uint16_t *)bufptr + (y - 1) * wd;
    for (x = 0; x < wd; x++) {
      numread = bmpfile.read(&bytes[0], 3); // read an RGB888 pixel
      totalread += numread;
      pixel565 = ((bytes[2] & 0xF8) << 8)  +
                 ((bytes[1] & 0xFC) << 3) +
                 (bytes[0] >> 3);
      *wrptr++ = pixel565;
    } // end of for(x= 0... loop

  } // end of for(y= ht...
  bmpfile.close();
  arm_dcache_flush_delete(  inmap->pbits, inmap->pixelbytes);
  AddtoFileStats(eumicros);
  return true;
}

//Run the PXP and refresh cache for output buffer
void clSlideshow::RunPXP(void) {
  eumicros = 0;
  arm_dcache_flush_delete(ptprmap->pbits, ptprmap->pixelbytes);
  pxp.Start();
  // wait until conversion finished
  while (!pxp.Done()) {};
  pxp.Stop();
  arm_dcache_flush_delete(ptoutmap->pbits, ptoutmap->pixelbytes);
  AddtoPXPStats(eumicros);
}

void clSlideshow::SetFlip(uint16_t flipVal) {
  pxp.SetFlip(flipVal);
}

// Dissolve successively blends ptalmap into current  ptprmap
// ps map and as map have to be set up before this is called
void clSlideshow::DoDissolve(void) {
  // place input in Alpha buffer and dissolve into processing buffer
  uint16_t i, dimalpha;
  uint32_t bitsize;
  elapsedMicros emt;
  ovalpha = 0;
  dimalpha = 0;  // set amount of image alfa surface in output
  //  pxp.SetAS(pnewmap->pbits, pnewmap->width, pnewmap->height, pnewmap->maptype);
  SetOutMap(ptoutmap);
  for (i = 0; i < 30; i++) {
    emt = 0;
    pxp.SetOVRAlpha(dimalpha);

    RunPXP();
    //  cvmicros = cvtimer;
    optr(ptoutmap);  // call the user output display function
    dimalpha += 8;
    while (emt < 50000) {} // limit to 20 Hz
  }
  SetFullFrame();
  memcpy(ptprmap->pbits, ptalmap->pbits, ptprmap->pixelbytes);
  pxp.SetOVRAlpha(255);
  RunPXP();
  eumicros = 0;
  optr(ptoutmap);
  AddtoOutputStats(eumicros);
}



// Set up the PXP_OUT_PS corners and scale
// so that source expands to fill output window, maintaining output aspect ratio
// but limiting so that source is within PS image.
// if output image is vertically flipped, adjust top and bottom values
void clSlideshow::SetBurns(uint16_t *psbuff, uint16_t left, uint16_t top, uint16_t width) {
  uint16_t dstright, dstbottom, ht ; //, srcbottom, dstbottom;
  uint32_t tloffset;
  bool vflip;
  float aspectratio, pscale;
  // destination right (output buffer surface) always at 0,0
  // destination always fills all of output frame
  dstright = (PXP_OUT_LRC >> 16); dstbottom = (PXP_OUT_LRC & 0x3FFF);
  aspectratio = float(dstright) / float(dstbottom); // normally 1.335
  vflip = PXP_CTRL & PXP_CTRL_VFLIP;
  // determine scale factor to make image fill output frame
  pscale = float(dstright) / float(width);
  ht = width / aspectratio; // pixel height of window
  PXP_PS_OFFSET = 0x0800080;  // sub pixel scaling of 1/2
  if (vflip) {
    top = 240 - ht - top;
  }
  tloffset = (top) * PXP_PS_PITCH + left * 2;
  // now we set the PS buffer address so that first pixel is the top left pixel
  PXP_PS_BUF = (uint8_t *)psbuff + tloffset;
  PXP_OUT_PS_ULC = 0;
  pxp.SetScale(pscale);
}

#define NUMSTEPS 80
void clSlideshow::KenBurns(tKBStruct *kbp) {  // always uses processing surface
  elapsedMillis emt;
  uint16_t i, left, top, width, numsteps;
  float fraction;
  uint32_t slideinterval;
  uint32_t stepinterval;
  tBitmap *pmap = ptprmap;
  if (kbp->displaySeconds == 0) {
    slideinterval = 4000;
  } else {
    slideinterval = kbp->displaySeconds * 1000;
  }
  numsteps = slideinterval / 50; // default 20Hz
  stepinterval = 50;
  SetFullFrame();
  ovalpha = 0;
  pxp.SetOVRAlpha(ovalpha);
  // do the pan and zoom in 80 steps over interval
  for (i = 0; i <=  numsteps; i++) {
    // change from start to end over numsteps
    fraction = (float)i / numsteps;
    left = kbp->startLeft + fraction * (kbp->endLeft - kbp->startLeft);
    top = kbp->startTop + fraction * (kbp->endTop - kbp->startTop);
    width = kbp->startWidth + fraction * (kbp->endWidth - kbp->startWidth);

    SetBurns((uint16_t *)pmap->pbits, left, top, width);
    emt = 0;
    RunPXP();
    eumicros = 0;
    optr(ptoutmap);
    AddtoOutputStats(eumicros);
    while (emt < stepinterval) {}; //minimum delay of stepinterval
  } // end of for( i=1 ....
}


void clSlideshow::SetOutMap(tBitmap *bmp) {
  ptoutmap = bmp;
  pxp.SetOutput(bmp->pbits, bmp->width, bmp->height, bmp->maptype);
}

// attach the callback function for output
void clSlideshow::AttachOutput(void (*ofptr)(void *)) {
  optr = ofptr;
}


void clSlideshow::KBFile(const char *fname, tKBStruct *kbp) {
  // put the file into processing bmp
  if (BMPFileToBuffer(ptprmap, fname)) {
    KenBurns(kbp);
  }
}

void clSlideshow::DissolveInFile(const char *fname) {
  SetFullFrame();
  if (BMPFileToBuffer(ptalmap, fname)) {
    DoDissolve(); // leaves new image in ptprmap
  }
}

void clSlideshow::ShowLabel(void) {
  // show label-----or anything else in processing surface buffer
  SetFullFrame();
  RunPXP();
  eumicros = 0;
  optr(ptoutmap);
  AddtoOutputStats(eumicros);
}

void clSlideshow::FadeInFile(const char *fname) {
  memset(ptprmap->pbits, 0, ptprmap->pixelbytes);
  if (BMPFileToBuffer(ptalmap, fname)) {
    DoDissolve(); // leaves new image in ptprmap
    memcpy(ptprmap->pbits, ptalmap->pbits, ptprmap->pixelbytes);
    arm_dcache_flush_delete((void *)ptprmap->pbits, ptprmap->pixelbytes);
    SetFullFrame();
    RunPXP();
    eumicros = 0;
    optr(ptoutmap);
    AddtoOutputStats(eumicros);
  }


}

void clSlideshow::FadeOut(void) {
  // fade current processing image to black
  memset(ptalmap->pbits, 0, ptalmap->pixelbytes);
  arm_dcache_flush_delete((void *)ptalmap->pbits, ptalmap->pixelbytes);
  DoDissolve();
}


void clSlideshow::StretchInFile(const char *fname) {
  uint16_t ulh, ulv, lrh, lrv, hinc, vinc;
  float sz;
  uint32_t oldulc, oldlrc;
  elapsedMicros emt;
  // read file into Processing buffer
  if (BMPFileToBuffer(ptprmap, fname)) {
    SetFullFrame();
    ovalpha = 0;
    pxp.SetOVRAlpha(ovalpha);
    PXP_PS_BACKGROUND_0 = 0;
    // save old PS corners which are same as output corners
    oldulc = PXP_OUT_PS_ULC;
    oldlrc = PXP_OUT_PS_LRC;
    ulh = ptoutmap->width / 2; // 1/2 of width
    ulv = ptoutmap->height / 2;

    lrh = ulh; lrv = ulv;
    hinc = 0.05 * (ulh); vinc = 0.05 * (ulv);
    for (sz = 0.05; sz < 1.0; sz += 0.05) {
      ulh -= hinc;  ulv -= vinc;
      lrh += hinc;  lrv += vinc;
      SetCorner(PXP_OUT_PS_LRC, lrh,lrv);
      SetCorner(PXP_OUT_PS_ULC, ulh, ulv);
      pxp.SetScale(sz);
      emt = 0;
      RunPXP();
      eumicros = 0;
      optr(ptoutmap);
      AddtoOutputStats(eumicros);
      while (emt < 50000) {}; //limit to 20Hz
    }
    SetFullFrame();
    RunPXP();
    optr(ptoutmap);  // call the user output display function
  }

}


// Wiping in a slide moves the new slide in over the top of
// the existing slide----which does not move
void clSlideshow::WipeInFile(const char *fname, tWipeDir dir) {
  // read file into Alpha buffer
  int32_t rpos;
  uint8_t *prbptr;
  uint16_t bpp;
  bpp = pxp.BytesPerPixel(ptalmap->maptype);
  prbptr = ptalmap->pbits;
  elapsedMicros emt;
  // put the new file into the alpha buffer
  SetFullFrame();
  bpp = pxp.BytesPerPixel(ptalmap->maptype);
  prbptr = ptalmap->pbits;
  if (BMPFileToBuffer(ptalmap, fname)) {
    ovalpha = 255;
    pxp.SetOVRAlpha(ovalpha);
    for (rpos = 0; rpos < 320; rpos += 8) {
      if (dir == WRIGHT) { // wiping left to right
        SetCorner(PXP_OUT_AS_LRC, (rpos) , 239);
      } else { // right to left
        prbptr = ptalmap->pbits + (320 - rpos) * bpp;
        PXP_AS_BUF = prbptr;
        SetCorner(PXP_OUT_AS_ULC, (320 - rpos), 0);
      }
      emt = 0;
      RunPXP();
      eumicros = 0;
      optr(ptoutmap);
      AddtoOutputStats(eumicros);
      while (emt < 50000) {}; //limit to 20Hz
    }


    // now copy the new data from AS to PS
    SetFullFrame();
    memcpy(ptprmap->pbits, ptalmap->pbits, ptprmap->pixelbytes);
    RunPXP();
    eumicros = 0;
    optr(ptoutmap);
    AddtoOutputStats(eumicros);
  }
}

// when we push in a slide, it pushes out the previous slide
// both the Alpha Surface and Processing surfaces have to move
// which adds some complexity
void clSlideshow::PushInFile(const char *fname, tWipeDir dir) {
  // read file into Alpha buffer
  int32_t rpos;
  uint8_t *prbptr;
  uint16_t bpp;
  elapsedMicros emt;
  // put the new file into the alpha buffer
  SetFullFrame();
  bpp = pxp.BytesPerPixel(ptprmap->maptype);
  prbptr = ptprmap->pbits;
  if (BMPFileToBuffer(ptalmap, fname)) {

    ovalpha = 255;
    pxp.SetOVRAlpha(ovalpha);

    for (rpos = 0; rpos < 320; rpos += 8) {
      if (dir == WLEFT) { // Pushing right to left
        SetCorner(PXP_OUT_PS_ULC,  0 , 0);
        prbptr = ptprmap->pbits + rpos * bpp;
        PXP_PS_BUF = prbptr;
        SetCorner(PXP_OUT_PS_LRC, (319 - rpos), 239);
        SetCorner(PXP_OUT_AS_ULC, (319 - rpos), 0);
      } else { // pshing left to right
        SetCorner(PXP_OUT_AS_ULC, 0 , 0);
        prbptr = ptalmap->pbits + (319 - rpos) * bpp;
        PXP_AS_BUF = prbptr;
        SetCorner(PXP_OUT_AS_LRC, (rpos), 239);
        SetCorner(PXP_OUT_PS_ULC, (rpos), 0);
      }
      emt = 0;
      RunPXP();
      eumicros = 0;
      optr(ptoutmap);
      AddtoOutputStats(eumicros);
      while (emt < 5000) {}; //limit to 200Hz
    }
    // now copy the new data from AS to PS
    SetFullFrame();
    memcpy(ptprmap->pbits, ptalmap->pbits, ptprmap->pixelbytes);
    RunPXP();
    optr(ptoutmap);  // call the user output display function
  }
}
// simple read and display of file
bool clSlideshow::DisplayFile(const char *fname) {
  eumicros = 0;
  if (BMPFileToBuffer(ptprmap, fname)) {
    pxp.SetOVRAlpha(0);
    RunPXP();
    eumicros = 0;
    optr(ptoutmap);
    AddtoOutputStats(eumicros);
    return true;
  } else return false;
}

/************************************
      float pxpsum, freadsum, outputsum;
      float pxpmin, pxpmax, freadmin,freadmax, outputmin,outputmax;
      uint32_t pxpcount, freadcount, outputcount;
**************************************** */
void clSlideshow::ShowStats(void) {
  Serial.println("\nSlide show operation timing in milliseconds.");
  Serial.printf("File Read and decode:   min: %6.3f  max: %6.3f  Avg: %6.3f\n",
                freadmin / 1000, freadmax / 1000, freadsum / freadcount / 1000);
  Serial.printf("PXP Conversion:         min: %6.3f  max: %6.3f  Avg: %6.3f\n",
                pxpmin / 1000, pxpmax / 1000, pxpsum / pxpcount / 1000);
  Serial.printf("Output to device:       min: %6.3f  max: %6.3f  Avg: %6.3f\n",
                outputmin / 1000, outputmax / 1000, outputsum / outputcount / 1000);
}


void clSlideshow::AddtoPXPStats(float fmicros) {
  pxpsum += fmicros;
  pxpcount++;
  if (fmicros < pxpmin) pxpmin = fmicros;
  if (fmicros > pxpmax) pxpmax = fmicros;
}


void clSlideshow::AddtoFileStats(float fmicros) {
  freadsum += fmicros;
  freadcount++;
  if (fmicros < freadmin) freadmin = fmicros;
  if (fmicros > freadmax) freadmax = fmicros;
}

void clSlideshow::AddtoOutputStats(float fmicros) {
  outputsum += fmicros;
  outputcount++;
  if (fmicros < outputmin) outputmin = fmicros;
  if (fmicros > outputmax) outputmax = fmicros;
}


void clSlideshow::ClearStats(void) {
  pxpsum = 0.0;  freadsum = 0.0; outputsum = 0.0;
  pxpmin = 65000.0;  freadmin = 65000.0; outputmin = 65000.0;
  pxpmax =  0.0;  freadmax = 0.0; outputmax = 0.0;
  pxpcount = 0;  freadcount = 0;  outputcount = 0;
}
