
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <objidl.h>

#include "tp_stub.h"
#include "TVPBinaryStreamShim.h"
#include "LayerBitmapIntf.h"

#define TVPERILoadError TJS_W("ERI Read Error /%1")

//---------------------------------------------------------------------------
// ERI loading handler
//---------------------------------------------------------------------------

#ifdef __BORLANDC__
#pragma option push -pc -Vms
	// fix me: need platform independ one
#pragma pack(push, 1)
#endif

#include <eritypes.h>
#include <erinalib.h>

#ifdef __BORLANDC__
#pragma pack(pop)
#endif

//---------------------------------------------------------------------------
// eriAllocateMemory and eriFreeMemory
//---------------------------------------------------------------------------
PVOID eriAllocateMemory( DWORD dwBytes )
{
	return TJSAlignedAlloc(dwBytes, 4);
}
//---------------------------------------------------------------------------
void eriFreeMemory( PVOID ptrMem )
{
	TJSAlignedDealloc(ptrMem);
}
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// tTVPEFileObject : class for storage access
//---------------------------------------------------------------------------
class tTVPEFileObject : public ::EFileObject
{
	tTJSBinaryStream * Stream;
public:
	tTVPEFileObject(tTJSBinaryStream *ref);
	~tTVPEFileObject();

	virtual DWORD Read( void * ptrBuf, DWORD dwByte );
	virtual DWORD GetLength( void );
	virtual DWORD GetPointer( void );
	virtual void Seek( DWORD dwPointer );
};
//---------------------------------------------------------------------------
tTVPEFileObject::tTVPEFileObject(tTJSBinaryStream *ref)
{
	// tTVPEFileObject constructor

	// this object does not own "ref", so user routine still has responsibility
	// for "ref".

	Stream = ref;
//	if(ref->GetSize() & TJS_UI64_VAL(0xffffffff00000000))
	// if-statement above may cause a bcc i64 comparison-to-zero bug
	if(((tjs_uint64)ref->GetSize()) >> 32) // this is still buggy, but works
	{
		// too large to handle with tEFlieObject
		TVPThrowExceptionMessage(TVPERILoadError, TJS_W("Too large storage"));
	}
}
//---------------------------------------------------------------------------
tTVPEFileObject::~tTVPEFileObject()
{
	// tTVPEFileObject destructor
	// nothing to do
}
//---------------------------------------------------------------------------
DWORD tTVPEFileObject::Read( void * ptrBuf, DWORD dwByte )
{
	return (DWORD)Stream->Read(ptrBuf, dwByte);
}
//---------------------------------------------------------------------------
DWORD tTVPEFileObject::GetLength( void )
{
	return (DWORD)Stream->GetSize();
}
//---------------------------------------------------------------------------
DWORD tTVPEFileObject::GetPointer( void )
{
	return (DWORD)Stream->GetPosition();
}
//---------------------------------------------------------------------------
void tTVPEFileObject::Seek( DWORD dwPointer )
{
	Stream->SetPosition(dwPointer);
}
//---------------------------------------------------------------------------


//---------------------------------------------------------------------------
// tTVPRLHDecodeContext
//---------------------------------------------------------------------------
class tTVPRLHDecodeContext : public ::RLHDecodeContext
{
	::ERIFile & ERIFileObject;

public:
	tTVPRLHDecodeContext(::ERIFile & erifile, ULONG nBufferingSize)
		: ERIFileObject(erifile), RLHDecodeContext(nBufferingSize) {};
	virtual ULONG ReadNextData(PBYTE ptrBuffer, ULONG nBytes )
		{ return ERIFileObject.Read(ptrBuffer, nBytes); }
};
//---------------------------------------------------------------------------


#ifdef __BORLANDC__
#pragma option pop
#endif

#if 0
#ifdef _M_IX86
#include "tvpgl_ia32_intf.h"
#endif
#endif

//---------------------------------------------------------------------------
static bool TVPERINAInit = false;
static void TVPInitERINA()
{
	if(TVPERINAInit) return;
	::eriInitializeLibrary();

	// sync simd features with the library.
#if 0
#ifdef _M_IX86
	DWORD flags;

	flags = (TVPCPUType & TVP_CPU_HAS_MMX) ? ERI_USE_MMX_PENTIUM : 0 +
			(TVPCPUType & TVP_CPU_HAS_SSE) ? ERI_USE_XMM_P3 : 0;
	if(flags) ::eriEnableMMX(flags);

	flags = !(TVPCPUType & TVP_CPU_HAS_MMX) ? ERI_USE_MMX_PENTIUM : 0 +
			!(TVPCPUType & TVP_CPU_HAS_SSE) ? ERI_USE_XMM_P3 : 0;
	if(flags) ::eriDisableMMX(flags);
#endif
#endif

	TVPERINAInit = true;
}
//---------------------------------------------------------------------------
#if 0
static void TVPUninitERINA()
{
	if(TVPERINAInit)
		::eriCloseLibrary();
}
//---------------------------------------------------------------------------
static tTVPAtExit TVPUninitERINAAtExit
	(TVP_ATEXIT_PRI_CLEANUP, TVPUninitERINA);
