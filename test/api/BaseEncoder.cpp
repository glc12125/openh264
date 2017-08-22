#include "BaseEncoder.h"

#include <fstream>
#include <iostream>
#include <cassert>
#include "codec_def.h"
#include "utils/BufferedData.h"
#include "utils/FileInputStream.h"
#include "utils/HashFunctions.h"
#include <BufferInputStream.h>
#include <globals.h>


static int InitWithParam (ISVCEncoder* encoder, SEncParamExt* pEncParamExt) {

  SliceModeEnum eSliceMode = pEncParamExt->sSpatialLayers[0].sSliceArgument.uiSliceMode;
  bool bBaseParamFlag      = (SM_SINGLE_SLICE == eSliceMode             && !pEncParamExt->bEnableDenoise
                              && pEncParamExt->iSpatialLayerNum == 1     && !pEncParamExt->bIsLosslessLink
                              && !pEncParamExt->bEnableLongTermReference && !pEncParamExt->iEntropyCodingModeFlag) ? true : false;
  if (bBaseParamFlag) {
    SEncParamBase param;
    memset (&param, 0, sizeof (SEncParamBase));

    param.iUsageType     = pEncParamExt->iUsageType;
    param.fMaxFrameRate  = pEncParamExt->fMaxFrameRate;
    param.iPicWidth      = pEncParamExt->iPicWidth;
    param.iPicHeight     = pEncParamExt->iPicHeight;
    param.iTargetBitrate = 5000000;
    return encoder->Initialize (&param);
  } else {
    SEncParamExt param;
    encoder->GetDefaultParams (&param);

    param.iUsageType       = pEncParamExt->iUsageType;
    param.fMaxFrameRate    = pEncParamExt->fMaxFrameRate;
    param.iPicWidth        = pEncParamExt->iPicWidth;
    param.iPicHeight       = pEncParamExt->iPicHeight;
    param.iTargetBitrate   = 5000000;
    param.bEnableDenoise   = pEncParamExt->bEnableDenoise;
    param.iSpatialLayerNum = pEncParamExt->iSpatialLayerNum;
    param.bIsLosslessLink  = pEncParamExt->bIsLosslessLink;
    param.bEnableLongTermReference = pEncParamExt->bEnableLongTermReference;
    param.iEntropyCodingModeFlag   = pEncParamExt->iEntropyCodingModeFlag ? 1 : 0;
    if (eSliceMode != SM_SINGLE_SLICE
        && eSliceMode != SM_SIZELIMITED_SLICE) //SM_SIZELIMITED_SLICE don't support multi-thread now
      param.iMultipleThreadIdc = 2;

    for (int i = 0; i < param.iSpatialLayerNum; i++) {
      param.sSpatialLayers[i].iVideoWidth     = pEncParamExt->iPicWidth  >> (param.iSpatialLayerNum - 1 - i);
      param.sSpatialLayers[i].iVideoHeight    = pEncParamExt->iPicHeight >> (param.iSpatialLayerNum - 1 - i);
      param.sSpatialLayers[i].fFrameRate      = pEncParamExt->fMaxFrameRate;
      param.sSpatialLayers[i].iSpatialBitrate = param.iTargetBitrate;

      param.sSpatialLayers[i].sSliceArgument.uiSliceMode = eSliceMode;
      if (eSliceMode == SM_SIZELIMITED_SLICE) {
        param.sSpatialLayers[i].sSliceArgument.uiSliceSizeConstraint = 600;
        param.uiMaxNalSize = 1500;
        param.iMultipleThreadIdc = 4;
        param.bUseLoadBalancing = false;
      }
      if (eSliceMode == SM_FIXEDSLCNUM_SLICE) {
        param.sSpatialLayers[i].sSliceArgument.uiSliceNum = 4;
        param.iMultipleThreadIdc = 4;
        param.bUseLoadBalancing = false;
      }
      if (param.iEntropyCodingModeFlag) {
        param.sSpatialLayers[i].uiProfileIdc = PRO_MAIN;
      }
    }
    param.iTargetBitrate *= param.iSpatialLayerNum;
    return encoder->InitializeExt (&param);
  }
}


