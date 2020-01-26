// Set DPI awareness in the manifest file

#ifdef _DEBUG
// leak tracking
#define _CRTDBG_MAP_ALLOC  
#include <stdlib.h>  
#include <crtdbg.h> 

#ifndef DBG_NEW  
    #define DBG_NEW new ( _NORMAL_BLOCK , __FILE__ , __LINE__ )  
    #define new DBG_NEW  
#endif  
#endif

// Console app to take in a path, size, and output file
// It scans the path, picks a random image file,
// resizes the image, and saves it as a BMP. And many other tasks.

#define _WIN32_IE 0x0400
// ImgSource needs _WINDOWS defined to include text stuff
#define _WINDOWS 0x0400

#include <windows.h>
#include <wininet.h>
#include <shlobj.h>
#include <time.h>
#include <stdio.h>
#include <crtdbg.h>
#include <vector>
#include <conio.h>

#define TRACE(x) printf(x)
#define ASSERT _ASSERT

// Note you need ImgSource4.0 to use this - it will run
// in demo mode if you don't have a license.
// Note2: and this may be insurmountable, you need to have
// modified the library to work strictly in unicode, I didn't
// use the partial unicode support the author built in.
#include "D:\WORK\imgsource\4.0\islibs40_vs17_unicode\ISource.h"

#define MAXFILES 500000

wchar_t szFiles[MAXFILES][256];
BYTE *pTmp;
int iCnt, n, ret, errCount;
int nSkip;
FILE *fpSkip=NULL;
unsigned int idx1, idx2;
int COLORCHECK;
int AddFilename;
int ScaleMode;
int Background;
unsigned int iMinWidth, iMinHeight;
wchar_t szFolder[256];
wchar_t szOutfile[256];
wchar_t szBuf[256];
wchar_t szFillBuffName[256];
wchar_t *hquads=NULL;
wchar_t *vquads=NULL;
unsigned int iWidth, iHeight;
unsigned int inWidth, inHeight;
unsigned int outWidth, outHeight;
unsigned int finalW, finalH;
unsigned int drawOffsetX, drawOffsetY;	// used in the server+monitor case
int maxscale;
int minscale;
int currentx;
int currenty;
int currentw;
int currenth;
int maxerrorcount;
int totalpics;
int minmosaicx, minmosaicy;
int maxmosaicx, maxmosaicy;
HISSRC hSource, hDest;
HGLOBAL hBuffer, hBuffer2;
DWORD bgcolor;
LOGFONT myFont;
bool fMosaic, fStretch, fRandFill, fSequential, fFirstFill;
bool fNoDupes;
bool fCheckFlip;
bool fForce43=false;
bool fRandomSize=false;
bool fRandomQuads=false;
bool fSkipBlank=false;
bool fFirstFile;
bool fStartFirst;
bool fPinkAlpha;
bool fCoolSpacing;		// Secret flag for Cool Herders textures - finds character out-41 and pads it to 48x48 at the top left corner
bool fStopOnErr;		// stop before exit if an error was detected - possibly useful in large batch files to SEE the error
bool fBiggestFirst;
bool fSmallestFirst;
bool fBiggestXFirst;
bool fSmallestXFirst;
bool fBiggestYFirst;
bool fSmallestYFirst;
bool fMonitorsMode;
bool bGotOffs, bNeedOffs, bGotVpag, bNeedVpag;
BYTE *FillBuffer;
int exitcode=0;			// set for use by batch files
bool fArgsOK=true;
bool fYesIMeanIt=false;
bool fServer=false;		// used to serve files
bool fReturnCount=false;	// return code is count of mosaic files processed
bool fReturnFailed=false;	// return code is count of mosaic skipped/failed
HANDLE hServerEvent=INVALID_HANDLE_VALUE;
HANDLE hCompleteEvent=INVALID_HANDLE_VALUE;
HANDLE hTerminateEvent=INVALID_HANDLE_VALUE;
HWND hOutWnd=NULL;
int nFailedFiles=0;
int nDupeFiles=0;
int nBlankFiles=0;
int nFlippedFiles=0;
int nPerFileDelay=0;

extern HGLOBAL load_gif(wchar_t *filename, unsigned int *iWidth, unsigned int *iHeight);
// for the map file, we read this data from the png. ImageMagick adds the oFFs and vpAg chunks
int mapOrigW, mapOrigH, mapOffW, mapOffH;
wchar_t szMap[256];

int instr(wchar_t *, wchar_t*);
bool ScalePic(wchar_t *szFilename);
void BuildFileList(wchar_t *szFolder);
BOOL ResizeRGBBiCubic(BYTE *pImgSrc, UINT32 uSrcWidthPix, UINT32 uSrcHeight, BYTE *pImgDest, UINT32 uDestWidthPix, UINT32 uDestHeight);
void DoOneRun();
void ProcessMonitors();
void ProcessHQuads();
void ProcessVQuads();

// MY malloc is better than YOUR malloc
// allocate in fixed chunks to hopefully reduce fragmentation in server mode
#define MEGABYTE (1024*1024)
void* mymalloc(int bytecount) {
    // round everything up to 1MB, if it's bigger than 512k (Windows cutoff for different allocator) (supposedly)
    if (bytecount >= 512*1024) {
        bytecount = (bytecount+MEGABYTE-1)/MEGABYTE*MEGABYTE;
    }
    return malloc(bytecount);
}

// exit batchfile helper
void check(int code) {
	if ((code>0)&&(fStopOnErr)) {
		wprintf(L"\nError code %d - Press Enter to continue.", code);
        while (!_kbhit()) {
            Sleep(100);
        }
		wprintf(L"\n");
	}
	if (fReturnCount) {
		// return total successful pics
		exit(totalpics-nFailedFiles);
	} else if (fReturnFailed) {
		exit(nFailedFiles);
	} else {
		exit(code);
	}
}

// helper to open a file and get the image dimensions
// returns the number of pixels (w*h), 0 if can't open.
int GetImageSize(const wchar_t *pFile, UINT32 &w, UINT32 &h) {
	UINT32 bits, b, t, c, f, i, r, g, bl, gr, m;
	IS4DPIStruct dpi;
	RGBQUAD pal[256];
	BOOL in;

	w=0;
	h=0;

	HISSRC hSource=IS40_OpenFileSource(pFile);
	if (NULL==hSource) {
		return 0;
	}

	// guess filetype
	UINT32 ret=IS40_GuessFileType(hSource);
	IS40_Seek(hSource, 0, 0);

	switch (ret)
	{
	case 1: //bmp
		IS40_GetBMPDims(hSource, &w, &h, &bits, &dpi, 0);
		break;

	case 2: //gif
		IS40_GetGIFDims(hSource, &w, &h, &bits, (int*)&b, (int*)&t, pal, &in, 0);
		break;

	case 3: //jpg
		IS40_GetJPGDims(hSource, &w, &h, &bits, &c, &f, &dpi, 0);
		break;

	case 4: //png
		IS40_GetPNGDims(hSource, &w, &h, &bits, &c, &i, &t, &r, &g, &bl, &gr, &dpi, 0);
		break;

	case 5: //pcx
		IS40_GetPCXDims(hSource, &w, &h, &bits, &dpi, 0);
		break;

	case 6: //tif
		IS40_GetTIFFDims(hSource, 0, &w, &h, &bits, &dpi, 0);
		break;

	case 10: // psd
		IS40_GetPSDDims(hSource, &w, &h, &m, &dpi, 0);
		break;

	case 14: // tga
		IS40_GetTGADims(hSource, &w, &h, &bits, 0);
		break;

	default:
		w=0;
		h=0;
	}

	IS40_CloseSource(hSource);

	return w*h;
}


// qsorts for the file list
int fncomp(const void *p1, const void *p2) {
	return wcscmp((wchar_t*)p1, (wchar_t*)p2);
}
void SortListByName(int nCnt) {
	wprintf(L"Sorting file list...\n");
	qsort(szFiles, nCnt, 256, fncomp);
}

int bigcomp(const void *p1, const void *p2) {
	UINT32 w=0, h=0;
	int n1=GetImageSize((wchar_t*)p1, w, h);
	int n2=GetImageSize((wchar_t*)p2, w, h);
	return n2-n1;
}
void SortListByBiggest(int nCnt) {
	wprintf(L"Sorting file list by largest image...\n");
	qsort(szFiles, nCnt, 256, bigcomp);
}

int smallcomp(const void *p1, const void *p2) {
	// just swap the arguments
	return bigcomp(p2,p1);
}
void SortListBySmallest(int nCnt) {
	wprintf(L"Sorting file list by smallest image...\n");
	qsort(szFiles, nCnt, 256, smallcomp);
}

int bigxcomp(const void *p1, const void *p2) {
	UINT32 w1=0, w2=0, h=0;
	GetImageSize((wchar_t*)p1, w1, h);
	GetImageSize((wchar_t*)p2, w2, h);
	return w2-w1;
}
void SortListByBiggestX(int nCnt) {
	wprintf(L"Sorting file list by widest image...\n");
	qsort(szFiles, nCnt, 256, bigxcomp);
}

int smallxcomp(const void *p1, const void *p2) {
	// just swap the arguments
	return bigxcomp(p2,p1);
}
void SortListBySmallestX(int nCnt) {
	wprintf(L"Sorting file list by narrowest image...\n");
	qsort(szFiles, nCnt, 256, smallxcomp);
}

int bigycomp(const void *p1, const void *p2) {
	UINT32 w=0, h1=0, h2=0;
	GetImageSize((wchar_t*)p1, w, h1);
	GetImageSize((wchar_t*)p2, w, h2);
	return h2-h1;
}
void SortListByBiggestY(int nCnt) {
	wprintf(L"Sorting file list by tallest image...\n");
	qsort(szFiles, nCnt, 256, bigycomp);
}

int smallycomp(const void *p1, const void *p2) {
	// just swap the arguments
	return bigxcomp(p2,p1);
}
void SortListBySmallestY(int nCnt) {
	wprintf(L"Sorting file list by shortest image...\n");
	qsort(szFiles, nCnt, 256, smallycomp);
}

// Draws outlined text
void MyDrawText(void *pIn, int w, int h, wchar_t *myStr, int x, int y)
{
	BYTE *pRGB=(BYTE*)pIn;
	unsigned int tw, th;
	RECT myRect;

	myRect.top=0;
	myRect.left=0;
	myRect.right=w;
	myRect.bottom=h;

	IS40_GetTextLineSize2(myStr, &myFont, &myRect, DT_NOPREFIX|DT_SINGLELINE, 0, &tw, &th);

	if (x-(tw/2) < 0) return;
	
	myRect.right=x+(tw/2)+2;
	myRect.bottom=y+2;

	// top left
	myRect.top=y-th;
	myRect.left=x-(tw/2);
	IS40_DrawTextOnRGB2(pRGB, w, h, w*3, myStr, &myFont, &myRect, DT_NOPREFIX|DT_SINGLELINE|DT_TOP|DT_LEFT, 0, 0);

	// center left
	myRect.top++;
	IS40_DrawTextOnRGB2(pRGB, w, h, w*3, myStr, &myFont, &myRect, DT_NOPREFIX|DT_SINGLELINE|DT_TOP|DT_LEFT, 0, 0);

	// bottom left
	myRect.top++;
	IS40_DrawTextOnRGB2(pRGB, w, h, w*3, myStr, &myFont, &myRect, DT_NOPREFIX|DT_SINGLELINE|DT_TOP|DT_LEFT, 0, 0);

	// top center
	myRect.top-=2;
	myRect.left++;
	IS40_DrawTextOnRGB2(pRGB, w, h, w*3, myStr, &myFont, &myRect, DT_NOPREFIX|DT_SINGLELINE|DT_TOP|DT_LEFT, 0, 0);

	// bottom center
	myRect.top+=2;
	IS40_DrawTextOnRGB2(pRGB, w, h, w*3, myStr, &myFont, &myRect, DT_NOPREFIX|DT_SINGLELINE|DT_TOP|DT_LEFT, 0, 0);

	// top right
	myRect.top-=2;
	myRect.left++;
	IS40_DrawTextOnRGB2(pRGB, w, h, w*3, myStr, &myFont, &myRect, DT_NOPREFIX|DT_SINGLELINE|DT_TOP|DT_LEFT, 0, 0);

	// center right
	myRect.top++;
	IS40_DrawTextOnRGB2(pRGB, w, h, w*3, myStr, &myFont, &myRect, DT_NOPREFIX|DT_SINGLELINE|DT_TOP|DT_LEFT, 0, 0);

	// bottom right
	myRect.top++;
	IS40_DrawTextOnRGB2(pRGB, w, h, w*3, myStr, &myFont, &myRect, DT_NOPREFIX|DT_SINGLELINE|DT_TOP|DT_LEFT, 0, 0);

	// center (white)
	myRect.top--;
	myRect.left--;
	IS40_DrawTextOnRGB2(pRGB, w, h, w*3, myStr, &myFont, &myRect, DT_NOPREFIX|DT_SINGLELINE|DT_TOP|DT_LEFT, 0, 0x00ffffff);
}

