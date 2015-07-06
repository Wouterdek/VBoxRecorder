#include "ColorspaceConverter.h"
#include "VideoEncoder.h"

void BGRAtoYUV444Planes(BGRAPixel* data, uint width, uint height, byte* destYBuffer, byte* destUBuffer, byte* destVBuffer) {
	for(uint y = 0; y < height; y++) {
		for(uint x = 0; x < width; x++) {
			BGRAPixel cur = data[(y*width)+x];
			byte Y_Val = (byte)((0.299 * (double)cur.red) + (0.587 * (double)cur.green) + (0.114 * (double)cur.blue));
			byte U_Val = (byte)((cur.red * -.168736) + (cur.green * -.331264) + (cur.blue *  .500000) + 128);
			byte V_Val = (byte)((cur.red *  .500000) + (cur.green * -.418688) + (cur.blue * -.081312) + 128);
			destYBuffer[(y*width)+x] = Y_Val;
			destUBuffer[(y*width)+x] = U_Val;
			destVBuffer[(y*width)+x] = V_Val;
		}
	}
}

void BGRAtoRGBPlane(BGRAPixel* data, uint width, uint height, byte* destRGBBuffer) {
	RGBPixel* buffer = (RGBPixel*)destRGBBuffer;
	for(uint y = 0; y < height; y++) {
		for(uint x = 0; x < width; x++) {
			uint i = (y*width)+x;
			BGRAPixel cur = data[i];
			buffer[i].red = cur.red;
			buffer[i].green = cur.green;
			buffer[i].blue = cur.blue;
		}
	}
}