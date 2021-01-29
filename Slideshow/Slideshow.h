//  Slide show transitions and display
//  M. Borgerson   1/24/2021


#ifndef  SLIDESHOW_H
#define SLIDESHOW_H

#include <stdint.h>
#include <SD.h>
#include <PXP.h>


#ifdef __cplusplus
extern "C" {
#endif


// this file header structure needs to be packed because of the first uint16_t being
// followed by a uint32_t.  Without packing you get 2 0x00 bytes after the bfType.
// This form of default value initialization only works after C Version 11
// This default is set up for QVGA RGB888 bitmaps

# pragma pack (push, 2)
typedef struct  tBMPHDR888 {
  uint16_t  bfType = 0x4d42;   //'bm';
  uint32_t  bfSize = 230454;// 230400 pixels + 54 header
  uint16_t  bfReserved1 = 0;
  uint16_t  bfReserved2 = 0;
  uint32_t  bfOffBits =  54; // 14 bytes to here
  uint32_t  biSize = 40;   //Number of bytes in the DIB header (from this point)
  int32_t   biWidth = 320;
  int32_t   biHeight = -240;  // windows wants negative for top-down image
  int16_t   biPlanes = 1;
  uint16_t  biBitCount = 24 ;
  uint32_t  biCompression = 0;  // no compression
  uint32_t  biSizeImage = 230400;  // 320 * 240 * 3
  int32_t   biXPelsPerMeter = 0;
  int32_t   biYPelsPerMeter = 0;
  uint32_t  biClrUsed  = 0;
  uint32_t  biClrImportant = 0;// 54 bytes

} BMPHDR888;

enum tWipeDir {WLEFT, WRIGHT, WUP, WDOWN};

# pragma pack (pop)

// struct for specifying Ken Burns slide display
// Pan and zoom from starting top left and width
// to ending top, left and width.
// output matches output aspect ratio and fills
// output window
typedef struct tKBStruct {
  uint16_t startTop;
  uint16_t startLeft;
  uint16_t endTop;
  uint16_t endLeft;
  uint16_t startWidth;
  uint16_t endWidth;
  uint16_t displaySeconds;
} KBSType;


class clSlideshow
{
  protected:
  private:
    void (*optr)(void*);  // pointer to output callback parameter is bitmap ptr
    tBitmap *ptoutmap;
    tBitmap *ptalmap;
    tBitmap *ptprmap;
    uint16_t ovalpha;
    SdFs *fsptr = NULL;
    float pxpsum, freadsum, outputsum;
    float pxpmin, pxpmax, freadmin,freadmax, outputmin,outputmax;
    uint32_t pxpcount, freadcount, outputcount;
    elapsedMicros eumicros;

    void AddtoPXPStats(float  fmicros);
    void AddtoFileStats(float fmicros);
    void AddtoOutputStats(float  fmicros);

    void RunPXP(void);
    void SetBurns(uint16_t *psbuff, uint16_t left, uint16_t top, uint16_t width);
    bool GoodQVGA_RGB888(tBMPHDR888 *phdr);
    void SetOutMap(tBitmap *bmp);
    bool BMPFileToBuffer(tBitmap *inpmap, const char *fname);
    void DoDissolve(void);
    void KenBurns(tKBStruct *kbptr);  // always uses processing surface
  public:
    void begin(tBitmap *prmap, tBitmap *almap, tBitmap *omap);
    void SetFullFrame(void);
    void SetFlip(uint16_t flipVal);
    void AttachOutput(void (*ofptr)(void *));
    void SetFileSystem(SdFs *fsp);

    bool DisplayFile(const char *fname);
    void ShowLabel(void);
    void KBFile(const char *fname, tKBStruct *kbp);
    void DissolveInFile(const char *fname);
    void FadeInFile(const char *fname);
    void FadeOut(void);   // fade current processing image to black
    void StretchInFile(const char *fname);
    void WipeInFile(const char *fname, tWipeDir dir);
    void PushInFile(const char *fname, tWipeDir dir); 
    void ClearStats(void);
    void ShowStats(void);
};

#ifdef __cplusplus
}
#endif

#endif  // ifndef SLIDESHOW_H