// scan hBuffer for rectangles. Set currentx,y,w,h to the largest one
// Ignore rects smaller than inMinX, inMinY (unless FillBuffer was NULL)
// return number of rectangles found.
int EnumerateRectangles(int inMinX, int inMinY) {
	int nRects;
	bool bSpecialRet=false;
	struct {
		int x,y,w,h;
	} FoundRects[128];		// this should be more than enough
	int x,y, tx, ty, th, bx, by, idx, big;

	if (NULL == FillBuffer) {
		FillBuffer=(BYTE*)mymalloc(iWidth*(iHeight+1));	// 8-bit buffer just to mark used areas
		ZeroMemory(FillBuffer, iWidth*(iHeight+1));
		currentx=0;
		currenty=0;
		currentw=iWidth;
		currenth=iHeight;
		return 1;	// one rect - the whole buffer!
	}

	nRects=0;

	for (y=0; (y<(signed)iHeight)&&(nRects<128); y++) {
		x=0; 
		while (x<(signed)iWidth) {
			if (FillBuffer[y*iWidth+x] != 0) {
				x++;
				continue;
			}
			// found an opening, find the top left
			// rather than tracing a single line, we create a vertical bar,
			// and scan the entire bar for a stop point
			ty=y;
			tx=x;
			// so first, find the top of this line
			while ((ty>=0) && (FillBuffer[ty*iWidth+tx]==0)) {
				ty--;
			}
			// hit our stop, roll back by one
			ty++;
			// now find the bottom of the bar
			th=1;
			while (((ty+th)<(signed)iHeight) && (FillBuffer[(ty+th)*iWidth+tx]==0)) {	
				th++;
			}
			// Don't roll back cause it's height, not position
			// now scan the entire bar back to the left till we hit something
			bool bFlag=true;
			while (bFlag) {
				tx--;		// we can start like this because we already know the current bar is good
				if (tx < 0) break;
				for (idx=ty; idx<ty+th; idx++) {
					if (FillBuffer[idx*iWidth+tx] != 0) {
						bFlag=false;
						break;
					}
				}
			}
			// hit the limit, roll back one
			tx++;

			// Now we have a top left of this rect. See if we already mapped it
			for (idx=0; idx<nRects; idx++) {
				if ((FoundRects[idx].x==tx)&&(FoundRects[idx].y==ty)) {
					// yep, already checked this hole. 
					// this seems like a bug, but we sometimes spin if we don't check
					if (FoundRects[idx].x+FoundRects[idx].w > x) {
						x=FoundRects[idx].x+FoundRects[idx].w;
					} else {
						x++;
					}
					idx=9999;
				}
			}
			if (idx >= 9999) {
				// seen this one, move on
				continue;
			}

			// Find the bottom right. We already have the height of the bar, so just scan
			bx=tx;
			by=ty+th-1;
			bFlag=true;
			while (bFlag) {
				bx++;		// we can start like this because we already know the current bar is good
				if (bx >= (signed)iWidth) break;
				for (idx=ty; idx<by; idx++) {
					if (FillBuffer[idx*iWidth+bx] != 0) {
						bFlag=false;
						break;
					}
				}
			}
			bx--;

			// verify that this rect is large enough to consider
			if (((int)(bx-tx+1)>=inMinX) && ((int)(by-ty+1)>=inMinY)) {
				// Now we have our rect. Just update the array and find the next one.
				FoundRects[nRects].x=tx;
				FoundRects[nRects].y=ty;
				FoundRects[nRects].w=bx-tx+1;
				FoundRects[nRects].h=by-ty+1;
				
				wprintf(L"--- found rect (%d): %d,%d %dx%d\n", nRects, tx, ty, bx-tx+1, by-ty+1);

				nRects++;
				if (nRects >= 128) {
					break;
				}
			} else {
//				wprintf(L"Skipped rect at %d,%d, %dx%d\n", tx, ty, bx-tx+1, by-ty+1);
				if (-1 == maxscale) {
					bSpecialRet=true;
				}
			}
			// checked this hole. Scan to see if there's any more
			x++;
			if (bx >= x) x=bx+1;
		}
	}

	if (nRects > 0) {
		// Now which one is biggest?
		if (fRandFill) {
			big=rand()%nRects;
		} else if (fFirstFill) {
			big=0;
		} else {
			big=0;
			for (idx=1; idx<nRects; idx++) {
				if ((FoundRects[idx].w+FoundRects[idx].h)/2 > (FoundRects[big].w+FoundRects[big].h)/2) {
					// Could be more intelligence here - check aspect ratio of the hole against the image, etc
					// To find a more intelligent fit, not just the biggest hole.
					big=idx;
				}
			}
		}
		currentx=FoundRects[big].x;
		currenty=FoundRects[big].y;
		currentw=FoundRects[big].w;
		currenth=FoundRects[big].h;
	
		wprintf(L"*** Total %d rects, selected #%d\n", nRects, big);
	} else {
		big=0;
		if (bSpecialRet) {
			wprintf(L"*** No appropriate space found\n");
			nRects=-1;
			exitcode=99;
		} else {
			wprintf(L"*** All available space filled\n");
			exitcode=99;
		}
	}


	return nRects;
}

// try to read the image magick offset and virtual page chunks
// oFFs - 4 bytes of x offset (big endian), then 4 bytes for y offset
// vpAg - 4 bytes of width (big endian), then 4 bytes of height
// need to scan the whole file - new versions of ImageMagick stick it at the end
void ParseImageMagickChunks(wchar_t *szFileName) {
	FILE *fp;
	wchar_t buf[16];
	int pos=0;

	if (L'\0' == szMap[0]) {
		// don't bother looking if we aren't making a map
		return;
	}
	
	mapOrigW=0;
	mapOrigH=0;
	mapOffW=0;
	mapOffH=0;

	fp=_wfopen(szFileName, L"rb");
	if (NULL == fp) {
		wprintf(L"Couldn't check for virtual page information.\n");
		return;
	}

	memset(buf, 0, sizeof(buf));
	fread(buf, 1, 16, fp);

	// make sure it's PNG
	if ((buf[1]!='P')||(buf[2]!='N')||(buf[3]!='G')) {
		// not PNG
		fclose(fp);
		return;
	}

	pos=8;
	// chunk search ;)
	while (!feof(fp)) {
		fseek(fp, pos, SEEK_SET);
		memset(buf, 0, sizeof(buf));
		fread(buf, 1, 16, fp);		// gets enough data for chunk header and desired data

		int len=(buf[0]<<24)|(buf[1]<<16)|(buf[2]<<8)|(buf[3]);
		if (len < 1) break;
		DWORD tag=(buf[4]<<24)|(buf[5]<<16)|(buf[6]<<8)|(buf[7]);
		switch (tag) {
			case 'oFFs':
				mapOffW=(buf[8]<<24)|(buf[9]<<16)|(buf[10]<<8)|(buf[11]);
				mapOffH=(buf[12]<<24)|(buf[13]<<16)|(buf[14]<<8)|(buf[15]);
				wprintf(L"Got oFFs chunk\n");
				bGotOffs=true;
				break;

			case 'vpAg':
				mapOrigW=(buf[8]<<24)|(buf[9]<<16)|(buf[10]<<8)|(buf[11]);
				mapOrigH=(buf[12]<<24)|(buf[13]<<16)|(buf[14]<<8)|(buf[15]);
				wprintf(L"Got vpAg chunk\n");
				bGotVpag=true;
				break;
		}

		// dunno why we need the extra +4, didn't read the spec ;)
		pos+=len+12;
	}
}

