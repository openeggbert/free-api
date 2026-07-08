//
// Created by robertvokac on 5/3/26.
//

#ifndef FREE_API_WINDOWS_WINGDI_H
#define FREE_API_WINDOWS_WINGDI_H

//#68
#define SRCCOPY 0x00CC0020

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

// Real Win32's <wingdi.h> wraps these two structs in #pragma pack(push, 2)
// specifically because BITMAPFILEHEADER's WORD+DWORD+WORD+WORD+DWORD layout
// would otherwise pick up 2 bytes of compiler-inserted padding after
// bfType, making it 16 bytes instead of the real, on-disk 14-byte BMP file
// header -- both games' _lopen/_lread palette-fallback path (ddutil.cpp)
// reads real .bmp asset files directly into these structs, so the packed
// layout is required for correct decoding, not just source compatibility.
#pragma pack(push, 2)

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

#pragma pack(pop)

//#1054
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

//#1454
/** @brief Packs RGB bytes into COLORREF. @note Status: IMPLEMENTED */
#define RGB(r, g, b) ((COLORREF)(((BYTE)(r) | ((WORD)((BYTE)(g)) << 8)) | (((DWORD)(BYTE)(b)) << 16)))

//#1509
#define BLACK_BRUSH 4

//#1540
#define CLR_INVALID 0xFFFFFFFF

//#1624
#define SIZEPALETTE 104

extern "C"{
//#2622
/**
 * @brief Creates a GDI bitmap from raw pixel data. Supports 8bpp (indexed,
 * expanded as greyscale -- no palette lookup, confirmed intentional per
 * TASK-24H-0602/0603: only reached by planetblupi's minimap in fullscreen
 * mode, which is not the shipped default), 16bpp (RGB565), and 32bpp.
 * @note Status: PARTIAL
 */
HBITMAP WINAPI CreateBitmap(int nWidth, int nHeight, UINT nPlanes, UINT nBitCount, const void* lpBits);

//#2627
/** @brief Allocates an internal memory DC. @note Status: PARTIAL */
HDC WINAPI CreateCompatibleDC(HDC hdc);

//#2691
/** @brief Deletes an internal compatible DC. @note Status: PARTIAL */
BOOL WINAPI DeleteDC(HDC hdc);

//#2693
/** @brief Deletes an internal compatible bitmap. @note Status: PARTIAL */
BOOL WINAPI DeleteObject(HGDIOBJ ho);

//#2886
/** @brief Always returns 0, matching real Win32's SIZEPALETTE contract on a
 * modern (non-palette) TrueColor host -- both games branch their
 * TrueColor-vs-palette rendering path on this. @note Status: IMPLEMENTED
 * (for the one index, SIZEPALETTE, either game queries) */
int WINAPI GetDeviceCaps(HDC hdc, int index);

//#2923
/** @brief Reads an RGB color from a surface DC or selected bitmap. @note Status: PARTIAL */
COLORREF WINAPI GetPixel(HDC hdc, int x, int y);

//#2931
/** @brief Fills palette entries as grayscale values. @note Status: PARTIAL */
UINT WINAPI GetSystemPaletteEntries(HDC hdc, UINT iStartIndex, UINT nEntries, LPVOID lppe);

//#3239
/** @brief Selects an internal bitmap into a memory DC; returns previously selected object. @note Status: PARTIAL */
HGDIOBJ WINAPI SelectObject(HDC hdc, HGDIOBJ h);

//#3260
/** @brief Writes an RGB color into a surface DC or selected bitmap. @note Status: PARTIAL */
COLORREF WINAPI SetPixel(HDC hdc, int x, int y, COLORREF color);

//#3264
/**
 * @brief Copies/scales a region from a source DC to a destination DC.
 *
 * Only SRCCOPY from a memory DC with selected bitmap to an internal surface DC
 * is implemented. Uses nearest-neighbor scaling. In the scaled path, an
 * out-of-range source coordinate is clamped to the nearest edge pixel on
 * both the X and Y axes (consistent edge-clamp policy on both axes).
 * @note Status: PARTIAL
 */
BOOL WINAPI StretchBlt(HDC hdcDest,
                       int xDest,
                       int yDest,
                       int wDest,
                       int hDest,
                       HDC hdcSrc,
                       int xSrc,
                       int ySrc,
                       int wSrc,
                       int hSrc,
                       DWORD rop);

//#3569
/** @brief Fills a BITMAP structure for an internal compatible bitmap. @note Status: PARTIAL */
int WINAPI GetObjectA(HANDLE h, int c, LPVOID pv);
}

#endif //FREE_API_WINDOWS_WINGDI_H