BaseEncoder::BaseEncoder() {
  int rv = WelsCreateSVCEncoder (&encoder_);

  unsigned int uiTraceLevel = WELS_LOG_ERROR;
  encoder_->SetOption (ENCODER_OPTION_TRACE_LEVEL, &uiTraceLevel);
  SHA1Reset (&d_ctx);
}

BaseEncoder::~BaseEncoder() {
  if (encoder_) {
    encoder_->Uninitialize();
    WelsDestroySVCEncoder (encoder_);
  }
}

namespace {
  static void UpdateHashFromPlane (SHA1Context* ctx, const uint8_t* plane,
                                   int width, int height, int stride) {
    for (int i = 0; i < height; i++) {
      SHA1Input (ctx, plane, width);
      plane += stride;
    }
  }
} // End of anonymous namespace

void BaseEncoder::EncodeStream (InputStream* in, SEncParamExt* pEncParamExt, Callback* cbk) {

  assert(NULL != pEncParamExt);
  int rv = InitWithParam (encoder_, pEncParamExt);
  assert(rv == cmResultSuccess);

  // I420: 1(Y) + 1/4(U) + 1/4(V)
  int frameSize = pEncParamExt->iPicWidth * pEncParamExt->iPicHeight * 3 / 2;

  BufferedData buf;
  buf.SetLength (frameSize);
  assert(buf.Length() == (size_t)frameSize);

  SFrameBSInfo info;
  memset (&info, 0, sizeof (SFrameBSInfo));

  SSourcePicture pic;
  memset (&pic, 0, sizeof (SSourcePicture));
  pic.iPicWidth    = pEncParamExt->iPicWidth;
  pic.iPicHeight   = pEncParamExt->iPicHeight;
  pic.iColorFormat = videoFormatI420;
  pic.iStride[0]   = pic.iPicWidth;
  pic.iStride[1]   = pic.iStride[2] = pic.iPicWidth >> 1;
  pic.pData[0]     = buf.data();
  pic.pData[1]     = pic.pData[0] + pEncParamExt->iPicWidth * pEncParamExt->iPicHeight;
  pic.pData[2]     = pic.pData[1] + (pEncParamExt->iPicWidth * pEncParamExt->iPicHeight >> 2);


  int readSize = in->read (buf.data(), frameSize);
#ifdef _DEBUG_MODE_
  std::cout << "readSize: " << readSize << "\n";
#endif
  while (readSize == frameSize) {
    rv = encoder_->EncodeFrame (&pic, &info);
    assert(rv == cmResultSuccess);
    if (info.eFrameType != videoFrameTypeSkip && cbk != NULL) {
      SHA1Reset (&d_ctx);
      cbk->onEncodeFrame (info);
      //// calculate hash and print
      //SHA1Input (&d_ctx, pic.pData[0], frameSize);
      //unsigned char digest[SHA_DIGEST_LENGTH];
      //SHA1Result (&d_ctx, digest);
      //char hashStrCmp[SHA_DIGEST_LENGTH * 2 + 1];
      //ToHashStr (hashStrCmp, digest, SHA_DIGEST_LENGTH);
      //std::cout << "Hash of the frame: " << hashStrCmp << "\n";
    }
    readSize = in->read (buf.data(), frameSize);
  }
}

void BaseEncoder::EncodeBuffer(const uint8_t* buffer, int size, SEncParamExt* pEncParamExt, Callback* cbk)
{
  BufferInputStream bufferStream(buffer, size);
  assert(NULL != pEncParamExt);
  EncodeStream(&bufferStream, pEncParamExt, cbk);
}

void BaseEncoder::EncodeFile(const char* fileName, SEncParamExt* pEncParamExt, Callback* cbk) {
  FileInputStream fileStream;
  bool isOpened = fileStream.Open(fileName);
  assert(isOpened);
  assert(NULL != pEncParamExt);
  EncodeStream(&fileStream, pEncParamExt, cbk);
  cbk->finishedEncoding();
}