int wmain(int argc, wchar_t *argv[])
{

#ifdef _DEBUG
	_CrtSetDbgFlag ( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF );  
#endif

	// initialize
#include "imgsourcekey.txt"
    IS40_Initialize(IS40GUID);

	// Improve randomness for slightly different strings by adding the commands line
	// Good for batch files that run multiple times in the same second?
	unsigned int nTmpVal=0;
	for (int idx=0; idx<argc-1; idx++) {
		int p=0;
		while (argv[idx][p]) {
			nTmpVal+=argv[idx][p++];
		}
	}
	srand((unsigned)time(NULL)+nTmpVal);

	myFont.lfHeight=0;
	myFont.lfWeight=0;
	myFont.lfEscapement=0;
	myFont.lfOrientation=0;
	myFont.lfWeight=FW_DONTCARE;
	myFont.lfItalic=false;
	myFont.lfUnderline=false;
	myFont.lfStrikeOut=false;
	myFont.lfCharSet=DEFAULT_CHARSET;
	myFont.lfOrientation=OUT_DEFAULT_PRECIS;
	myFont.lfClipPrecision=CLIP_DEFAULT_PRECIS;
	myFont.lfQuality=DEFAULT_QUALITY;
	myFont.lfPitchAndFamily=DEFAULT_PITCH|FF_DONTCARE;
	wcscpy(myFont.lfFaceName, L"");

	// Set defaults
	// Check if the program argument has a path. If so, we'll set that path instead
	wchar_t path[_MAX_PATH];
	wchar_t drive[_MAX_DRIVE];
	path[0]=L'\0';
	drive[0]=L'\0';
	_wsplitpath(argv[0], drive, path, NULL, NULL);
	if (wcslen(path)) {
		int len;

		wcscpy(szFolder, drive);
		len=wcslen(szFolder);
		wcsncpy(&szFolder[len], path, 256-len);
		szFolder[255]=L'\0';
	} else {
		wcscpy(szFolder, L".");
	}
	
	wcscpy(szOutfile, L"");
	wcscpy(szFillBuffName, L"");
	GetWindowsDirectory(szOutfile, 245);
	wcscat(szOutfile, L"\\Backdrop.bmp");

	wcscpy(szMap, L"");

	COLORCHECK=0;
	AddFilename=0;
	Background=0;
	fMosaic=false;
	fSkipBlank=false;
	fSequential=false;
	fNoDupes=false;
	fCheckFlip=false;
	fStretch=false;
	fYesIMeanIt=false;
	fServer=false;
	fForce43=false;
	fRandFill=false;
	fFirstFill=false;
	fRandomSize=false;
	fFirstFile=true;	// used for bgcolor
	fStartFirst=false;
	fPinkAlpha=false;
	fCoolSpacing=false;
	fStopOnErr=false;
	fReturnCount=false;
	fReturnFailed=false;
	fBiggestFirst=false;
	fSmallestFirst=false;
	fBiggestXFirst=false;
	fSmallestXFirst=false;
	fBiggestYFirst=false;
	fSmallestYFirst=false;
	fMonitorsMode=false;
	bNeedOffs=false;
	bNeedVpag=false;
	minmosaicx=160;
	minmosaicy=120;
	maxmosaicx=999999;
	maxmosaicy=999999;
	bgcolor=0;	// 00RRGGBB
	
	iWidth=GetSystemMetrics(SM_CXSCREEN);
	iHeight=GetSystemMetrics(SM_CYSCREEN);
	iMinWidth=0;
	iMinHeight=0;
	maxerrorcount=6;
	totalpics=0;
	mapOrigW=0;
	mapOrigH=0;
	mapOffW=0;
	mapOffH=0;
	
	drawOffsetX = 0;
	drawOffsetY = 0;
	
	FillBuffer=NULL;

	maxscale=99999;
	minscale=0;

	// get arguments
	for (iCnt=1; iCnt<argc; iCnt++) {
		if (!_wcsnicmp(argv[iCnt], L"path=", 5)) {
            // defer this to after...
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"map=", 4)) {
			wcscpy(szMap, &argv[iCnt][4]);
			wprintf(L"Will generate image map for mosaic\n");
			// delete old file
			FILE *fp=_wfopen(szMap, L"w");
			fclose(fp);
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"outwidth=", 9)) {
			iWidth=_wtoi(&argv[iCnt][9]);
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"outheight=", 10)) {
			iHeight=_wtoi(&argv[iCnt][10]);
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"minwidth=", 9)) {
			iMinWidth=_wtoi(&argv[iCnt][9]);
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"minheight=", 10)) {
			iMinHeight=_wtoi(&argv[iCnt][10]);
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"outfile=", 8)) {
			wcscpy(szOutfile, &argv[iCnt][8]);
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"fillbuf=", 8)) {
			wcscpy(szFillBuffName, &argv[iCnt][8]);
			continue;
		}
		
		if (!_wcsnicmp(argv[iCnt], L"minmosaicx=", 11)) {
			minmosaicx=_wtoi(&argv[iCnt][11]);
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"minmosaicy=", 11)) {
			minmosaicy=_wtoi(&argv[iCnt][11]);
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"maxmosaicx=", 11)) {
			maxmosaicx=_wtoi(&argv[iCnt][11]);
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"maxmosaicy=", 11)) {
			maxmosaicy=_wtoi(&argv[iCnt][11]);
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"maxscale=", 9)) {
			maxscale=_wtoi(&argv[iCnt][9]);
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"minscale=", 9)) {
			minscale=_wtoi(&argv[iCnt][9]);
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"hwnd=", 5)) {
			hOutWnd=(HWND)_wtoi(&argv[iCnt][5]);
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"filedelay=", 10)) {
			nPerFileDelay=_wtoi(&argv[iCnt][10]);
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"bgfirst", 7)) {
			bgcolor=0x80000000;
			wprintf(L"Will set background color from first pixel\n");
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"pinkalpha", 9)) {
			fPinkAlpha=true;
			bgcolor=0x00ff00ff;
			wprintf(L"Transparent parts of PNG files will be colored #FF00FF\n");
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"coolspacing", 11)) {
			fCoolSpacing=true;
			wprintf(L"Using Cool Herders spacing\n");
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"stoponerr", 11)) {
			fStopOnErr=true;
			wprintf(L"Will stop if a fatal error occurred\n");
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"noscale", 7)) {
			maxscale=-1;
			minscale=0;
			continue;
		}
		if (!_wcsnicmp(argv[iCnt], L"maxerr=", 7)) {
			maxerrorcount=_wtoi(&argv[iCnt][7]);
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"color", 5)) {
			COLORCHECK=1;
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"background", 9)) {
			Background=1;
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"filename", 8)) {
			AddFilename|=1;
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"alwaysfilename", 8)) {
			AddFilename|=2;
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"mosaic", 6)) {
			wprintf(L"Will build mosaic\n");
			fMosaic=true;
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"skipblank", 9)) {
			wprintf(L"Will skip blank images\n");
			fSkipBlank=true;
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"sequential", 10)) {
			wprintf(L"Will use sequential images if mosaic is set\n");
			fSequential=true;
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"firstfile", 9)) {
			wprintf(L"Will start from first file in file list (ascii sort)\n");
			fStartFirst=true;
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"nodupes", 7)) {
			wprintf(L"Will avoid placing duplicate images (slow)\n");
			fNoDupes=true;
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"checkflip", 7)) {
			wprintf(L"Will avoid placing flipped images (slow)\n");
			fCheckFlip=true;
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"stretch", 7)) {
			wprintf(L"Will stretch last image.\n");
			fStretch=true;
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"biggestfirst", 12)) {
			wprintf(L"Will sort larger images first.\n");
			fBiggestFirst=true;
			fSequential=true;
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"smallestfirst", 13)) {
			wprintf(L"Will sort smaller images first.\n");
			fSmallestFirst=true;
			fSequential=true;
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"bigxfirst", 9)) {
			wprintf(L"Will sort wider images first.\n");
			fBiggestXFirst=true;
			fSequential=true;
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"smallxfirst", 11)) {
			wprintf(L"Will sort narrower images first.\n");
			fSmallestXFirst=true;
			fSequential=true;
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"bigyfirst", 9)) {
			wprintf(L"Will sort taller images first.\n");
			fBiggestYFirst=true;
			fSequential=true;
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"smallyfirst", 11)) {
			wprintf(L"Will sort shorter images first.\n");
			fSmallestYFirst=true;
			fSequential=true;
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"force43", 7)) {
			fForce43=true;
			// Calculate factors based on the primary monitor
			int w=GetSystemMetrics(SM_CXSCREEN);
			int h=GetSystemMetrics(SM_CYSCREEN);
			if ((w*100)/h == 133) {
				wprintf(L"Primary monitor is already 4:3, not enforcing ratio.\n");
				fForce43=false;
			}
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"YesIMeanIt", 10)) {
			fYesIMeanIt=true;
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"Server", 10)) {
			fServer=true;
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"randomfill", 10)) {
			fRandFill=true;
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"firstfill", 9)) {
			fFirstFill=true;
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"randomsize", 10)) {
			fRandomSize=true;
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"hquadrants=", 11)) {
			hquads=&argv[iCnt][11];
			if (NULL != vquads) {
				wprintf(L"## Error - hquadrants and vquadrants are mutually exclusive.\n");
				fArgsOK=false;
			}
			continue;
		}
		if (!_wcsnicmp(argv[iCnt], L"vquadrants=", 11)) {
			vquads=&argv[iCnt][11];
			if (NULL != hquads) {
				wprintf(L"## Error - hquadrants and vquadrants are mutually exclusive.\n");
				fArgsOK=false;
			}
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"randomquads", 11)) {
			fRandomQuads=true;
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"returncount", 11)) {
			fReturnCount=true;
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"returnfailed", 12)) {
			fReturnFailed=true;
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"needoffs", 8)) {
			bNeedOffs=true;
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"needvpag", 8)) {
			bNeedVpag=true;
			continue;
		}

		if (!_wcsnicmp(argv[iCnt], L"monitors", 8)) {
			wprintf(L"Sizing to monitors\n");
			fMonitorsMode=true;
			continue;
		}

		fArgsOK=false;
		wprintf(L"Unknown argument: %s\n", argv[iCnt]);
	}

	if ((fMosaic)&&(!fYesIMeanIt)) {
		// we need to verify some things here...
		if (maxmosaicx <= minmosaicx) {
			wprintf(L"## Error - maxmosaicx (%d) must be more than minmosaicx (%d)\n", maxmosaicx, minmosaicx);
			wprintf(L"## Add argument 'YesIMeanIt' to override, but behaviour is not\n");
			wprintf(L"## guaranteed! :)\n");
			fArgsOK=false;
		}
		if (maxmosaicy <= minmosaicy) {
			wprintf(L"## Error - maxmosaicy (%d) must be more than minmosaicy (%d)\n", maxmosaicy, minmosaicy);
			wprintf(L"## Add argument 'YesIMeanIt' to override, but behaviour is not\n");
			wprintf(L"## guaranteed! :)\n");
			fArgsOK=false;
		}
		if (!fArgsOK) {
#ifdef _DEBUG
			while (!_kbhit());
#endif
			check(exitcode);
		}
	}

	if (!fArgsOK) {
		wprintf(L"Use: NewPic [arg=value, arg=value, etc]\n");
		wprintf(L"Chooses a random picture, resizes it with aspect,\n");
		wprintf(L"and writes it as a BMP to the output filename. Input supports\n");
		wprintf(L"BMP, GIF, JPG, PNG, PCX, TIF, PSD and TGA. Sends Windows a 'backdrop\n");
		wprintf(L"changed' message so that you can use it to change a Windows backdrop. ;)\n");
		wprintf(L"If no parameters are specified, will size to primary monitor size, in the\n");
		wprintf(L"NewPic folder, save it as backdrop.bmp, and set the windows backdrop.\n");
		wprintf(L"Unicode filenames now supported.\n");
		wprintf(L"path=[path to pictures] (may be repeated for more paths)\n");
		wprintf(L"map=[path to mosaic map [file|x|y|w|h|origw|origh|offx|offy]]\n");
		wprintf(L"outwidth=[output width]\n");
		wprintf(L"outheight=[output height]\n");
		wprintf(L"outfile=[output filename]\n");
		wprintf(L"fillbuf=[output filename - debugging use]\n");
		wprintf(L"maxscale=[maximum permitted scale to fit - smaller images are discarded]\n");
		wprintf(L"minscale=[minimum permitted scale to fit - larger images are discarded]\n");
		wprintf(L"minwidth=[minimum input width]\n");
		wprintf(L"minheight=[minimum input height]\n");
		wprintf(L"minmosaicx=[minimum space to fit mosaic width (default 160)]\n");
		wprintf(L"minmosaicy=[minimum space to fit mosaic height (default 120)]\n");
		wprintf(L"maxmosaicx=[maximum width of one mosaic image]\n");
		wprintf(L"maxmosaicy=[minimum height of one mosaic image]\n");
		wprintf(L"maxerr=[maximum errors per file attempt (default 6)]\n");
		wprintf(L"hwnd=[window handle to render to, in decimal]\n");
		wprintf(L"filedelay=[time in milliseconds between each file in a mosaic (max 10000)]\n");
		wprintf(L"hquadrants=[string to define quadrants horizontally]\n");
		wprintf(L"vquadrants=[string to define quadrants vertically]\n");
		wprintf(L"randomquads [process quadrants in a random order]\n");
		wprintf(L"needvpag [require finding the ImageMagick vpAg chunk for a map]\n");
		wprintf(L"needoffs [require finding the ImageMagick oFFs chunk for a map]\n");
		wprintf(L"background [sets windows background if present]\n");
		wprintf(L"color [rejects pictures that are not color]\n");
		wprintf(L"filename [overlay filename]\n");
		wprintf(L"alwaysfilename [overlay filename, even on very small images]\n");
		wprintf(L"mosaic [attempts to fill output with multiple images]\n");
		wprintf(L"skipblank [skips images that are a single solid color]\n");
		wprintf(L"noscale [don't scale - large images will be skipped and overrides minmosaic]\n");
		wprintf(L"stretch [stretch final image to fit, ignoring aspect and noscale]\n");
		wprintf(L"sequential [mosaic is built of sequential images rather than all random]\n");
		wprintf(L"biggestfirst [biggest images are added first - short lists only]\n");
		wprintf(L"smallestfirst [smallest images are added first - short lists only]\n");
		wprintf(L"bigxfirst [In mosaic mode, add the widest images (x) first]\n");
		wprintf(L"bigyfirst [In mosaic mode, add the tallest images (y) first]\n");
		wprintf(L"smallxfirst [In mosaic mode, add the narrowest images (x) first]\n");
		wprintf(L"smallyfirst [In mosaic mode, add the shortest images (y) first]\n");
		wprintf(L"firstfile [will start with first file in list (ascii sort)]\n");
		wprintf(L"nodupes [will avoid placing duplicate images (slow)]\n");
		wprintf(L"checkflip [will avoid placing flipped images (slow)]\n");
		wprintf(L"randomfill [fill mosaic randomly rather than largest blocks first]\n");
		wprintf(L"firstfill [fill mosaic with first found block rather than largest]\n");
		wprintf(L"randomsize [fill mosaic with random sizes rather than best fit]\n");
		wprintf(L"force43 [compensates for non-square pixel modes on primary monitor]\n");
		wprintf(L"bgfirst [set the background color from the first pixel in the first pic]\n");
		wprintf(L"pinkalpha [set transparent parts of PNG to RGB #FF00FF]\n");
		wprintf(L"stoponerr [stop if an error occurred - note that filling a mosaic is an error]\n");
		wprintf(L"returncount [errorlevel return is number of mosaic files processed]\n");
		wprintf(L"returnfailed [errorlevel return is number of mosaic files failed or skipped]\n");
		wprintf(L"server [process multiple files with sync events to external app]\n");
		wprintf(L"monitors [size image to monitors and align with monitor offsets]\n");
		wprintf(L"\n\nBy Mike Brent (Tursi). http://harmlesslion.com v119 - 25 January 2020\n");
#ifdef _DEBUG
			while (!_kbhit());
#endif
		check(exitcode);
	}

	// get list of files
	iCnt=0;
	nSkip=0;
	
	// Enable this line for a list of skipped files in the root
	//	fpSkip=_wfopen(L"D:\\skiplist.txt", L"w");

    // BuildFileList will append all found files, as its meant to
    // be called recursively anyway. So we can just call it in a loop
    // in order to support multiple source folders
    // note that afterwards, szFolder will contain the last listed one!
    {
        bool pathSpecified = false;
        for (int idx=1; idx<argc; idx++) {
		    if (!_wcsnicmp(argv[idx], L"path=", 5)) {
                pathSpecified = true;
			    wcscpy(szFolder, &argv[idx][5]);
                if (wcslen(szFolder) > 0) {
                    wprintf(L"Scanning %s...\n", szFolder);
            	    BuildFileList(szFolder);
                }
			    continue;
		    }
        }
        if (!pathSpecified) {
            // legacy behaviour, call with the default path
            if (wcslen(szFolder) > 0) {
                wprintf(L"Scanning %s...\n", szFolder);
                BuildFileList(szFolder);
            }
        }
    }

	if (NULL != fpSkip) {
		fclose(fpSkip);	
	}

	wprintf(L"\n%d files found. (%d skipped)\n", iCnt, nSkip);

	if ((fBiggestFirst)||(fSmallestFirst)||(fBiggestXFirst)||(fSmallestXFirst)||(fBiggestYFirst)||(fSmallestYFirst)) {
		if ((iCnt>50) && (!fYesIMeanIt)) {
			wprintf(L"## More than 50 files is excessive for biggest/smallest first!\n");
			wprintf(L"## Every file needs to be opened and examined - this can take\n");
			wprintf(L"## a very long time! Add argument 'YesIMeanIt' if you really\n");
			wprintf(L"## want to do this.\n");
			check(-1);
		}
	}

	if (fStartFirst) {
		SortListByName(iCnt);
	}

	if (fBiggestFirst) {
		SortListByBiggest(iCnt);
	}
	if (fSmallestFirst) {
		SortListBySmallest(iCnt);
	}
	if (fBiggestXFirst) {
		SortListByBiggestX(iCnt);
	}
	if (fSmallestXFirst) {
		SortListBySmallestX(iCnt);
	}
	if (fBiggestYFirst) {
		SortListByBiggestY(iCnt);
	}
	if (fSmallestYFirst) {
		SortListBySmallestY(iCnt);
	}

	if (fServer) {
		wprintf(L"Creating event handle NEWPIC_EVENT_TRIGGER\n");
		hServerEvent=CreateEvent(NULL, FALSE, TRUE, L"NEWPIC_EVENT_TRIGGER");
		wprintf(L"Creating event handle NEWPIC_EVENT_COMPLETE\n");
		hCompleteEvent=CreateEvent(NULL, FALSE, TRUE, L"NEWPIC_EVENT_COMPLETE");
		wprintf(L"Creating event handle NEWPIC_EVENT_TERMINATE\n");
		hTerminateEvent=CreateEvent(NULL, TRUE, FALSE, L"NEWPIC_EVENT_TERMINATE");

		// and just make sure of their state in case we inherited them
		ResetEvent(hServerEvent);
		ResetEvent(hCompleteEvent);
		ResetEvent(hTerminateEvent);
	}
	
	if (0==iCnt) {
		wprintf(L"That's an error.\n");
		if  (exitcode==0) exitcode=10;
#ifdef _DEBUG
 			while (!_kbhit());
#endif
		check(exitcode);
	}

	do {		// for fServer

		if (fServer) {
			HANDLE hObj[2];
			hObj[0]=hServerEvent;
			hObj[1]=hTerminateEvent;
			if (WAIT_OBJECT_0 != WaitForMultipleObjects(2, hObj, FALSE, INFINITE)) {
				break;	// exit the loop
			}
			fFirstFile=true;
		}

	// Used to fit the image into the remaining space
	currentx=0;
	currenty=0;
	currentw=iWidth;
	currenth=iHeight;
	hBuffer2=NULL;
	n=-1;		// set the last image count

	// call the correct processing group
	if (fMonitorsMode) {
		ProcessMonitors(); 
	} else if (NULL != hquads) {
		ProcessHQuads();
	} else if (NULL != vquads) {
		ProcessVQuads();
	} else {
		DoOneRun();
	}

	if (!fServer) {
		hDest=IS40_OpenFileDest(szOutfile, FALSE);

		if (NULL == hDest) {
			wprintf(L"Can't open output file %s - fatal error.\n", szOutfile);
			if (exitcode==0) exitcode=20;
#ifdef _DEBUG
			while (!_kbhit());
#endif
            if (NULL != hBuffer2) {
    	        free(hBuffer2);
                hBuffer2=NULL;
            }
			check(exitcode);
		}

		if (0==IS40_WriteBMP(hDest, (BYTE*)hBuffer2, iWidth, iHeight, 24, iWidth*3, 0, NULL, NULL, 0)) {
			wprintf(L"Write to file failed - fatal error.\n");
			if (exitcode==0) exitcode=30;
#ifdef _DEBUG
			while (!_kbhit());
#endif
			check(exitcode);
		}

		IS40_CloseSource(hDest);
	}

    if (NULL != hBuffer2) {
    	free(hBuffer2);
        hBuffer2=NULL;
    }

	if ('\0' != szFillBuffName[0]) {
		// write fill buffer to a file too
		hDest=IS40_OpenFileDest(szFillBuffName, FALSE);

		if (NULL != hDest) {
			RGBQUAD pal[256];
			memset(pal, 255, sizeof(pal));
			memset(pal, 0, sizeof(RGBQUAD));
			IS40_WriteBMP(hDest, (BYTE*)FillBuffer, iWidth, iHeight, 8, iWidth, 256, pal, NULL, 0);
			IS40_CloseSource(hDest);
		} else {
			wprintf(L"Failed to write fill buffer.\n");
		}
	}
	
	wprintf(L"%d total pictures read, %d dupes, %d blank, %d flipped, %d files failed/discarded\n", totalpics, nDupeFiles, nBlankFiles, nFlippedFiles, nFailedFiles);

	// Set background
	if (Background) {
		wcscpy(szBuf,szOutfile);
		SystemParametersInfo(SPI_SETDESKWALLPAPER,0,szBuf,SPIF_UPDATEINIFILE);
	}

	// Tell Active Desktop to refresh
	HRESULT hr;
	IActiveDesktop *pActiveDesktop;
	COMPONENT Comp;
	int idx, nCount;

	pActiveDesktop=NULL;
	CoInitialize(NULL);
	hr = CoCreateInstance(CLSID_ActiveDesktop, NULL, CLSCTX_INPROC_SERVER, IID_IActiveDesktop, (void**)&pActiveDesktop);
	if (NULL != pActiveDesktop) {
		wprintf(L"Active desktop refresh...\n");
		// try and find an active desktop item with the output file's name
		nCount=0;
		pActiveDesktop->GetDesktopItemCount(&nCount, 0);
		wprintf(L"%d items...\n", nCount);
		Comp.dwSize=sizeof(Comp);
		for (idx=0; idx<nCount; idx++) {
			if (S_OK==(hr=pActiveDesktop->GetDesktopItem((unsigned long)idx, &Comp, 0))) {
				if ((Comp.iComponentType==COMP_TYPE_PICTURE) && (instr(Comp.wszSource,szOutfile))) {
					wprintf(L"Changed one...\n");
					Comp.fDirty=true;
					pActiveDesktop->ModifyDesktopItem(&Comp, COMP_ELEM_DIRTY);
				}
			}
		}
		pActiveDesktop->ApplyChanges(AD_APPLY_ALL);
		pActiveDesktop->Release();
	}

	if (NULL != FillBuffer) {
		free(FillBuffer);
		FillBuffer=NULL;
	}
#ifdef _DEBUG
	if (!fServer) {
		while (!_kbhit());
	}
#endif

		if (fServer) {
			SetEvent(hCompleteEvent);
		}
	} while (fServer);

	check(exitcode);	// never returns
}

