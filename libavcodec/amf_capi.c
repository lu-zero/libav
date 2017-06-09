/*
* C API for the AMF media library
*
* Copyright (c) 2015 Advanced Micro Devices, Inc.
* All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice (including the
* disclaimer below) shall be included in all copies or substantial
* portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
* THE SOFTWARE.
*/

#include "amf_capi.h"

#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #define VC_EXTRALEAN
    #include <windows.h>

    typedef HMODULE             DYNLIB_HANDLE;

    #define DYNLIB_OPEN    LoadLibrary
    #define DYNLIB_CLOSE   FreeLibrary
    #define DYNLIB_IMPORT  GetProcAddress
#else
    #include <dlfcn.h>

    typedef void*                   DYNLIB_HANDLE;

    #define DYNLIB_OPEN(path)  dlopen(path, RTLD_NOW | RTLD_GLOBAL)
    #define DYNLIB_CLOSE       dlclose
    #define DYNLIB_IMPORT      dlsym
#endif

static DYNLIB_HANDLE module = NULL;

// Function Pointers
FPAMFCREATECONTEXT amfCreateContext = NULL;
FPAMFCONTEXTTERMINATE amfContextTerminate = NULL;
FPAMFALLOCSURFACE amfAllocSurface = NULL;
FPAMFCREATESURFACEFROMHOSTNATIVE amfCreateSurfaceFromHostNative = NULL;
FPAMFRELEASESURFACE amfReleaseSurface = NULL;
FPAMFRELEASEDATA amfReleaseData = NULL;

FPAMFBUFFERGETSIZE amfBufferGetSize = NULL;
FPAMFBUFFERGETNATIVE amfBufferGetNative = NULL;
FPAMFDATAGETPTS amfDataGetPts = NULL;
FPAMFDATASETPTS amfDataSetPts = NULL;
FPAMFDATAGETDURATION amfDataGetDuration = NULL;
FPAMFDATASETDURATION amfDataSetDuration = NULL;

FPAMFSURFACEGETFORMAT amfSurfaceGetFormat = NULL;
FPAMFSURFACEGETPLANESCOUNT amfSurfaceGetPlanesCount = NULL;
FPAMFSURFACEGETPLANEAT amfSurfaceGetPlaneAt = NULL;
FPAMFSURFACEGETPLANE amfSurfaceGetPlane = NULL;

FPAMFPLANEGETTYPE amfPlaneGetType = NULL;
FPAMFPLANEGETNATIVE amfPlaneGetNative = NULL;
FPAMFPLANEGETPIXELSIZEINBYTES amfPlaneGetSizeInBytes = NULL;
FPAMFPLANEGETOFFSETX amfPlaneGetOffsetX = NULL;
FPAMFPLANEGETOFFSETY amfPlaneGetOffsetY = NULL;
FPAMFPLANEGETWIDTH amfPlaneGetWidth = NULL;
FPAMFPLANEGETHEIGHT amfPlaneGetHeight = NULL;
FPAMFPLANEGETHPITCH amfPlaneGetHPitch = NULL;
FPAMFPLANEGETVPITCH amfPlaneGetVPitch = NULL;

FPAMFCREATECOMPONENT amfCreateComponent = NULL;
FPAMFCOMPONENTINIT amfComponentInit = NULL;
FPAMFCOMPONENTREINIT amfComponentReInit = NULL;
FPAMFCOMPONENTTERMINATE amfComponentTerminate = NULL;
FPAMFCOMPONENTDRAIN amfComponentDrain = NULL;
FPAMFCOMPONENTFLUSH amfComponentFlush = NULL;
FPAMFCOMPONENTSUBMITINPUT amfComponentSubmitInput = NULL;
FPAMFCOMPONENTQUERYOUTPUT amfComponentQueryOutput = NULL;

FPAMFSETPROPERTYBOOL amfSetPropertyBool = NULL;
FPAMFSETPROPERTYINT64 amfSetPropertyInt64 = NULL;
FPAMFSETPROPERTYDOUBLE amfSetPropertyDouble = NULL;
FPAMFSETPROPERTYSTRING amfSetPropertyString = NULL;
FPAMFSETPROPERTYWSTRING amfSetPropertyWString = NULL;
//PFNAMFSETPROPERTYINTERFACE amfSetPropertyInterface = NULL;
FPAMFSETPROPERTYRECT amfSetPropertyRect = NULL;
FPAMFSETPROPERTYSIZE amfSetPropertySize = NULL;
FPAMFSETPROPERTYPOINT amfSetPropertyPoint = NULL;
FPAMFSETPROPERTYRATE amfSetPropertyRate = NULL;
FPAMFSETPROPERTYRATIO amfSetPropertyRatio = NULL;
FPAMFSETPROPERTYCOLOR amfSetPropertyColor = NULL;