//---------------------------------------------------------------------------
#endif



//---------------------------------------------------------------------------
void TVPLoadERI(void* formatdata, void *callbackdata, tTVPGraphicSizeCallback sizecallback,
	tTVPGraphicScanLineCallback scanlinecallback, tTVPMetaInfoPushCallback metainfopushcallback,
	tTJSBinaryStream *src, tjs_int keyidx,  tTVPGraphicLoadMode mode)
{
	// ERI loading handler
	TVPInitERINA();

	// ERI-chan ( stands for "Entis Rasterized Image" ) will be
	// available at http://www.entis.jp/eri/

	// currently TVP's ERI handler does not support palettized ERI loading
	if(mode == glmPalettized)
		TVPThrowExceptionMessage(TVPERILoadError,
			TJS_W("currently Kirikiri does not support loading palettized ERI image."));

	// create wrapper object for EFileObject
	tTVPEFileObject fileobject(src);

	// open with ERIFile
	::ERIFile erifile;
	if(!erifile.Open(&fileobject))
	{
		TVPThrowExceptionMessage(TVPERILoadError, TJS_W("ERIFile::Open failed."));
	}

	// set size
	sizecallback(callbackdata,
		erifile.m_InfoHeader.nImageWidth,
		std::abs(static_cast<int>(erifile.m_InfoHeader.nImageHeight)));

	// set RASTER_IMAGE_INFO up
	bool has_alpha;
	::RASTER_IMAGE_INFO rii;
	if(erifile.m_InfoHeader.dwBitsPerPixel == 8 &&
		!(erifile.m_InfoHeader.fdwFormatType & ERI_WITH_ALPHA))
	{
		// grayscaled or paletted image
		rii.fdwFormatType = erifile.m_InfoHeader.fdwFormatType;
		rii.dwBitsPerPixel = 8;
		has_alpha = false;
	}
	else if(erifile.m_InfoHeader.dwBitsPerPixel >= 24)
	{
		rii.fdwFormatType = ERI_RGBA_IMAGE;
		rii.dwBitsPerPixel = 32;
		has_alpha = erifile.m_InfoHeader.fdwFormatType & ERI_WITH_ALPHA;
	}
	else
	{
		// ??
		rii.fdwFormatType = (mode == glmGrayscale) ? ERI_GRAY_IMAGE : ERI_RGBA_IMAGE;
		rii.dwBitsPerPixel = (mode == glmGrayscale) ? 8 : 32;
		has_alpha = erifile.m_InfoHeader.fdwFormatType & ERI_WITH_ALPHA;
	}
	
	tjs_int h;
	tjs_int w;
	rii.nImageWidth = w = erifile.m_InfoHeader.nImageWidth;
	tjs_uint imagesize;
	rii.nImageHeight = h = std::abs(static_cast<int>(erifile.m_InfoHeader.nImageHeight));

	rii.BytesPerLine = ((rii.nImageWidth * rii.dwBitsPerPixel /8)+0x07)& ~0x07;

	rii.ptrImageArray = (PBYTE)malloc(imagesize = h * rii.BytesPerLine);
	if(!rii.ptrImageArray)
		TVPThrowExceptionMessage(TVPERILoadError, TJS_W("Insufficient memory."));


	tjs_uint32 *palette = NULL;
	try
	{
		// create decode context
		tTVPRLHDecodeContext dc(erifile, 0x10000);

		// create decoder
		::ERIDecoder ed;
		if(ed.Initialize(erifile.m_InfoHeader))
			TVPThrowExceptionMessage(TVPERILoadError,
				TJS_W("ERIDecoder::Initialize failed."));

		// decode
		if(ed.DecodeImage(rii, dc, false))
			TVPThrowExceptionMessage(TVPERILoadError,
				TJS_W("ERIDecoder::DecodeImage failed."));

		// prepare the palette
		if(rii.dwBitsPerPixel == 8 &&
			(erifile.m_InfoHeader.fdwFormatType&ERI_TYPE_MASK) != ERI_GRAY_IMAGE)
		{
			palette = new tjs_uint32 [256];
			for(tjs_int i = 0; i<256; i++)
			{
				// CHECK: endian-ness should be considered ??
				tjs_uint32 val = *(tjs_uint32*)&(erifile.m_PaletteTable[i]);
				val |= 0xff000000;
				palette[i] = val;
			}

			// convert palette to monochrome
			if(mode == glmGrayscale)
				TVPDoGrayScale(palette, 256);

			if(keyidx != -1)
			{
				// if color key by palette index is specified
				palette[keyidx&0xff] &= 0x00ffffff; // make keyidx transparent
			}
		}

		// put image back to caller

		// TODO: more efficient implementation
		// (including to write more store functions )
		tjs_int pitch = rii.BytesPerLine;
		const tjs_uint8 *in = ((const tjs_uint8 *)rii.ptrImageArray) +
			imagesize - pitch;
		for(tjs_int i=0; i<h; i++)
		{
			void *scanline = scanlinecallback(callbackdata, i);
			if(!scanline) break;
			if(mode == glmNormal)
			{
				// destination is RGBA
				if(rii.dwBitsPerPixel == 8)
				{
					if((erifile.m_InfoHeader.fdwFormatType&ERI_TYPE_MASK) ==
						ERI_GRAY_IMAGE)
					{
						// source is gray scale
						TVPExpand8BitTo32BitGray((tjs_uint32*)scanline,
							(const tjs_uint8 *)in, w);
					}
					else
					{
						// source is paletted image
						TVPBLExpand8BitTo32BitPal((tjs_uint32*)scanline,
							(const tjs_uint8 *)in, w, palette);
					}

				}
				else
				{
					if(!has_alpha)
					{
						// source does not have alpha
						TVPCopyOpaqueImage((tjs_uint32*)scanline,
							(const tjs_uint32*)in, w);
					}
					else
					{
						TVPBLConvert32BitTo32Bit_AddAlpha(
							(tjs_uint32*)scanline,
							(tjs_uint32*)in, w);
					}
				}
			}
			else
			{
				// destination is grayscale
				if(rii.dwBitsPerPixel == 8)
				{
					if((erifile.m_InfoHeader.fdwFormatType&ERI_TYPE_MASK) ==
						ERI_GRAY_IMAGE)
					{
						// source is gray scale
						memcpy(scanline, in, w);
					}
					else
					{
						// source is paletted image
						TVPBLExpand8BitTo8BitPal((tjs_uint8*)scanline,
							(const tjs_uint8 *)in, w, palette);
					}
				}
				else
				{
					TVPBLConvert32BitTo8Bit((tjs_uint8*)scanline,
						(const tjs_uint32 *)in, w);

				}
			}

			scanlinecallback(callbackdata, -1);
			in -= pitch;
		}
	}
	catch(...)
	{
		if(palette) delete [] palette;
		free(rii.ptrImageArray);
		throw;
	}

	if(palette) delete [] palette;
	free(rii.ptrImageArray);
}

