#pragma once

typedef unsigned char byte;
typedef unsigned int uint;

struct BGRAPixel;
struct RGBPixel;

void BGRAtoRGBPlane(BGRAPixel* data, uint width, uint height, byte* destRGBBuffer);
void BGRAtoYUV444Planes(BGRAPixel* data, uint width, uint height, byte* destYBuffer, byte* destUBuffer, byte* destVBuffer);