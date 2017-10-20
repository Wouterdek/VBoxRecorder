#include "VideoEncoder.h"

#include <stdint.h>
#include <Windows.h>
extern "C"{
	#include <libavcodec\avcodec.h>
	#include <libavutil\imgutils.h>
	#include <libavformat\avformat.h>
}
#include "ColorspaceConverter.h"

//Default EncoderSettings configuration
EncoderSettings::EncoderSettings() : codecID(), codecOptions(nullptr), pixFormat(), fps({1, 25}), strictStdCompliance(FF_COMPLIANCE_NORMAL) {}

VideoEncoder::VideoEncoder() : width(0), height(0), currentPts(0), frame(nullptr), codecCtx(nullptr), formatCtx(nullptr), settings(){}

VideoEncoder::~VideoEncoder() {}

void VideoEncoder::setSettings(const EncoderSettings& settings) {
	this->settings = settings;
}

bool VideoEncoder::open(const std::string& filePath, const uint width, const uint height) {
	this->width = width;
	this->height = height;

	avcodec_register_all();
	av_register_all();

	//Create encoder objects
	AVCodec* codec = avcodec_find_encoder(settings.codecID);

	if(codec == nullptr) {
		printf("Failed to retrieve codec with ID %d\n", settings.codecID);
		return false;
	}

	//Set encoder settings
	codecCtx = avcodec_alloc_context3(codec);
	if(codecCtx == nullptr) {
		printf("Failed to allocate context for codec\n");
		return false;
	}

	codecCtx->bit_rate = 400000;
	codecCtx->width = width;
	codecCtx->height = height;
	/* frames per second */
	codecCtx->time_base = settings.fps;
	codecCtx->gop_size = 10; /* emit one intra frame every ten frames */
	codecCtx->max_b_frames = 1;
	codecCtx->pix_fmt = settings.pixFormat;
	
	codecCtx->strict_std_compliance = settings.strictStdCompliance;

	if(avcodec_open2(codecCtx, codec, &settings.codecOptions) < 0) {
		printf("Error opening codec\n");
		return false;
	}

	formatCtx = avformat_alloc_context();
	formatCtx->oformat = av_guess_format(settings.format.c_str(), nullptr, nullptr);
	if(formatCtx->oformat == nullptr) {
		printf("Error finding format\n");
		return false;
	}
	AVStream* out = avformat_new_stream(formatCtx, codec);
	if(out == nullptr) {
		printf("Error creating stream\n");
		return false;
	}

	out->time_base = codecCtx->time_base;
	int ret = avcodec_parameters_from_context(out->codecpar, codecCtx);
	if (ret != 0)
	{
		printf("Error setting stream codec parameters\n");
		return false;
	}
	//formatCtx->streams[0]->codec = codecCtx;

	ret = avio_open(&formatCtx->pb, filePath.c_str(), AVIO_FLAG_WRITE);
	if(ret < 0) {
		char buf[AV_ERROR_MAX_STRING_SIZE] = { 0 };
		av_strerror(ret, buf, sizeof(buf));
		printf("Could not open video file: %d (%s)\n", ret, buf);
		return false;
	}

	ret = avformat_write_header(formatCtx, nullptr);
	if(ret != 0) {
		char buf[AV_ERROR_MAX_STRING_SIZE] = { 0 };
		av_strerror(ret, buf, sizeof(buf));
		printf("Could not write headers: %d (%s)\n", ret, buf);
		return false;
	}

	frame = av_frame_alloc();
	if(frame == nullptr) {
		printf("Error allocating frame\n");
		return false;
	}
	frame->format = codecCtx->pix_fmt;
	frame->width = codecCtx->width;
	frame->height = codecCtx->height;

	ret = av_image_alloc(frame->data, frame->linesize, codecCtx->width, codecCtx->height, codecCtx->pix_fmt, 32);
	if(ret < 0) {
		printf("Could not allocate raw picture buffer: error %d\n", ret);
		return false;
	}
	return true;
}

bool VideoEncoder::recordFrame(BGRAPixel* data) {
	if(settings.pixFormat == AV_PIX_FMT_RGB24) {
		BGRAtoRGBPlane(data, width, height, frame->data[0]);
	} else if(settings.pixFormat == AV_PIX_FMT_YUV444P) {
		BGRAtoYUV444Planes(data, width, height, frame->data[0], frame->data[1], frame->data[2]);
	} else if(settings.pixFormat == AV_PIX_FMT_BGR0) {
		memcpy(frame->data[0], data, sizeof(BGRAPixel) * width * height);
	} else {
		printf("Error: unsupported pixel format!\n");
		return false;
	}

	frame->pts = currentPts;
	
	currentPts++;

	//encode the image
	int ret = avcodec_send_frame(codecCtx, frame);

	AVPacket pkt;
	while(true)
	{
		av_init_packet(&pkt);
		pkt.data = nullptr;    // packet data will be allocated by the encoder
		pkt.size = 0;

		ret = avcodec_receive_packet(codecCtx, &pkt);
		if(ret == AVERROR(EAGAIN))
		{
			break;
		}else if(ret < 0)
		{
			printf("Error encoding frame\n");
			return false;
		}

		if (av_write_frame(formatCtx, &pkt) < 0) {
			printf("Error writing frame\n");
			return false;
		}
		av_packet_unref(&pkt);
	}
	

	return true;
}

bool VideoEncoder::close() {
	//flush buffer
	int ret = avcodec_send_frame(codecCtx, nullptr);
	if (ret < 0) {
		printf("Error encoding frame\n");
		return false;
	}

	AVPacket pkt;
	while(true) {
		av_init_packet(&pkt);
		pkt.data = nullptr;
		pkt.size = 0;

		ret = avcodec_receive_packet(codecCtx, &pkt);

		if(ret < 0 && ret != AVERROR_EOF)
		{
			printf("Error encoding frame\n");
			return false;
		}
		if(ret == AVERROR_EOF)
		{
			break;
		}

		if (av_write_frame(formatCtx, &pkt) < 0) {
			printf("Error writing frame\n");
			return false;
		}
		av_packet_unref(&pkt);
	}

	//write file ending and close
	int result = av_write_trailer(formatCtx);
	if(result != 0) {
		char buf[AV_ERROR_MAX_STRING_SIZE] = {0};
		av_strerror(result, buf, sizeof(buf));
		printf("Error writing trailer: %d (%s)\n", result, buf);
	}
	result = avio_close(formatCtx->pb);
	if(result != 0) {
		char buf[AV_ERROR_MAX_STRING_SIZE] = { 0 };
		av_strerror(result, buf, sizeof(buf));
		printf("Error closing video: %d (%s)\n", result, buf);
	}

	//free resources
	avcodec_close(codecCtx);
	avcodec_free_context(&codecCtx);
	av_freep(&frame->data[0]);
	av_frame_free(&frame);
	av_free(formatCtx);
	return true;
}