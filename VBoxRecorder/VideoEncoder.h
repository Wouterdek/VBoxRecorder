#pragma once

#include <string>
extern "C"{
	#include <libavcodec\avcodec.h>
	#include <libavutil\imgutils.h>
	#include <libavformat\avformat.h>
}

//Virtualbox framebuffer is formatted in BGRA

typedef unsigned char byte;
typedef unsigned int uint;

struct BGRAPixel {
	byte blue;
	byte green;
	byte red;
	byte alpha;
};

struct RGBPixel {
	byte red;
	byte green;
	byte blue;
};

struct EncoderSettings {
	AVCodecID codecID;
	AVDictionary* codecOptions;
	AVPixelFormat pixFormat;
	AVRational fps;
	int strictStdCompliance;
	std::string format;
	
	EncoderSettings();
};

class VideoEncoder {
public:
	VideoEncoder();
	~VideoEncoder();
	void VideoEncoder::encodeHUFFYUV(std::string filePath, BGRAPixel* data, uint width, uint height);
	void setSettings(EncoderSettings settings);
	bool open(std::string filePath, uint width, uint height);
	bool recordFrame(BGRAPixel* data);
	bool close();
private:
	uint width, height, currentPts;
	AVFrame* frame;
	AVCodecContext* codecCtx;
	AVFormatContext* formatCtx;
	EncoderSettings settings;
};