FPAMFGETPROPERTYBOOL amfGetPropertyBool = NULL;
FPAMFGETPROPERTYINT64 amfGetPropertyInt64 = NULL;
FPAMFGETPROPERTYDOUBLE amfGetPropertyDouble = NULL;
FPAMFGETPROPERTYSTRING amfGetPropertyString = NULL;
FPAMFGETPROPERTYWSTRING amfGetPropertyWString = NULL;
//PFNAMFGETPROPERTYINTERFACE amfGetPropertyInterface = NULL;
FPAMFGETPROPERTYRECT amfGetPropertyRect = NULL;
FPAMFGETPROPERTYSIZE amfGetPropertySize = NULL;
FPAMFGETPROPERTYPOINT amfGetPropertyPoint = NULL;
FPAMFGETPROPERTYRATE amfGetPropertyRate = NULL;
FPAMFGETPROPERTYRATIO amfGetPropertyRatio = NULL;
FPAMFGETPROPERTYCOLOR amfGetPropertyColor = NULL;

FPAMFINITENCODER amfInitEncoder = NULL;
FPAMFCOMPONENTGETEXTRADATA amfComponentGetExtraData = NULL;
FPAMFCOPYYUV420HOSTTONV12DX9 amfCopyYUV420HostToNV12DX9 = NULL;

void amf_capi_exit(void)
{
    if (module != NULL) {
        DYNLIB_CLOSE(module);
        module = NULL;
    }
}

enum AMF_RESULT amf_capi_init(void)
{
    if (module != NULL)
        return AMF_OK;

#ifdef _WIN32
#ifdef __GNUC__
    {
        char *mediaRoot = getenv("AMDMEDIAROOT");
        char *origPath = getenv("PATH");
        if ((mediaRoot != NULL) && (origPath != NULL)) {
            size_t size = 2 * strlen(mediaRoot) + 10 + strlen(origPath) + 8;
            char *newPath = (char *)malloc(size);
            strcpy(newPath, "PATH=");
            strcat(newPath, mediaRoot);
            strcat(newPath, "\\x64");
            strcat(newPath, ";");
            strcat(newPath, mediaRoot);
            strcat(newPath, "\\x86");
            strcat(newPath, ";");
            strcat(newPath, origPath);
            putenv(newPath);
            free(newPath);
        }
    }
#else
    {
        char *mediaRoot;
        size_t size;
        getenv_s(&size, NULL, 0, "AMDMEDIAROOT");
        if (size > 0) {
            char *origPath;
            char *newPath;
            size_t pSize;
            mediaRoot = (char *)malloc(size);
            getenv_s(&size, mediaRoot, size, "AMDMEDIAROOT");
            getenv_s(&pSize, NULL, 0, "PATH");
            origPath = (char *)malloc(pSize);
            getenv_s(&pSize, origPath, pSize, "PATH");
            newPath = (char *)malloc(size + pSize);
            strcpy_s(newPath, size + pSize, mediaRoot);
            strcat_s(newPath, size + pSize, ";");
            strcat_s(newPath, size + pSize, origPath);
            _putenv_s("PATH", newPath);
            free(mediaRoot);
            free(origPath);
            free(newPath);
        }
    }
#endif
#endif

#ifdef __GNUC__
    module = DYNLIB_OPEN("AMFCInterface.dll");
#else
    module = DYNLIB_OPEN(L"AMFCInterface.dll");
#endif

    if (module == NULL)
        return AMF_FAIL;

    if (atexit(amf_capi_exit)) {
        DYNLIB_CLOSE(module);
        module = NULL;
        return AMF_FAIL;
    }

    // Look up functions