void DoOneRun() {

	if (!fMosaic) {
		minmosaicx=0;
		minmosaicy=0;
	}

	// Loop for mosaic
	do {
		wchar_t szFileName[256];
		int cntdown;

		errCount=0;
		ScaleMode=-1;

		// randomly choose one
	tryagain:
		errCount++;
		if (errCount>maxerrorcount) {
			wprintf(L"Too many errors - declaring completion.\n");
			fMosaic=false;
			goto escapeloop;
		}

		if (fServer) {
			if (WAIT_OBJECT_0 == WaitForSingleObject(hTerminateEvent, 0)) {
				// we're done, abort immediately
				exit(-1);
			}
		}

		if ((n == -1) && (fCoolSpacing)) {
			// find out-41.png as the first sprite
			int z;
			for (z=0; z<iCnt; z++) {
				if (wcsstr(szFiles[z], L"out-41")) {
					break;
				}
			}
			if (z<iCnt) {
				n=z;
				wprintf(L"Cool Spacing enabled - starting with out-41\n");
			} else {
				wprintf(L"* Could not find out-41 -- disabling Cool Spacing.\n");
				fCoolSpacing=false;
				n=0;
			}
		} else {
			if ((n==-1)||(!fSequential)) {
				if ((n == -1)&&(fStartFirst)) {
					n=0;
				} else {
					n=((rand()<<16)|rand())%iCnt;
				}
			} else if ((fCoolSpacing) && (fSequential)) {
				// clear cool spacing and reset n to the top
				wprintf(L"Cool Spacing complete, restarting sequential search\n");
				n=0;
				fCoolSpacing=false;
			} else {
				n++;
			}
		}

recount:
		cntdown=iCnt;
		while ((szFiles[n][0]=='\0') || (szFiles[n][0] & 0x80)) {
			n++;
			if (n>=iCnt) n=0;
			cntdown--;
			if (cntdown == 0) {
				if (fServer) {
					// we aren't allowed to run out of files - reset the list
					wprintf(L"List exhausted - restarting\n");
					for (int i=0; i<iCnt; i++) {
						szFiles[n][i]&=0x7f;
					}
					goto recount;
				}
				if (fMosaic) {
					wprintf(L"No more images to choose from! Going with what we have.\n");
					fMosaic=false;
					goto escapeloop;
				}
				wprintf(L"Ran out of images in the list, calling it done.\n");
				goto escapeloop;
			}
		}

		wprintf(L"Chose #%d: %s\n", n, &szFiles[n][0]);
		wcscpy(szFileName, szFiles[n]);
		szFiles[n][0]|=0x80;				// flag used (this is probably still okay - drive letters won't be unicode yet)

		// open the file
		hSource=IS40_OpenFileSource(szFileName);
		if (NULL==hSource) {
			wprintf(L"Can't open image file.\n");
			nFailedFiles++;
			goto tryagain;
		}

		// guess filetype
		ret=IS40_GuessFileType(hSource);
		IS40_Seek(hSource, 0, 0);

		switch (ret)
		{
		case 1: //bmp
			hBuffer=IS40_ReadBMP(hSource, &inWidth, &inHeight, 24, NULL, 0);
			break;

		case 2: //gif
	//		hBuffer=ISReadGIFToRGB(hSource, &inWidth, &inHeight, &dummy);
			// Although the GIF code is now enabled in ImgSrc 4.0, we'll just keep
			// using my old routine which works well enough.
			IS40_CloseSource(hSource);
			hSource=NULL;
			hBuffer=load_gif(szFileName, &inWidth, &inHeight);
			break;

		case 3: //jpg
			hBuffer=IS40_ReadJPG(hSource, &inWidth, &inHeight, 24, 0);
			break;

		case 4: //png
			if (fPinkAlpha) {
				// read 32-bit and then convert to 24
				hBuffer=IS40_ReadPNG(hSource, &inWidth, &inHeight, 32, NULL, 0);
				BYTE *psrc=(BYTE*)hBuffer, *pdst=(BYTE*)hBuffer;
				int r,g,b,a;
				// we overwrite the image in place - the buffer will end up too big,
				// but it's temporary anyway
				for (unsigned int idx=0; idx<inHeight*inWidth; idx++) {
					r=*(psrc++);
					g=*(psrc++);
					b=*(psrc++);
					a=*(psrc++);
					if (a<128) {
						r=255;
						g=0;
						b=255;
					}
					*(pdst++)=r;
					*(pdst++)=g;
					*(pdst++)=b;
				}
			} else {
				// one quick read to 24 bit
				hBuffer=IS40_ReadPNG(hSource, &inWidth, &inHeight, 24, NULL, 0);
			}
			break;

		case 5: //pcx
			hBuffer=IS40_ReadPCX(hSource, &inWidth, &inHeight, 24, NULL, 0);
			break;

		case 6: //tif
			hBuffer=IS40_ReadTIFF(hSource, &inWidth, &inHeight, 24, NULL, 0, 0);
			break;

		case 10: // psd
			hBuffer=IS40_ReadPSD(hSource, &inWidth, &inHeight, 24, NULL, 0);
			break;

		case 14: // tga
			hBuffer=IS40_ReadTGA(hSource, &inWidth, &inHeight, 24, NULL, 0);
			break;

		default:
			wprintf(L"Unable to indentify file (corrupt?)\n-> %s <-\n", szFileName);
			IS40_CloseSource(hSource);
			nFailedFiles++;
			goto tryagain;
		}

		// make sure these are cleared
		bGotVpag=false;
		bGotOffs=false;

		if (NULL != hSource) {
			IS40_CloseSource(hSource);
			if (ret == 4) {
				// image was a PNG - reoopen it and try to read the ImageMagick chunks
				ParseImageMagickChunks(szFileName);
			}
		}

		if ((iMinWidth > 0) && (inWidth < iMinWidth)) {
			wprintf(L"Image is only %d x %d - skipping.\n", inWidth, inHeight);
			GlobalFree(hBuffer);
			nFailedFiles++;
			goto tryagain;
		}
		if ((iMinHeight > 0) && (inHeight < iMinHeight)) {
			wprintf(L"Image is only %d x %d - skipping.\n", inWidth, inHeight);
			GlobalFree(hBuffer);
			nFailedFiles++;
			goto tryagain;
		}

		if (NULL == hBuffer) {
			wprintf(L"Failed reading image file. (%d)\n", IS40_GetLastError());
			nFailedFiles++;
			goto tryagain;
		}

		if (COLORCHECK) {
			wprintf(L"Testing color... ");
			// check for color
			pTmp=(BYTE*)hBuffer;
			for (idx1=0; idx1<inHeight; idx1++) {
				for (idx2=0; idx2<inWidth; idx2++) {
					//wprintf(L"%d-%d   ", *(pTmp), *(pTmp+1));
					if (((*(pTmp) - *(pTmp+1)) > 25 ) || ((*(pTmp) - *(pTmp+1)) < -25)) {
						wprintf(L"OK\n");
						goto doscale;					// just checking red and green
					}
					pTmp+=3;
				}
			}
			wprintf(L"Not a color image! (Probably!)\n");
			GlobalFree(hBuffer);
			nFailedFiles++;
			goto tryagain;
		}

		if (fSkipBlank) {
			wprintf(L"Testing for blank image... ");
			// check for color
			pTmp=(BYTE*)hBuffer;
			for (idx1=0; idx1<inHeight; idx1++) {
				for (idx2=0; idx2<inWidth; idx2++) {
					if ((*pTmp != *(char*)hBuffer) ||
						(*(pTmp+1) != *(((char*)hBuffer)+1)) ||
						(*(pTmp+2) != *(((char*)hBuffer)+2))) {
						wprintf(L"OK\n");
						goto doscale;					// just checking red and green
					}
					pTmp+=3;
				}
			}
			wprintf(L"Looks blank\n");
			GlobalFree(hBuffer);
			nBlankFiles++;
			goto tryagain;
		}

		// scale the image
	doscale:
		
		if ((fNoDupes)&&(NULL != FillBuffer)&&(NULL != hBuffer2)) {
			BYTE *pCur=(BYTE*)hBuffer;	// current picture
			BYTE *pOut=(BYTE*)hBuffer2;	// output buffer
			bool flag;
			// scan the image for duplicates of this one - slow operation
			// if we find a duplicate, update the map as if we did draw it
			for (unsigned int x=0; x<iWidth; x++) {
				for (unsigned int y=0; y<iHeight; y++) {
					if (0 == FillBuffer[y*iHeight+x]) {
						// nothing here
						continue;
					}
					flag=true;
					for (unsigned int x2=0; (x2<inWidth)&&(flag); x2++) {
						for (unsigned int y2=0; (y2<inHeight)&&(flag); y2++) {
							if (memcmp(pCur+y2*inWidth*3+x2*3, pOut+(y2+y)*iWidth*3+(x2+x)*3, 3)) {
								flag=false;
							}
						}
					}
					if (!flag) {
						continue;
					}
					// it's a match!
					wprintf(L"Found duplicate image at %d,%d (%dx%d), not re-drawing.\n",x,y,inWidth,inHeight);
					nDupeFiles++;
					totalpics++;

					// before writing the map, see if we got mandatory offset information
					if ((bNeedVpag)&&(!bGotVpag)) {
						wprintf(L"Error: Did not get mandatory vpAg chunk\n");
						nFailedFiles++;
					}
					if ((bNeedOffs)&&(!bGotOffs)) {
						wprintf(L"Error: Did not get mandatory oFFs chunk\n");
						nFailedFiles++;
					}

					if ('\0' != szMap[0]) {
						// write to the map file, we do it kind of sloppy here but it'll work
						FILE *fp;
						fp=_wfopen(szMap, L"a");
						if (NULL == fp) {
							wprintf(L"Failed to create map file!\n");
							wcscpy(szMap, L"");
						} else {
							fwprintf(fp, L"%s|%d|%d|%d|%d|%d|%d|%d|%d\n", szFileName, x, y, inWidth, inHeight, mapOrigW, mapOrigH, mapOffW, mapOffH);
							fclose(fp);
						}
					}

					goto skipdrawing;
				}
			}
		}

		if ((fCheckFlip)&&(NULL != FillBuffer)&&(NULL != hBuffer2)) {
			BYTE *pCur=(BYTE*)hBuffer;	// current picture
			BYTE *pOut=(BYTE*)hBuffer2;	// output buffer
			bool flag;
			// scan the image for duplicates of this one, but flipped - slow operation
			// if we find a duplicate, update the map as if we did draw it
			// this is pretty hacky - we scan three times - once for each type of flip
			// we don't check for no flip, nodupes does that.
			// h flip only
			for (unsigned int x=0; x<iWidth; x++) {
				for (unsigned int y=0; y<iHeight; y++) {
					if (0 == FillBuffer[y*iHeight+x]) {
						// nothing here
						continue;
					}
					flag=true;
					for (unsigned int x2=0; (x2<inWidth)&&(flag); x2++) {
						for (unsigned int y2=0; (y2<inHeight)&&(flag); y2++) {
							// compare 1 pixel
							if (memcmp(pCur+y2*inWidth*3+(inWidth-x2-1)*3, pOut+(y2+y)*iWidth*3+(x2+x)*3, 3)) {
								flag=false;
							}
						}
					}
					if (!flag) {
						continue;
					}
					// it's a match!
					wprintf(L"Found h-flipped image at %d,%d (%dx%d), not re-drawing.\n",x,y,inWidth,inHeight);
					nFlippedFiles++;
					totalpics++;

					// before writing the map, see if we got mandatory offset information
					if ((bNeedVpag)&&(!bGotVpag)) {
						wprintf(L"Error: Did not get mandatory vpAg chunk\n");
						nFailedFiles++;
					}
					if ((bNeedOffs)&&(!bGotOffs)) {
						wprintf(L"Error: Did not get mandatory oFFs chunk\n");
						nFailedFiles++;
					}

					if ('\0' != szMap[0]) {
						// write to the map file, we do it kind of sloppy here but it'll work
						FILE *fp;
						fp=_wfopen(szMap, L"a");
						if (NULL == fp) {
							wprintf(L"Failed to create map file!\n");
							wcscpy(szMap, L"");
						} else {
							fwprintf(fp, L"%s|%d|%d|%d|%d|%d|%d|%d|%d\n", szFileName, x, y, -(signed)inWidth, inHeight, mapOrigW, mapOrigH, mapOffW, mapOffH);
							fclose(fp);
						}
					}

					goto skipdrawing;
				}
			}

			// v flip only
			for (unsigned int x=0; x<iWidth; x++) {
				for (unsigned int y=0; y<iHeight; y++) {
					if (0 == FillBuffer[y*iHeight+x]) {
						// nothing here
						continue;
					}
					flag=true;
					for (unsigned int x2=0; (x2<inWidth)&&(flag); x2++) {
						for (unsigned int y2=0; (y2<inHeight)&&(flag); y2++) {
							// compare 1 pixel
							if (memcmp(pCur+(inHeight-y2-1)*inWidth*3+x2*3, pOut+(y2+y)*iWidth*3+(x2+x)*3, 3)) {
								flag=false;
							}
						}
					}
					if (!flag) {
						continue;
					}
					// it's a match!
					wprintf(L"Found v-flipped image at %d,%d (%dx%d), not re-drawing.\n",x,y,inWidth,inHeight);
					nFlippedFiles++;
					totalpics++;

					// before writing the map, see if we got mandatory offset information
					if ((bNeedVpag)&&(!bGotVpag)) {
						wprintf(L"Error: Did not get mandatory vpAg chunk\n");
						nFailedFiles++;
					}
					if ((bNeedOffs)&&(!bGotOffs)) {
						wprintf(L"Error: Did not get mandatory oFFs chunk\n");
						nFailedFiles++;
					}

					if ('\0' != szMap[0]) {
						// write to the map file, we do it kind of sloppy here but it'll work
						FILE *fp;
						fp=_wfopen(szMap, L"a");
						if (NULL == fp) {
							wprintf(L"Failed to create map file!\n");
							wcscpy(szMap, L"");
						} else {
							fwprintf(fp, L"%s|%d|%d|%d|%d|%d|%d|%d|%d\n", szFileName, x, y, inWidth, -(signed)inHeight, mapOrigW, mapOrigH, mapOffW, mapOffH);
							fclose(fp);
						}
					}

					goto skipdrawing;
				}
			}

			// h and v flip
			for (unsigned int x=0; x<iWidth; x++) {
				for (unsigned int y=0; y<iHeight; y++) {
					if (0 == FillBuffer[y*iHeight+x]) {
						// nothing here
						continue;
					}
					flag=true;
					for (unsigned int x2=0; (x2<inWidth)&&(flag); x2++) {
						for (unsigned int y2=0; (y2<inHeight)&&(flag); y2++) {
							// compare 1 pixel
							if (memcmp(pCur+(inHeight-y2-1)*inWidth*3+(inWidth-x2-1)*3, pOut+(y2+y)*iWidth*3+(x2+x)*3, 3)) {
								flag=false;
							}
						}
					}
					if (!flag) {
						continue;
					}
					// it's a match!
					wprintf(L"Found h&v-flipped image at %d,%d (%dx%d), not re-drawing.\n",x,y,inWidth,inHeight);
					nFlippedFiles++;
					totalpics++;

					// before writing the map, see if we got mandatory offset information
					if ((bNeedVpag)&&(!bGotVpag)) {
						wprintf(L"Error: Did not get mandatory vpAg chunk\n");
						nFailedFiles++;
					}
					if ((bNeedOffs)&&(!bGotOffs)) {
						wprintf(L"Error: Did not get mandatory oFFs chunk\n");
						nFailedFiles++;
					}

					if ('\0' != szMap[0]) {
						// write to the map file, we do it kind of sloppy here but it'll work
						FILE *fp;
						fp=_wfopen(szMap, L"a");
						if (NULL == fp) {
							wprintf(L"Failed to create map file!\n");
							wcscpy(szMap, L"");
						} else {
							fwprintf(fp, L"%s|%d|%d|%d|%d|%d|%d|%d|%d\n", szFileName, x, y, -(signed)inWidth, -(signed)inHeight, mapOrigW, mapOrigH, mapOffW, mapOffH);
							fclose(fp);
						}
					}

					goto skipdrawing;
				}
			}
		}

		// Figure out where it's going to go (updates the 4 'current' vars)
		if (-1 == maxscale) {
			// noscale mode, mosaic set by size of image
			minmosaicx=inWidth;
			minmosaicy=inHeight;
		}
		if (0 == EnumerateRectangles(minmosaicx, minmosaicy)) {
			// there's no room left!
			break;
		}

		// increment the pic count
		totalpics++;

		// check if the available space exceeds the max mosaic settings (only if set), and resize if so
		if (fMosaic) {
			if (currentw > maxmosaicx) {
				currentw=maxmosaicx;
			}
			if (currenth > maxmosaicy) {
				currenth=maxmosaicy;
			}
		}

		if (fRandomSize) {
			// randomly size the block to a minimum of minmosaicx or y
			if (currentw>minmosaicx) {
				currentw=rand()%(currentw-minmosaicx)+minmosaicx;
			}
			if (currenth>minmosaicy) {
				currenth=rand()%(currenth-minmosaicy)+minmosaicy;
			}
		}

		// cache these values from ScalePic for the filename code
		int origx, origy, origw, origh;
		// no, not very clean code ;) And I started out so well ;)
		origx=currentx;
		origy=currenty;
		origw=currentw;
		origh=currenth;
		ScaleMode=-1;

		if (!ScalePic(szFileName)) {			// from hBuffer to hBuffer2
			// failed due to scale 
			nFailedFiles++;
			goto tryagain;
		}

		// We do an outline by drawing 8 copies of the string around the desired point
		// Just 1 pixel wide for now
		if (AddFilename) {
			wchar_t myStr[_MAX_PATH];
			int lastw=origw-currentw;
			int lasth=origh-currenth;
			int x=lastw/2+origx;
			int y=lasth-5+origy;

			if ((lastw > 319) || (AddFilename > 1)) {
				wprintf(L"Adding filename caption...\n");

				wcscpy(myStr, szFileName);
				MyDrawText(hBuffer2, iWidth, iHeight, myStr, x-1, y-1);
			}
		}

		if (nPerFileDelay > 0) {
			if (nPerFileDelay > 10000) nPerFileDelay=10000;
			Sleep(nPerFileDelay);
		}

skipdrawing:
		GlobalFree(hBuffer);

escapeloop: ;
	} while (fMosaic);

}

