#include <gtest/gtest.h>
#include "BaseEncoderTest.h"
#include "BaseDecoderTest.h"
#include <string>

void encToDecData (const SFrameBSInfo& info, int& len) {
  len = 0;
  for (int i = 0; i < info.iLayerNum; ++i) {
    const SLayerBSInfo& layerInfo = info.sLayerInfo[i];
    for (int j = 0; j < layerInfo.iNalCount; ++j) {
      len += layerInfo.pNalLengthInByte[j];
    }
  }
}

static void DecodeFromFrame (const SFrameBSInfo& info) {
  /*for (int i = 0; i < info.iLayerNum; ++i) {
    const SLayerBSInfo& layerInfo = info.sLayerInfo[i];
    int layerSize = 0;
    for (int j = 0; j < layerInfo.iNalCount; ++j) {
      layerSize += layerInfo.pNalLengthInByte[j];
    }
    SHA1Input (ctx, layerInfo.pBsBuf, layerSize);
  }*/
  int iLen = 0, rv;
  unsigned char* pData[3] = { NULL };
  SBufferInfo    dstBufInfo;
  ISVCDecoder* decoder;
  rv = WelsCreateDecoder (&decoder);
  EXPECT_EQ (0, rv);
  EXPECT_TRUE (decoder != NULL);
  //extract target layer data
  encToDecData (info, iLen);
  SDecodingParam decParam;
  memset (&decParam, 0, sizeof (SDecodingParam));
  decParam.uiTargetDqLayer = UCHAR_MAX;
  decParam.eEcActiveIdc = ERROR_CON_SLICE_COPY;
  decParam.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_DEFAULT;

  rv = decoder->Initialize (&decParam);
  EXPECT_EQ (0, rv);
  rv = decoder->DecodeFrameNoDelay (info.sLayerInfo[0].pBsBuf, iLen, pData, &dstBufInfo);
  EXPECT_TRUE (rv == cmResultSuccess) << " rv = " << rv;
  if (decoder != NULL) {
    decoder->Uninitialize();
    WelsDestroyDecoder (decoder);
  }
}

class EncoderInitTest : public ::testing::Test, public BaseEncoderTest, public BaseDecoderTest {
 public:
  virtual void SetUp() {
    BaseEncoderTest::SetUp();
    BaseDecoderTest::SetUp();
  }
  virtual void TearDown() {
    BaseEncoderTest::TearDown();
    BaseDecoderTest::TearDown();
  }
};

TEST_F (EncoderInitTest, JustInit) {}

struct EncodeFileParam {
  const char* pkcFileName;
  const char* pkcHashStr[2];
  EUsageType eUsageType;
  int iWidth;
  int iHeight;
  float fFrameRate;
  SliceModeEnum eSliceMode;
  bool bDenoise;
  int  iLayerNum;
  bool bLossless;
  bool bEnableLtr;
  bool bCabac;
// unsigned short iMultipleThreadIdc;
};

void EncFileParamToParamExt (EncodeFileParam* pEncFileParam, SEncParamExt* pEnxParamExt) {
  ASSERT_TRUE (NULL != pEncFileParam && NULL != pEnxParamExt);
  pEnxParamExt->iUsageType       = pEncFileParam->eUsageType;
  pEnxParamExt->iPicWidth        = pEncFileParam->iWidth;
  pEnxParamExt->iPicHeight       = pEncFileParam->iHeight;
  pEnxParamExt->fMaxFrameRate    = pEncFileParam->fFrameRate;
  pEnxParamExt->iSpatialLayerNum = pEncFileParam->iLayerNum;

  pEnxParamExt->bEnableDenoise   = pEncFileParam->bDenoise;
  pEnxParamExt->bIsLosslessLink  = pEncFileParam->bLossless;
  pEnxParamExt->bEnableLongTermReference = pEncFileParam->bEnableLtr;
  pEnxParamExt->iEntropyCodingModeFlag   = pEncFileParam->bCabac ? 1 : 0;

  for (int i = 0; i < pEnxParamExt->iSpatialLayerNum; i++) {
    pEnxParamExt->sSpatialLayers[i].sSliceArgument.uiSliceMode = pEncFileParam->eSliceMode;
  }

}

class ImageEncoderTest : public ::testing::WithParamInterface<EncodeFileParam>,
  public EncoderInitTest , public BaseEncoderTest::Callback {
 public:
  virtual void SetUp() {
    EncoderInitTest::SetUp();
    if (HasFatalFailure()) {
      return;
    }
  }
  virtual void onEncodeFrame (const SFrameBSInfo& frameInfo) {
    DecodeFromFrame (frameInfo);
  }
};


TEST_P (ImageEncoderTest, CompareOutput) {
  EncodeFileParam p = GetParam();
  SEncParamExt EnxParamExt;

  EncFileParamToParamExt (&p, &EnxParamExt);
  EncodeFile (p.pkcFileName, &EnxParamExt, this);
  //will remove this after screen content algorithms are ready,
  //because the bitstream output will vary when the different algorithms are added.
  /*unsigned char digest[SHA_DIGEST_LENGTH];
  SHA1Result (&ctx_, digest);
  if (!HasFatalFailure()) {
    CompareHashAnyOf (digest, p.pkcHashStr, sizeof p.pkcHashStr / sizeof *p.pkcHashStr);
  }*/
}
static const EncodeFileParam kFileParamArray[] = {
  {
    "res/out.yuv",
    {"913766b932176ff14943c0581f7c363e4fcb3e7a"}, CAMERA_VIDEO_REAL_TIME, 1280, 720, 30.0f, SM_SINGLE_SLICE, false, 1, false, false, false
  },

};

INSTANTIATE_TEST_CASE_P (EncodeFile, ImageEncoderTest,
                         ::testing::ValuesIn (kFileParamArray));