    amfCreateContext = (FPAMFCREATECONTEXT) DYNLIB_IMPORT(module, "amfCreateContext");
    amfContextTerminate = (FPAMFCONTEXTTERMINATE) DYNLIB_IMPORT(module, "amfContextTerminate");
    amfAllocSurface = (FPAMFALLOCSURFACE) DYNLIB_IMPORT(module, "amfAllocSurface");
    amfCreateSurfaceFromHostNative = (FPAMFCREATESURFACEFROMHOSTNATIVE) DYNLIB_IMPORT(module, "amfCreateSurfaceFromHostNative");
    amfReleaseSurface = (FPAMFRELEASESURFACE) DYNLIB_IMPORT(module, "amfReleaseSurface");
    amfReleaseData = (FPAMFRELEASEDATA) DYNLIB_IMPORT(module, "amfReleaseData");

    amfBufferGetSize = (FPAMFBUFFERGETSIZE) DYNLIB_IMPORT(module, "amfBufferGetSize");
    amfBufferGetNative = (FPAMFBUFFERGETNATIVE) DYNLIB_IMPORT(module, "amfBufferGetNative");
    amfDataGetPts = (FPAMFDATAGETPTS) DYNLIB_IMPORT(module, "amfDataGetPts");
    amfDataSetPts = (FPAMFDATASETPTS) DYNLIB_IMPORT(module, "amfDataSetPts");
    amfDataGetDuration = (FPAMFDATAGETDURATION) DYNLIB_IMPORT(module, "amfDataGetDuration");
    amfDataSetDuration = (FPAMFDATASETDURATION) DYNLIB_IMPORT(module, "amfDataSetDuration");

    amfSurfaceGetFormat = (FPAMFSURFACEGETFORMAT) DYNLIB_IMPORT(module, "amfSurfaceGetFormat");
    amfSurfaceGetPlanesCount = (FPAMFSURFACEGETPLANESCOUNT) DYNLIB_IMPORT(module, "amfSurfaceGetPlanesCount");
    amfSurfaceGetPlaneAt = (FPAMFSURFACEGETPLANEAT) DYNLIB_IMPORT(module, "amfSurfaceGetPlaneAt");
    amfSurfaceGetPlane = (FPAMFSURFACEGETPLANE) DYNLIB_IMPORT(module, "amfSurfaceGetPlane");

    amfPlaneGetType = (FPAMFPLANEGETTYPE) DYNLIB_IMPORT(module, "amfPlaneGetType");
    amfPlaneGetNative = (FPAMFPLANEGETNATIVE) DYNLIB_IMPORT(module, "amfPlaneGetNative");
    amfPlaneGetSizeInBytes = (FPAMFPLANEGETPIXELSIZEINBYTES) DYNLIB_IMPORT(module, "amfPlaneGetSizeInBytes");
    amfPlaneGetOffsetX = (FPAMFPLANEGETOFFSETX) DYNLIB_IMPORT(module, "amfPlaneGetOffsetX");
    amfPlaneGetOffsetY = (FPAMFPLANEGETOFFSETY) DYNLIB_IMPORT(module, "amfPlaneGetOffsetY");
    amfPlaneGetWidth = (FPAMFPLANEGETWIDTH) DYNLIB_IMPORT(module, "amfPlaneGetWidth");
    amfPlaneGetHeight = (FPAMFPLANEGETHEIGHT) DYNLIB_IMPORT(module, "amfPlaneGetHeight");
    amfPlaneGetHPitch = (FPAMFPLANEGETHPITCH) DYNLIB_IMPORT(module, "amfPlaneGetHPitch");
    amfPlaneGetVPitch = (FPAMFPLANEGETVPITCH) DYNLIB_IMPORT(module, "amfPlaneGetVPitch");

    amfCreateComponent = (FPAMFCREATECOMPONENT) DYNLIB_IMPORT(module, "amfCreateComponent");
    amfComponentInit = (FPAMFCOMPONENTINIT) DYNLIB_IMPORT(module, "amfComponentInit");
    amfComponentReInit = (FPAMFCOMPONENTREINIT) DYNLIB_IMPORT(module, "amfComponentReInit");
    amfComponentTerminate = (FPAMFCOMPONENTTERMINATE) DYNLIB_IMPORT(module, "amfComponentTerminate");
    amfComponentDrain = (FPAMFCOMPONENTDRAIN) DYNLIB_IMPORT(module, "amfComponentDrain");
    amfComponentFlush = (FPAMFCOMPONENTFLUSH) DYNLIB_IMPORT(module, "amfComponentFlush");
    amfComponentSubmitInput = (FPAMFCOMPONENTSUBMITINPUT) DYNLIB_IMPORT(module, "amfComponentSubmitInput");
    amfComponentQueryOutput = (FPAMFCOMPONENTQUERYOUTPUT) DYNLIB_IMPORT(module, "amfComponentQueryOutput");