void BuildFileList(wchar_t *szFolder)
{
	HANDLE hIndex;
	WIN32_FIND_DATA dat;
	wchar_t buffer[256]; 

	wcscpy(buffer, szFolder);
	wcscat(buffer, L"\\*");
	hIndex=FindFirstFile(buffer, &dat);

	while (INVALID_HANDLE_VALUE != hIndex) {

		rand();		// random number mixing

		if (iCnt>MAXFILES-1) {
			FindClose(hIndex);
			return;
		}
		
		wcscpy(buffer, szFolder);
		wcscat(buffer, L"\\");
		wcscat(buffer, dat.cFileName);

		if (dat.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			if (dat.cFileName[0]=='.') goto next;
			BuildFileList(buffer);
			goto next;
		}

		// BMP, GIF, JPG, PNG, PCX, TIF
		// Check last three characters
		if ((0==_wcsicmp(&buffer[wcslen(buffer)-3], L"bmp")) ||
			(0==_wcsicmp(&buffer[wcslen(buffer)-3], L"gif")) ||
			(0==_wcsicmp(&buffer[wcslen(buffer)-3], L"jpg")) ||
			//(0==_wcsicmp(&buffer[wcslen(buffer)-3], L"jpc")) ||
			(0==_wcsicmp(&buffer[wcslen(buffer)-4], L"jpeg")) ||
			(0==_wcsicmp(&buffer[wcslen(buffer)-4], L"jfif")) ||
			(0==_wcsicmp(&buffer[wcslen(buffer)-3], L"png")) ||
			(0==_wcsicmp(&buffer[wcslen(buffer)-3], L"pcx")) ||
			(0==_wcsicmp(&buffer[wcslen(buffer)-3], L"psd")) ||
			(0==_wcsicmp(&buffer[wcslen(buffer)-3], L"tga")) ||
			(0==_wcsicmp(&buffer[wcslen(buffer)-3], L"tif"))) {
				wcscpy(&szFiles[iCnt++][0], buffer);
		} else {
			if (NULL != fpSkip) {
				fwprintf(fpSkip, L"%s\n", buffer);
			}
			nSkip++;
		}

next:
		if (false == FindNextFile(hIndex, &dat)) {
			int ret;
			if ((ret=GetLastError()) != ERROR_NO_MORE_FILES) {
				OutputDebugString(L"Error in findnextfile\n");
			}
			FindClose(hIndex);
			hIndex=INVALID_HANDLE_VALUE;
		}
	}
}

