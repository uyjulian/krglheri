
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <objidl.h>
#include "tp_stub.h"
#include "TVPBinaryStreamShim.h"
#include "LayerBitmapIntf.h"
#define EXPORT(hr) extern "C" __declspec(dllexport) hr __stdcall

extern void TVPLoadERI(void* formatdata, void *callbackdata, tTVPGraphicSizeCallback sizecallback,
	tTVPGraphicScanLineCallback scanlinecallback, tTVPMetaInfoPushCallback metainfopushcallback,
	tTJSBinaryStream *src, tjs_int keyidx,  tTVPGraphicLoadMode mode);

void TVPLoadERIForPlugin(void* formatdata, void *callbackdata, tTVPGraphicSizeCallback sizecallback,
	tTVPGraphicScanLineCallback scanlinecallback, tTVPMetaInfoPushCallback metainfopushcallback,
	IStream *isrc, tjs_int keyidx, tTVPGraphicLoadMode mode)
{
	tTJSBinaryStream src(isrc);
	TVPLoadERI(formatdata, callbackdata, sizecallback, scanlinecallback, metainfopushcallback, &src, keyidx, mode);
}

extern void TVPLoadHeaderERI(void* formatdata, tTJSBinaryStream *src, iTJSDispatch2** dic );
void TVPLoadHeaderERIForPlugin(void* formatdata, IStream *isrc, iTJSDispatch2** dic)
{
	tTJSBinaryStream src(isrc);
	TVPLoadHeaderERI(formatdata, &src, dic);
}

extern void TVPSaveAsERI(void* formatdata, tTJSBinaryStream* dst, const tTVPBaseBitmap* image, const ttstr & mode, iTJSDispatch2* meta );

void TVPSaveAsERIForPlugin(void* formatdata, void* callbackdata, IStream* idst, const ttstr & mode, tjs_uint width, tjs_uint height, tTVPGraphicSaveScanLineCallback scanlinecallback, iTJSDispatch2* meta )
{
	tTJSBinaryStream dst(idst);
	tTVPBaseBitmap bmp(width, height, callbackdata, scanlinecallback);
	TVPSaveAsERI(formatdata, &dst, &bmp, mode, meta);
}

extern bool TVPAcceptSaveAsERI(void* formatdata, const ttstr & type, class iTJSDispatch2** dic);

BOOLEAN WINAPI DllMain(HMODULE hDllHandle, DWORD nReason, LPVOID lpReserved)
{
	switch (nReason)
	{
		case DLL_PROCESS_ATTACH:
		{
			DisableThreadLibraryCalls(hDllHandle);
			break;
		}
	}

	return TRUE;
}

static tjs_int GlobalRefCountAtInit = 0;
EXPORT(HRESULT) V2Link(iTVPFunctionExporter *exporter)
{
	TVPInitImportStub(exporter);

	TVPRegisterGraphicLoadingHandler( ttstr(TJS_W(".eri")), TVPLoadERIForPlugin, TVPLoadHeaderERIForPlugin, TVPSaveAsERIForPlugin, TVPAcceptSaveAsERI, NULL );

	GlobalRefCountAtInit = TVPPluginGlobalRefCount;
	return S_OK;
}

EXPORT(HRESULT) V2Unlink() {
	if(TVPPluginGlobalRefCount > GlobalRefCountAtInit) return E_FAIL;

	TVPUnregisterGraphicLoadingHandler( ttstr(TJS_W(".eri")), TVPLoadERIForPlugin, TVPLoadHeaderERIForPlugin, TVPSaveAsERIForPlugin, TVPAcceptSaveAsERI, NULL );

	TVPUninitImportStub();
	return S_OK;
}
