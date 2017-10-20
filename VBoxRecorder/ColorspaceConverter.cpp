#include "ColorspaceConverter.h"
#include "VideoEncoder.h"

void BGRAtoYUV444Planes(BGRAPixel* data, const uint width, const uint height, byte* destYBuffer, byte* destUBuffer, byte* destVBuffer) {
	for(uint y = 0; y < height; y++) {
		for(uint x = 0; x < width; x++) {
			const uint i = (y*width) + x;
			const BGRAPixel cur = data[i];
			destYBuffer[i] = static_cast<byte>((cur.red * 0.299) + (cur.green * 0.587) + (cur.blue * 0.114));
			destUBuffer[i] = static_cast<byte>((cur.red * -.168736) + (cur.green * -.331264) + (cur.blue * .500000) + 128);
			destVBuffer[i] = static_cast<byte>((cur.red * .500000) + (cur.green * -.418688) + (cur.blue * -.081312) + 128);
		}
	}
}

void BGRAtoRGBPlane(BGRAPixel* data, const uint width, const uint height, byte* destRGBBuffer) {
	RGBPixel* buffer = reinterpret_cast<RGBPixel*>(destRGBBuffer);
	for(uint y = 0; y < height; y++) {
		for(uint x = 0; x < width; x++) {
			const uint i = (y*width)+x;
			const BGRAPixel cur = data[i];
			buffer[i].red = cur.red;
			buffer[i].green = cur.green;
			buffer[i].blue = cur.blue;
		}
	}
}