void TVPLoadHeaderERI(void* formatdata, tTJSBinaryStream *src, iTJSDispatch2** dic)
{
	// ERI loading handler
	TVPInitERINA();

	// ERI-chan ( stands for "Entis Rasterized Image" ) will be
	// available at http://www.entis.jp/eri/

	// create wrapper object for EFileObject
	tTVPEFileObject fileobject(src);

	// open with ERIFile
	::ERIFile erifile;
	if(!erifile.Open(&fileobject))
	{
		TVPThrowExceptionMessage(TVPERILoadError, TJS_W("ERIFile::Open failed."));
	}

	*dic = TJSCreateDictionaryObject();
	tTJSVariant val((tjs_int)(erifile.m_InfoHeader.nImageWidth));
	(*dic)->PropSet(TJS_MEMBERENSURE, TJS_W("width"), 0, &val, (*dic) );
	val = tTJSVariant((tjs_int)(std::abs(static_cast<int>(erifile.m_InfoHeader.nImageHeight))));
	(*dic)->PropSet(TJS_MEMBERENSURE, TJS_W("height"), 0, &val, (*dic) );
}

bool TVPAcceptSaveAsERI(void* formatdata, const ttstr & type, class iTJSDispatch2** dic)
{
	// TODO: stub
	return false;
}

void TVPSaveAsERI(void* formatdata, tTJSBinaryStream* dst, const class tTVPBaseBitmap* image, const ttstr & mode, class iTJSDispatch2* meta)
{
	// TODO: stub
}