// Scales from hBuffer to hBuffer2
// Filename is only for printing the map file
bool ScalePic(wchar_t *szFile)
{
#define X_AXIS 1
#define Y_AXIS 2
	
	double x1,y1,x_scale,y_scale;
	unsigned int thisx, thisy;
	HGLOBAL tmpBuffer;
	bool fLast=false;
	double pixelratio;
	
	wprintf(L"Image:  %d x %d\n",inWidth, inHeight);
	wprintf(L"Output: %d x %d\n",currentw, currenth);

	if (maxscale == -1) {
		// if in noscale mode, then we don't scale, but we do fail if
		// the image is too large to fit
		if ((currentw < (signed)inWidth) || (currenth < (signed)inHeight)) {
			wprintf(L"Rejecting - image too large for noscale.\n");
			return false;
		}
	}
	
	x1=(double)(inWidth);
	y1=(double)(inHeight);
	
	if (maxscale == -1) {
		x_scale=1.0;
		y_scale=1.0;
	} else {
		x_scale=((double)(currentw))/x1;
		y_scale=((double)(currenth))/y1;
	}

	wprintf(L"Scale:  %f x %f\n",x_scale,y_scale);
	
	if (ScaleMode == -1) {
		ScaleMode=Y_AXIS;
	
		if (x1*y_scale > (double)(currentw)) ScaleMode=X_AXIS;
		if (y1*x_scale > (double)(currenth)) ScaleMode=Y_AXIS;
	
		wprintf(L"Decided scale (1=X, 2=Y): %d\n",ScaleMode);
	} else {
		wprintf(L"Using scale (1=X, 2=Y): %d\n",ScaleMode);
	}

	if (ScaleMode==X_AXIS) {
		y_scale=x_scale;
	} else {
		x_scale=y_scale;
	}

	if (fForce43) {
		// this will not always result in exactly correct output sizes
		// this can override noscale, too ;)
		if (ScaleMode==X_AXIS) {
			// calculate adjustment for Y axis
			int w=GetSystemMetrics(SM_CXSCREEN);
			int h=GetSystemMetrics(SM_CYSCREEN);
			if (w>0) {
				double h43=(double)w/(4.0/3.0);
				pixelratio=(double)h/h43;	// maybe the other way?
				y_scale*=pixelratio;
				wprintf(L"Y scale ratio - %f - new scale %f\n", pixelratio, y_scale);
			}
		} else {
			// Y_AXIS scalemode
			// calculate adjustment for Y axis - usually smaller
			int w=GetSystemMetrics(SM_CXSCREEN);
			int h=GetSystemMetrics(SM_CYSCREEN);
			if (w>0) {
				double w43=(double)h*(4.0/3.0);
				pixelratio=(double)w/w43;
				x_scale*=pixelratio;
				wprintf(L"X scale ratio - %f - new scale %f\n", pixelratio, x_scale);
			}
		}
	}

	if (ScaleMode==Y_AXIS) {
		if ((maxscale != -1) && (y_scale > (double)maxscale)) {
			wprintf(L"Rejecting - scale larger than maxscale (%d).\n", maxscale);
			return false;
		}
		if ((minscale > 0) && (y_scale < (double)minscale)) {
			wprintf(L"Rejecting - scale smaller than minscale (%d).\n", minscale);
			return false;
		}

		x1*=x_scale;
		y1*=y_scale;
		if ((signed)(currentw-(int)x1) < minmosaicx) {
			fLast=true;
		}
	} else {
		if ((maxscale != -1) && (x_scale > (double)maxscale)) {
			wprintf(L"Rejecting - scale larger than maxscale (%d).\n", maxscale);
			return false;
		}
		if ((minscale > 0) && (x_scale < (double)minscale)) {
			wprintf(L"Rejecting - scale smaller than minscale (%d).\n", minscale);
			return false;
		}

		x1*=x_scale;
		y1*=y_scale;
		if ((signed)(currenth-(int)y1) < minmosaicy) {
			fLast=true;
		}
	}
	
	if ((x_scale == 1.0) && (y_scale == 1.0)) {
		// if no scaling in effect in the end, don't bother
		finalW=inWidth;
		finalH=inHeight;
	} else {
		x1+=0.5;	// rounding at 0.5
		y1+=0.5;
		finalW=(int)(x1);
		finalH=(int)(y1);
	}
	
	if ((fStretch) && (fLast)) {
		// this is the last image, so affect stretch mode
		wprintf(L"Note: stretch is overriding aspect ratio based scaling\n");
		finalW=currentw;
		finalH=currenth;
	}

	wprintf(L"Output size: %d x %d\n", finalW, finalH);

	if ((finalW==inWidth) && (finalH==inHeight)) {
		// just copy to the temp buffer
        tmpBuffer = (HGLOBAL)mymalloc(finalW*finalH*3);
		memcpy(tmpBuffer, hBuffer, finalW*finalH*3);
	} else {
		// do the scale - just use ImgSource - it has really nice scaling!
        // the modes are:
        //Mode     Name                 Notes
        //0        box filter 
        //1        triangle filter
        //2        Hamming filter
        //3        Gaussian filter      min dimension : 2
        //4        bell filter          min dimension : 2
        //5        B-spline filter      min dimension : 2
        //6        cubic 1 filter       min dimension : 2
        //7        cubic 2 filter       min dimension : 2
        //8        Lanczos3 filter      min dimension : 3
        //9        Mitchell filter      min dimension : 2
        //10       sinc filter          min dimension : 4
        //11       Hermite filter
        //12       Hanning filter
        //13       Catrom filter        min dimension : 2
        //14       area-average (fast)  Reduction only
        //15       area-average         Reduction only
        //16       bi-linear interpolation
        //17       bi-cubic interpolation
        //18       nearest neighbor     very fast
        const int mode = 5;
        tmpBuffer = (HGLOBAL)mymalloc(finalW * finalH * 3);
        IS40_ResizeImage((BYTE*)hBuffer, inWidth, inHeight, inWidth*3, (BYTE*)tmpBuffer, finalW, finalH, finalW*3, 3, mode, 0);
	}

	if (NULL == hBuffer2) {
        hBuffer2 = (HGLOBAL)mymalloc(iWidth * (iHeight+1) * 3);
		ZeroMemory(hBuffer2, iWidth * (iHeight+1) * 3);
	}

	// calculate the exact position. If this is the last image (remaining space will be too small),
	// then we will center this one in the remaining space
	thisx=currentx;
	thisy=currenty;

	if (maxscale != -1) {
		// the scale axis is full, so we only reduce the other one!
		if (ScaleMode==Y_AXIS) {
			if ((fLast)&&((unsigned)currentw > finalW)) {
				// last image, so center it and clear remaining space
				wprintf(L"Rect is full - centering\n");
				thisx+=(currentw-finalW)/2;
			}
		} else {
			if ((fLast)&&((unsigned)currenth > finalH)) {
				// last image, so center it and clear remaining space
				wprintf(L"Rect is full - centering\n");
				thisy+=(currenth-finalH)/2;
			}
		}
	}

	// update fillbuffer
	if (NULL != FillBuffer) {
		if ((fCoolSpacing) && (fFirstFile)) {
			// regardless of the final size - pad 48x48. we assume we placed it at 0,0!
			RECT myRect;
			if ((thisy!=0)||(thisx!=0)) {
				wprintf(L"Warning: Cool Spacing is not happening at 0,0 like we expect - %d,%d\n", thisx,thisy);
			}
			if ((finalH>48)||(finalW>48)) {
				wprintf(L"Warning: Cool Spacing is not less than 48x48 like we expect - %d,%d\n", finalW,finalH);
			}
			myRect.top=thisy;
			myRect.left=thisx;
			myRect.bottom=thisy+47;
			myRect.right=thisx+47;
			IS40_FillSolidRect((BYTE*)FillBuffer, iWidth, iHeight, 1, iWidth, &myRect, 0xff, 0);

			wprintf(L"Padding to 48x48 and centering..\n");
			// Horizontally center the character in the 48 pixel block
			thisx+=(48-finalW)/2;
		} else {
			if ((fLast)&&(maxscale != -1)) {
				RECT myRect;
				myRect.top=currenty;
				myRect.left=currentx;
				myRect.bottom=currenty+currenth-1;
				myRect.right=currentx+currentw-1;
				IS40_FillSolidRect((BYTE*)FillBuffer, iWidth, iHeight, 1, iWidth, &myRect, 0xff, 0);
			} else {
				RECT myRect;
				myRect.top=thisy;
				myRect.left=thisx;
				myRect.bottom=thisy+finalH;	// todo: there is a bug here somewhere, we should have to do -1 here 
				myRect.right=thisx+finalW-1;	
				IS40_FillSolidRect((BYTE*)FillBuffer, iWidth, iHeight, 1, iWidth, &myRect, 0xff, 0);
			}
		}
	}

	currenth-=finalH;
	currentw-=finalW;
	currentx+=finalW;
	currenty+=finalH;

	wprintf(L"Draw at: %d,%d\n", thisx, thisy);

	if (fFirstFile) {
		fFirstFile=false;
		if (bgcolor & 0x80000000) {
			// get color from image
			BYTE *p=(BYTE*)tmpBuffer;
			bgcolor=(p[0]<<16)|(p[1]<<8)|(p[2])|0x80000000;		// preserve the flag
		}
		// if color isn't black, color the buffer
		if (bgcolor != 0) {
			BYTE *p=(BYTE*)hBuffer2;
			int r,g,b;
			r=(bgcolor&0x00ff0000)>>16;
			g=(bgcolor&0x0000ff00)>>8;
			b=(bgcolor&0x000000ff);

			for (unsigned int idx=0; idx<iWidth*iHeight; idx++) {
				*p++=r;
				*p++=g;
				*p++=b;
			}
		}
	}

	// Doing the draw myself because ImgSrc is getting too fancy, and the OverlayImageColor function isn't working for me
	unsigned int tmpy=0;
	for (unsigned int rows=thisy; rows<thisy+finalH; rows++) {
		if (rows>iHeight) break;
		BYTE *pAddr=((BYTE*)hBuffer2)+(rows*iWidth*3)+(thisx*3);
		memcpy(pAddr, ((BYTE*)tmpBuffer)+((tmpy++)*finalW*3), finalW*3);
	}

	if (NULL != hOutWnd) {
		// also write a copy to the passed in window handle
		HDC myDC=::GetDC(hOutWnd);
		if (NULL != myDC) {
			IS40_DrawRGB(myDC, (BYTE*)tmpBuffer, finalW, finalH, finalW*3, thisx+drawOffsetX, thisy+drawOffsetY, NULL);
			::ReleaseDC(hOutWnd, myDC);
		}
	}

    if (NULL != tmpBuffer) {
        free(tmpBuffer);
        tmpBuffer = NULL;
    }

	// before writing the map, see if we got mandatory offset information
	if ((bNeedVpag)&&(!bGotVpag)) {
		wprintf(L"Error: Did not get mandatory vpAg chunk\n");
		nFailedFiles++;
	}
	if ((bNeedOffs)&&(!bGotOffs)) {
		wprintf(L"Error: Did not get mandatory oFFs chunk\n");
		nFailedFiles++;
	}

	if ('\0' != szMap[0]) {
		// write to the map file, we do it kind of sloppy here but it'll work
		FILE *fp;
		fp=_wfopen(szMap, L"a");
		if (NULL == fp) {
			wprintf(L"Failed to create map file!\n");
			wcscpy(szMap, L"");
		} else {
			fwprintf(fp, L"%s|%d|%d|%d|%d|%d|%d|%d|%d\n", szFile, thisx, thisy, finalW, finalH, mapOrigW, mapOrigH, mapOffW, mapOffH);
			fclose(fp);
		}
	}

	return true;
}

