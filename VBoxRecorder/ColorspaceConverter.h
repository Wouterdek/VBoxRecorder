#pragma once

typedef unsigned char byte;
typedef unsigned int uint;

struct BGRAPixel;
struct RGBPixel;

void BGRAtoRGBPlane(BGRAPixel* data, const uint width, const uint height, byte* destRGBBuffer);
void BGRAtoYUV444Planes(BGRAPixel* data, const uint width, const uint height, byte* destYBuffer, byte* destUBuffer, byte* destVBuffer);