//
// Created by robertvokac on 5/3/26.
//

#ifndef FREE_API_WINDOWS_WINGDI_H
#define FREE_API_WINDOWS_WINGDI_H

/** @brief GDI bitmap metadata structure. @note Status: STUB */
typedef struct tagBITMAP {
    LONG bmType;
    LONG bmWidth;
    LONG bmHeight;
    LONG bmWidthBytes;
    WORD bmPlanes;
    WORD bmBitsPixel;
    LPVOID bmBits;
} BITMAP, *PBITMAP, *LPBITMAP;

/** @brief RGB color quad used by DIB/BMP formats. @note Status: STUB */
typedef struct tagRGBQUAD {
    BYTE rgbBlue;
    BYTE rgbGreen;
    BYTE rgbRed;
    BYTE rgbReserved;
} RGBQUAD, *LPRGBQUAD;

//815
/** @brief BMP info header. @note Status: STUB */
typedef struct tagBITMAPINFOHEADER {
    DWORD biSize;
    LONG biWidth;
    LONG biHeight;
    WORD biPlanes;
    WORD biBitCount;
    DWORD biCompression;
    DWORD biSizeImage;
    LONG biXPelsPerMeter;
    LONG biYPelsPerMeter;
    DWORD biClrUsed;
    DWORD biClrImportant;
} BITMAPINFOHEADER, *LPBITMAPINFOHEADER;

//940
/** @brief BMP file header. @note Status: STUB */
typedef struct tagBITMAPFILEHEADER {
    WORD bfType;
    DWORD bfSize;
    WORD bfReserved1;
    WORD bfReserved2;
    DWORD bfOffBits;
} BITMAPFILEHEADER, *LPBITMAPFILEHEADER;

//1431
#ifndef FREE_API_PALETTEENTRY_DEFINED
#define FREE_API_PALETTEENTRY_DEFINED
/** @brief Palette entry layout shared with DirectDraw. @note Status: PARTIAL */
typedef struct tagPALETTEENTRY {
    BYTE peRed;
    BYTE peGreen;
    BYTE peBlue;
    BYTE peFlags;
} PALETTEENTRY, *LPPALETTEENTRY;
#endif

#endif //FREE_API_WINDOWS_WINGDI_H