int instr(wchar_t *s1, wchar_t *s2)
{
	while (*s1)
	{
		if (*s1 != *s2) {
			s1++;
		} else {
			break;
		}
	}

	if (0 == *s1) {
		return 0;
	}

	while (*s2)
	{
		if (*(s1++) != *(s2++)) {
			return 0;
		}
	}

	return 1;
}

// These functions work by manipulating the setup parameters and fill buffer, then calling the
// main code to process.
int nCurQuad;

int TotalUp(wchar_t *pStr) {
	// return a total count from the passed string (ignore substrings)
	int nTotal1=0;
	int nCurQuad=0;
	
	while (pStr[nCurQuad]) {
		if (isdigit(pStr[nCurQuad])) {
			nTotal1+=pStr[nCurQuad]-'0';
		} else if ('[' == pStr[nCurQuad]) {
			// skip this block
			while ((pStr[nCurQuad])&&(']'!=pStr[nCurQuad])) nCurQuad++;
			if ('\0'==pStr[nCurQuad]) nCurQuad--;	// an error actually
		} else if ((' '==pStr[nCurQuad])||(']' == pStr[nCurQuad])) {
			break;
		} else {
			wprintf(L"Error parsing quadrant string!\n");
			check(5);
		}
		nCurQuad++;
	}

	return nTotal1;
}

void shufflestring(wchar_t *qstring) {
	// shuffles qstring in place
	// shuffle the string, but make sure subquads stay with
	// their parent (should shuffle the subquads among themselves
	// too, though.
	int len=wcslen(qstring);
	for (int i=0; i<len; i++) {
		int p1=rand()%len;
		int p2=rand()%len;
		if (p1 == p2) {
			continue;
		}
		// make sure p1 is the lower one
		if (p2 < p1) {
			int tmp=p2;
			p2=p1;
			p1=tmp;
		}
		// make sure that the random setting is at the same level
		int nbracket1=0;
		int nbracket2=0;
		for (int z=0; z<p1; z++) {
			if ((qstring[z]=='[') || (qstring[z]==']')) {
				nbracket1++;
			}
		}
		for (int z=0; z<p2; z++) {
			if ((qstring[z]=='[') || (qstring[z]==']')) {
				nbracket2++;
			}
		}
		// if they are both even, we are okay. Otherwise they
		// must be the same number
		if ((nbracket1&0x01)||(nbracket2&0x01)) {
			if (nbracket1!=nbracket2) {
				continue;
			}
		}

		// last check, make sure we aren't pointing at a bracket
		if ((isdigit(qstring[p1])) && (isdigit(qstring[p2]))) {
			wchar_t buf1[256], buf2[256];
			memset(buf1, 0, 256);
			memset(buf2, 0, 256);
			buf1[0]=qstring[p1];
			if (qstring[p1+1]=='[') {
				// need to copy the substring too
				int cnt=1;
				while (qstring[p1+cnt]) {
					buf1[cnt]=qstring[p1+cnt];
					if (buf1[cnt]==']') break;
					cnt++;
				}
			}
			buf2[0]=qstring[p2];
			if (qstring[p2+1]=='[') {
				// need to copy the substring too
				int cnt=1;
				while (qstring[p2+cnt]) {
					buf2[cnt]=qstring[p2+cnt];
					if (buf2[cnt]==']') break;
					cnt++;
				}
			}
			// avoid overlap
			if (p1+(signed)wcslen(buf1) > p2) {
				continue;
			}
			// now get the 3 pieces of the string
			wchar_t ps1[256], ps2[256], ps3[256];
			wcscpy(ps1, qstring);
			ps1[p1]=L'\0';
			wcscpy(ps2, &qstring[p1+wcslen(buf1)]);
			ps2[p2-(p1+wcslen(buf1))]=L'\0';
			wcscpy(ps3, &qstring[p2+wcslen(buf2)]);
			// and now put it all back together
			wcscpy(qstring, ps1);
			wcscat(qstring, buf2);
			wcscat(qstring, ps2);
			wcscat(qstring, buf1);
			wcscat(qstring, ps3);
		}
	}
}

#define MAXMONITORS 9
int NumMonitors=0;
RECT MonitorRect[MAXMONITORS];

// it seems we may need to set per monitor DPI aware to not get scaled
// in Win8.1 and later: https://msdn.microsoft.com/en-us/library/windows/desktop/dn280512(v=vs.85).aspx
// try to do it in a backwards compatible way.
BOOL CALLBACK monitorfunc(HMONITOR hMon, HDC , LPRECT rect, LPARAM ) {
	// it's possible to get duplicates (different modes? I didn't look at why)
	// we only care about the geometry, not the details.
	if (NumMonitors < MAXMONITORS) {
		for (int idx = 0; idx<NumMonitors; idx++) {
			if ((MonitorRect[idx].top == rect->top)&&(MonitorRect[idx].left == rect->left)) return TRUE;
		}
		MonitorRect[NumMonitors++] = *rect;
		return TRUE;
	}
	return FALSE;
}