    amfSetPropertyBool = (FPAMFSETPROPERTYBOOL) DYNLIB_IMPORT(module, "amfSetPropertyBool");
    amfSetPropertyInt64 = (FPAMFSETPROPERTYINT64) DYNLIB_IMPORT(module, "amfSetPropertyInt64");
    amfSetPropertyDouble = (FPAMFSETPROPERTYDOUBLE) DYNLIB_IMPORT(module, "amfSetPropertyDouble");
    amfSetPropertyString = (FPAMFSETPROPERTYSTRING) DYNLIB_IMPORT(module, "amfSetPropertyString");
    amfSetPropertyWString = (FPAMFSETPROPERTYWSTRING) DYNLIB_IMPORT(module, "amfSetPropertyWString");
    //amfSetPropertyInterface = (PFNAMFSETPROPERTYINTERFACE) DYNLIB_IMPORT(module, "amfSetPropertyInterface")
    amfSetPropertyRect = (FPAMFSETPROPERTYRECT) DYNLIB_IMPORT(module, "amfSetPropertyRect");
    amfSetPropertySize = (FPAMFSETPROPERTYSIZE) DYNLIB_IMPORT(module, "amfSetPropertySize");
    amfSetPropertyPoint = (FPAMFSETPROPERTYPOINT) DYNLIB_IMPORT(module, "amfSetPropertyPoint");
    amfSetPropertyRate = (FPAMFSETPROPERTYRATE) DYNLIB_IMPORT(module, "amfSetPropertyRate");
    amfSetPropertyRatio = (FPAMFSETPROPERTYRATIO) DYNLIB_IMPORT(module, "amfSetPropertyRatio");
    amfSetPropertyColor = (FPAMFSETPROPERTYCOLOR) DYNLIB_IMPORT(module, "amfSetPropertyColor");

    amfGetPropertyBool = (FPAMFGETPROPERTYBOOL)DYNLIB_IMPORT(module, "amfGetPropertyBool");
    amfGetPropertyInt64 = (FPAMFGETPROPERTYINT64)DYNLIB_IMPORT(module, "amfGetPropertyInt64");
    amfGetPropertyDouble = (FPAMFGETPROPERTYDOUBLE)DYNLIB_IMPORT(module, "amfGetPropertyDouble");
    amfGetPropertyString = (FPAMFGETPROPERTYSTRING)DYNLIB_IMPORT(module, "amfGetPropertyString");
    amfGetPropertyWString = (FPAMFGETPROPERTYWSTRING)DYNLIB_IMPORT(module, "amfGetPropertyWString");
    //amfGetPropertyInterface = (PFNAMFGETPROPERTYINTERFACE) DYNLIB_IMPORT(module, "amfGetPropertyInterface")
    amfGetPropertyRect = (FPAMFGETPROPERTYRECT)DYNLIB_IMPORT(module, "amfGetPropertyRect");
    amfGetPropertySize = (FPAMFGETPROPERTYSIZE)DYNLIB_IMPORT(module, "amfGetPropertySize");
    amfGetPropertyPoint = (FPAMFGETPROPERTYPOINT)DYNLIB_IMPORT(module, "amfGetPropertyPoint");
    amfGetPropertyRate = (FPAMFGETPROPERTYRATE)DYNLIB_IMPORT(module, "amfGetPropertyRate");
    amfGetPropertyRatio = (FPAMFGETPROPERTYRATIO)DYNLIB_IMPORT(module, "amfGetPropertyRatio");
    amfGetPropertyColor = (FPAMFGETPROPERTYCOLOR)DYNLIB_IMPORT(module, "amfGetPropertyColor");

    amfInitEncoder = (FPAMFINITENCODER) DYNLIB_IMPORT(module, "amfInitEncoder");
    amfComponentGetExtraData = (FPAMFCOMPONENTGETEXTRADATA) DYNLIB_IMPORT(module, "amfComponentGetExtraData");
    amfCopyYUV420HostToNV12DX9 = (FPAMFCOPYYUV420HOSTTONV12DX9) DYNLIB_IMPORT(module, "amfCopyYUV420HostToNV12DX9");

    return AMF_OK;
}


