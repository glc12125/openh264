#include "BaseDecoder.h"

#include <istream>
#include <iostream>
#include <cassert>
#include "codec_def.h"
#include "codec_app_def.h"
#include "utils/BufferedData.h"
#include <YuvImageData.h>
#include <globals.h>

namespace {

static void ReadFrame(std::istream* stream, BufferedData* buf) {
  // get length of file:
  char b;

  buf->Clear();
  for (;;) {
    stream->read (&b, 1);
    if (stream->gcount() != 1) { // end of stream
#ifdef _DEBUG_MODE_
      std::cout << "reached the end of stream!\n";
#endif
      return;
    }
    //std::cout << "read one byte: " << (int)b << "\n";
    if (!buf->PushBack (b)) {
      std::cerr << "unable to allocate memory";
      exit(-1);
    }
  }
}

static const unsigned int RETRY_WAIT_TIME_MILI_SEC = 10;
static const char * const END_OF_STREAM_FLAG = "ENDOFTHESTREAM";
// The length of flag ENDOFTHESTREAM
static const unsigned int END_OF_STREAM_FLAG_LENGTH = 14;
} // End of anonymous namespace

BaseDecoder::BaseDecoder()
  : decoder_ (NULL), decodeStatus_ (OpenFile), imageReaderThread_(orderedImageReader_, 100), lastTimestamp_(0), imageDataFetcherRunning_(true),
    availableImageDataCount_(0)
{
  imageDataFetcher_ = std::thread(&BaseDecoder::imageDataFetcherMain, this);
}

void BaseDecoder::fisnihedReadingImageData(const RoboK::YuvImageData& imageData, bool success)
{
  ++availableImageDataCount_;
}

void BaseDecoder::imageDataFetcherMain()
  {
    while (imageDataFetcherRunning_)
    {
      // Will try to call dequeueImageData
      try
      {
        if (availableImageDataCount_.load() > 0)
        {
          auto imageDataPtr = orderedImageReader_.dequeueImageData(lastTimestamp_);
          lastTimestamp_ = imageDataPtr->getKey();
          auto buf = imageDataPtr->getBufferedData();
          if(buf->Length() != END_OF_STREAM_FLAG_LENGTH)
          {
            // decode first
            DecodeFrame (buf->data(), buf->Length(), imageDataPtr->decoderCallback());
            // then save or feed to SLAM
          }
          else if (buf->Length() == END_OF_STREAM_FLAG_LENGTH &&
                   std::string((const char *)buf->data(), END_OF_STREAM_FLAG_LENGTH) == END_OF_STREAM_FLAG)
          {
            std::cout << "[END] The end of stream\n";
            int32_t iEndOfStreamFlag = 1;
            decoder_->SetOption (DECODER_OPTION_END_OF_STREAM, &iEndOfStreamFlag);
            // invoke call
            imageDataPtr->decoderCallback()->onFinishedDecodingStream(true);
          }
        }
        else
        {
          std::this_thread::sleep_for(
              std::chrono::milliseconds(RETRY_WAIT_TIME_MILI_SEC));
        }
      }
      catch (const std::runtime_error &e)
      {
        //std::cerr << "[ERROR]: " << e.what() << "\n Will retry getting image data\n";
        std::this_thread::sleep_for(
            std::chrono::milliseconds(RETRY_WAIT_TIME_MILI_SEC));
      }
    }
  }

int32_t BaseDecoder::SetUp() {
  long rv = WelsCreateDecoder (&decoder_);
  if (decoder_ == NULL) {
    return rv;
  }

  SDecodingParam decParam;
  memset (&decParam, 0, sizeof (SDecodingParam));
  decParam.uiTargetDqLayer = UCHAR_MAX;
  decParam.eEcActiveIdc = ERROR_CON_SLICE_COPY;
  decParam.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_DEFAULT;

  rv = decoder_->Initialize (&decParam);
  return (int32_t)rv;
}

void BaseDecoder::TearDown() {
  if (decoder_ != NULL) {
    decoder_->Uninitialize();
    WelsDestroyDecoder (decoder_);
  }
}


void BaseDecoder::DecodeFrame (const uint8_t* src, size_t sliceSize, DecoderListener::Callback* cbk) {
  uint8_t* data[3];
  SBufferInfo bufInfo;
  memset (data, 0, sizeof (data));
  memset (&bufInfo, 0, sizeof (SBufferInfo));

  DECODING_STATE rv = decoder_->DecodeFrameNoDelay (src, (int) sliceSize, data, &bufInfo);
  assert (rv == dsErrorFree);
#ifdef _DEBUG_MODE_
  std::cout << "finished decoding, bufInfo.iBufferStatus: " << (int)bufInfo.iBufferStatus << ", cbk: " << cbk << "\n";
#endif
  if (bufInfo.iBufferStatus == 1 && cbk != NULL) {
#ifdef _DEBUG_MODE_
      std::cout << "will invoke callback\n";
#endif
    const Frame frame = {
      {
        // y plane
        data[0],
        bufInfo.UsrData.sSystemBuffer.iWidth,
        bufInfo.UsrData.sSystemBuffer.iHeight,
        bufInfo.UsrData.sSystemBuffer.iStride[0]
      },
      {
        // u plane
        data[1],
        bufInfo.UsrData.sSystemBuffer.iWidth / 2,
        bufInfo.UsrData.sSystemBuffer.iHeight / 2,
        bufInfo.UsrData.sSystemBuffer.iStride[1]
      },
      {
        // v plane
        data[2],
        bufInfo.UsrData.sSystemBuffer.iWidth / 2,
        bufInfo.UsrData.sSystemBuffer.iHeight / 2,
        bufInfo.UsrData.sSystemBuffer.iStride[1]
      },
    };
    cbk->onDecodeFrame (data, &bufInfo, lastTimestamp_);
  }
}

namespace {

// This function will remove first Timestamp_Length bytes as timestamp from the BufferedData
static unsigned long getTimeStamp(BufferedData * bufPtr)
{
  uint8_t timestamp[Timestamp_Length] = {0};
  bufPtr->PopFront(timestamp, Timestamp_Length);
  std::string timestampStr((const char *)timestamp, Timestamp_Length); // need to init by '0'
  timestampStr.erase(
      0, timestampStr.find_first_not_of('0')); // remove preceeding '0's
#ifdef _DEBUG_MODE_
  std::cout << "Timestamp: " << timestampStr << "\n";
#endif
  return std::stoul(timestampStr);
}

} // End of anonymous namespace

void BaseDecoder::DecodeStream (std::istream& stream, DecoderListener::Callback* cbk) {

  std::unique_ptr<BufferedData> bufferedDataPtr(new BufferedData());
  BufferedData * bufPtr = bufferedDataPtr.get();
  ReadFrame (&stream, bufPtr);
  if (bufPtr->Length() == 0) {
    return;
  }
  bufPtr->PopBack(16); // Remove the last @ENDOFTHEBUFFER@

  std::unique_ptr<RoboK::YuvImageData> imageData(
      new RoboK::YuvImageData(getTimeStamp(bufPtr), false, std::move(bufferedDataPtr), cbk)
  );
  // Will enque the decoding into a timestamp ordered data structure with no delay
  //DecodeFrame (buf.data(), buf.Length(), cbk);
  orderedImageReader_.queueRequest(std::move(imageData), *this);

}