void ProcessMonitors() {
	int MasterWidth=0, MasterHeight=0;
	int MinX=999999,MaxX=-999999,MinY=999999,MaxY=-999999;

	// collect the information about the monitors...
	if (!EnumDisplayMonitors(NULL, NULL, monitorfunc, NULL)) {
		wprintf(L"Failed to enumerate monitors\n");
		exit(-1);
	}

	// we run the program for each monitor, and we stitch it all together
	for (int idx=0; idx<NumMonitors; idx++) {
		if (MonitorRect[idx].left < MinX) MinX = MonitorRect[idx].left;
		if (MonitorRect[idx].right > MaxX) MaxX = MonitorRect[idx].right;
		if (MonitorRect[idx].top < MinY) MinY = MonitorRect[idx].top;
		if (MonitorRect[idx].bottom > MaxY) MaxY = MonitorRect[idx].bottom;
	}
	MasterWidth=MaxX-MinX;
	MasterHeight=MaxY-MinY;

	wprintf(L"\nDetected %d monitors, with overall size of %d x %d\n\n", NumMonitors, MasterWidth, MasterHeight);

	// create a master buffer - this will eventually become hBuffer2 before we return
	BYTE *hMasterBuf = (BYTE*)mymalloc(MasterWidth*(MasterHeight+1)*3);
	ZeroMemory(hMasterBuf, MasterWidth*(MasterHeight+1)*3);

	// Prior to Windows 10 (not sure about 8!), a tiled image at 0,0 started at the primary monitor,
	// and monitors left of that were negative, requiring the image to 'wrap around'.
	// In Windows 10 (and probably 8, I haven't tested), the tiled image starts at the leftmost monitor
	// and goes rightward. So, we need to account for this difference in order to create a correct
	// image. We detect the OS and set alignX and alignY as offets to the coordinates.
	int alignX = 0, alignY = 0;
	int drawAlignX = 0, drawAlignY = 0;
	{
	    DWORD dwVersion = 0; 
		DWORD dwMajorVersion = 0;
		DWORD dwMinorVersion = 0; 
		dwVersion = GetVersion();
	    dwMajorVersion = (DWORD)(LOBYTE(LOWORD(dwVersion)));
	    dwMinorVersion = (DWORD)(HIBYTE(LOWORD(dwVersion)));

		// enumerate the monitors and create an offset from the leftmost, topmost one - we need this either way
		for (int idx=0; idx<NumMonitors; idx++) {
			if (MonitorRect[idx].left < drawAlignX) drawAlignX = MonitorRect[idx].left;
			if (MonitorRect[idx].top < drawAlignY) drawAlignY = MonitorRect[idx].top;
		}

		// Check for Windows 8 or higher (6.2, Win7 is 6.1)
		if (((dwMajorVersion == 6) && (dwMinorVersion >= 2)) || (dwMajorVersion > 6)) {
			alignX = drawAlignX;
			alignY = drawAlignY;
			wprintf(L"* Detected Win8 or above, adjusting image offset to left edge %d,%d\n", alignX, alignY);
		}
	}

	// process each monitor in turn
	for (int idx=0; idx<NumMonitors; idx++) {

		iWidth = MonitorRect[idx].right-MonitorRect[idx].left;
		iHeight = MonitorRect[idx].bottom-MonitorRect[idx].top;

		// similar to the Windows 8 alignment, but only used for drawing to the passed in window, all OS's
		// that's why I need a separate set of variables
		drawOffsetX = MonitorRect[idx].left - drawAlignX;
		drawOffsetY = MonitorRect[idx].top - drawAlignY;

		wprintf(L"\n---\nProcessing monitor %d %dx%d at %d,%d\n---\n", idx, iWidth, iHeight, MonitorRect[idx].left, MonitorRect[idx].top);

		if (NULL != hquads) {
			ProcessHQuads();
		} else if (NULL != vquads) {
			ProcessVQuads();
		} else {
			DoOneRun();
		}

		// it's possible for a monitor to find no valid pictures, in that case, hBuffer2 was never created
		// We should probably error out, for now there's not much we can do
		if (NULL != hBuffer2) {
			// now we have one monitor's worth of buffer - put it in the right place on the buffer
			// monitor alignment means that except for the monitor at 0,0, we may need to deal with
			// wrap. My monitors are all horizontal - how would it work vertically? We'll try this.
			// This is a little slower, but to handle the wrap properly I will just copy by pixel for now
			for (unsigned int x=0; x<iWidth; x++) {
				for (unsigned int y=0; y<iHeight; y++) {
					int tx = x + MonitorRect[idx].left;
					int ty = y + MonitorRect[idx].top;
					// remember the aligns are negative if they are anything
					tx -= alignX;
					ty -= alignY;
					if (tx >= MasterWidth) tx-=MasterWidth;
					if (tx < 0) tx+=MasterWidth;
					if (ty >= MasterHeight) ty-=MasterHeight;
					if (ty < 0) ty+=MasterHeight;

					memcpy(&hMasterBuf[(ty*MasterWidth+tx)*3], &(((BYTE*)hBuffer2)[(y*iWidth+x)*3]), 3);
				}
			}

			// destroy the old hBuffer2, and repeat
            free(hBuffer2);
			hBuffer2=NULL;
		} else {
			wprintf(L"Warning! No images at all for monitor %d\n", idx);
		}
		if (NULL != FillBuffer) {
			free(FillBuffer);
			FillBuffer=NULL;
		}
	}

	// copy the master buffer over
	hBuffer2 = hMasterBuf;
	iWidth = MasterWidth;
	iHeight = MasterHeight;

	// and see if the caller likes that ;)
}

void ProcessHQuads() {
	wchar_t qstring[256];
	int nTotal1, nTotal2;
	int nthisx, nthisy;

	wcsncpy(qstring, hquads, 256);
	qstring[255]='\0';
	// find end of the string
	nCurQuad=0;
	while (qstring[nCurQuad]) {
		if (' ' == qstring[nCurQuad]) {
			qstring[nCurQuad]='\0';
			break;
		}
		nCurQuad++;
	}

	// process the string into the array
	nTotal1=TotalUp(qstring);

	if (fRandomQuads) {
		shufflestring(qstring);
	}

	// Create the output buffer here
	if (NULL == hBuffer2) {
		hBuffer2=(HGLOBAL)mymalloc(iWidth * (iHeight+1) * 3);
		ZeroMemory(hBuffer2, iWidth * iHeight * 3);
	}
	// And the fill buffer
	if (NULL == FillBuffer) {
		FillBuffer=(BYTE*)mymalloc(iWidth*iHeight);	// 8-bit buffer just to mark used areas
		ZeroMemory(FillBuffer, iWidth*iHeight);
	}

	// Now we have a total, so we can process each step
	nCurQuad=0;
	nthisx=0;
	nthisy=0;

	while (qstring[nCurQuad]) {
		if ('[' != qstring[nCurQuad]) {
			currentw=0;
			currenth=0;
		}
		if (isdigit(qstring[nCurQuad])) {
			int x=qstring[nCurQuad]-'0';
			if ((x>9)||(x<1)) {	// only 0 really triggers here
				wprintf(L"Bad value in quadrant string!\n");
				check(5);
			}
			double ratio=(double)nTotal1/(double)x;
			currentw=(int)((double)iWidth/ratio);
			currenth=iHeight;
			currentx=nthisx;
			currenty=nthisy;

			// run the code as long as it's not divided further
			if ('[' != qstring[nCurQuad+1]) {
				// Update the fill buffer for JUST the space we want
				memset(FillBuffer, 0xff, iWidth*iHeight);
				wprintf(L"Clearing fill buffer at (%d,%d) for %dx%d\n", 
					nthisx,nthisy,currentw,currenth);
				for (int x=0; x<currentw; x++) {
					for (int y=0; y<currenth; y++) {
						FillBuffer[(y+nthisy)*iWidth+(x+nthisx)]=0;
					}
				}

				// update values before they are corrupted
				nthisx+=currentw;
				nthisy=0;

				DoOneRun();
			}
		} else if ('[' == qstring[nCurQuad]) {
			// process a subblock. We don't permit multiple nesting,
			// so we'll just repeat the code here
			nCurQuad++;

			// This one is now VERTICAL, not horizontal
			// Our main location is nthisx,nthisy and we are currentw,iheight sized
			// we need to preserve that data for when we exit
			int nsubx, nsuby, nsubw, nsubh;
			nsubx=nthisx;
			nsuby=nthisy;
			nsubw=currentw;
			nsubh=iHeight;
			// and update the this values so we don't forget later
			nthisx+=currentw;
			nthisy=0;
			// from here on, use the nsub versions
			nTotal2=TotalUp(&qstring[nCurQuad]);
				
			while (qstring[nCurQuad]) {
				currentw=0;
				currenth=0;
				if (isdigit(qstring[nCurQuad])) {
					int x=qstring[nCurQuad]-'0';
					if ((x>9)||(x<1)) {	// only 0 really triggers here
						wprintf(L"Bad value in quadrant string!\n");
						check(5);
					}
					double ratio=(double)nTotal2/(double)x;
					currentw=nsubw;
					currenth=(int)((double)nsubh/ratio);
					currentx=nsubx;
					currenty=nsuby;

					// Update the fill buffer for JUST the space we want
					memset(FillBuffer, 0xff, iWidth*iHeight);
					wprintf(L"Clearing fill buffer at sub (%d,%d) for %dx%d\n", 
						nsubx,nsuby,currentw,currenth);
					for (int x=0; x<currentw; x++) {
						for (int y=0; y<currenth; y++) {
							FillBuffer[(y+nsuby)*iWidth+(x+nsubx)]=0;
						}
					}

					// update values before they are corrupted
					nsuby+=currenth;

					// run the code
					DoOneRun();
				} else if ((' ' == qstring[nCurQuad])||(']' == qstring[nCurQuad])) {
					break;
				} else {
					// error!
					wprintf(L"Error parsing quadrant substring digits!\n");
					check(6);
				}
				nCurQuad++;
			}
		} else if (' ' == qstring[nCurQuad]) {
			break;
		} else {
			// error!
			wprintf(L"Error parsing quadrant string digits!\n");
			check(5);
		}
		nCurQuad++;
	}
}

void ProcessVQuads() {
	wchar_t qstring[256];
	int nTotal1, nTotal2;
	int nthisx, nthisy;

	wcsncpy(qstring, vquads, 256);
	qstring[255]='\0';
	// find end of the string
	nCurQuad=0;
	while (qstring[nCurQuad]) {
		if (' ' == qstring[nCurQuad]) {
			qstring[nCurQuad]='\0';
			break;
		}
		nCurQuad++;
	}

	// process the string into the array
	nTotal1=TotalUp(qstring);

	if (fRandomQuads) {
		shufflestring(qstring);
	}

	// Create the output buffer here
	if (NULL == hBuffer2) {
		hBuffer2=(HGLOBAL)mymalloc(iWidth * (iHeight+1) * 3);
		ZeroMemory(hBuffer2, iWidth * (iHeight+1) * 3);
	}
	// And the fill buffer
	if (NULL == FillBuffer) {
		FillBuffer=(BYTE*)mymalloc(iWidth*iHeight);	// 8-bit buffer just to mark used areas
		ZeroMemory(FillBuffer, iWidth*iHeight);
	}

	// Now we have a total, so we can process each step
	nCurQuad=0;
	nthisx=0;
	nthisy=0;

	while (qstring[nCurQuad]) {
		if ('[' != qstring[nCurQuad]) {
			currentw=0;
			currenth=0;
		}
		if (isdigit(qstring[nCurQuad])) {
			int x=qstring[nCurQuad]-'0';
			if ((x>9)||(x<1)) {	// only 0 really triggers here
				wprintf(L"Bad value in quadrant string!\n");
				check(5);
			}
			double ratio=(double)nTotal1/(double)x;
			currentw=iWidth;
			currenth=(int)((double)iHeight/ratio);
			currentx=nthisx;
			currenty=nthisy;

			// run the code as long as it's not divided further
			if ('[' != qstring[nCurQuad+1]) {
				// Update the fill buffer for JUST the space we want
				memset(FillBuffer, 0xff, iWidth*iHeight);
				wprintf(L"Clearing fill buffer at (%d,%d) for %dx%d\n", 
					nthisx,nthisy,currentw,currenth);
				for (int x=0; x<currentw; x++) {
					for (int y=0; y<currenth; y++) {
						FillBuffer[(y+nthisy)*iWidth+(x+nthisx)]=0;
					}
				}

				// update values before they are corrupted
				nthisy+=currenth;
				nthisx=0;

				DoOneRun();
			}
		} else if ('[' == qstring[nCurQuad]) {
			// process a subblock. We don't permit multiple nesting,
			// so we'll just repeat the code here
			nCurQuad++;

			// This one is now horizontal, not vertical
			// Our main location is nthisx,nthisy and we are iwidth,currenth sized
			// we need to preserve that data for when we exit
			int nsubx, nsuby, nsubw, nsubh;
			nsubx=nthisx;
			nsuby=nthisy;
			nsubw=iWidth;
			nsubh=currenth;
			// and update the this values so we don't forget later
			nthisx=0;
			nthisy+=currenth;
			// from here on, use the nsub versions
			nTotal2=TotalUp(&qstring[nCurQuad]);
				
			while (qstring[nCurQuad]) {
				currentw=0;
				currenth=0;
				if (isdigit(qstring[nCurQuad])) {
					int x=qstring[nCurQuad]-'0';
					if ((x>9)||(x<1)) {	// only 0 really triggers here
						wprintf(L"Bad value in quadrant string!\n");
						check(5);
					}
					double ratio=(double)nTotal2/(double)x;
					currentw=(int)((double)nsubw/ratio);
					currenth=nsubh;
					currentx=nsubx;
					currenty=nsuby;

					// Update the fill buffer for JUST the space we want
					memset(FillBuffer, 0xff, iWidth*iHeight);
					wprintf(L"Clearing fill buffer at sub (%d,%d) for %dx%d\n", 
						nsubx,nsuby,currentw,currenth);
					for (int x=0; x<currentw; x++) {
						for (int y=0; y<currenth; y++) {
							FillBuffer[(y+nsuby)*iWidth+(x+nsubx)]=0;
						}
					}

					// update values before they are corrupted
					nsubx+=currentw;

					// run the code
					DoOneRun();
				} else if ((' ' == qstring[nCurQuad])||(']' == qstring[nCurQuad])) {
					break;
				} else {
					// error!
					wprintf(L"Error parsing quadrant substring digits!\n");
					check(6);
				}
				nCurQuad++;
			}
		} else if (' ' == qstring[nCurQuad]) {
			break;
		} else {
			// error!
			wprintf(L"Error parsing quadrant string digits!\n");
			check(5);
		}
		nCurQuad++;
	}
}
