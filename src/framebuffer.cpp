#include "framebuffer.h"

#include <iostream>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include "consolestyle.h"


Framebuffer::Framebuffer(uint32_t width, uint32_t height, uint32_t bitsPerPixel)
	: frameBufferDevice (0), frameBuffer(nullptr), frameBufferSize(0), bytesPerPixel(0)
{
    struct fb_var_screeninfo orig_vinfo;
    long int screensize = 0;
    
    std::cout << "Opening framebuffer /dev/fb0..." << std::endl;

    //open the framebuffer for reading/writing
    frameBufferDevice = open("/dev/fb0", O_RDWR);
    if (frameBufferDevice <= 0) {
        std::cout << ConsoleStyle(ConsoleStyle::RED) << "Failed to open /dev/fb0 for reading/writing!" << ConsoleStyle() << std::endl;
		frameBufferDevice = 0;
        return;
    }

    //get current mode information
    if (ioctl(frameBufferDevice, FBIOGET_VSCREENINFO, &currentMode)) {
        std::cout << ConsoleStyle(ConsoleStyle::YELLOW) << "Failed to read variable mode information!" << ConsoleStyle() << std::endl;
    }
    else {
        std::cout << "Original mode is " << currentMode.xres << "x" << currentMode.yres << "@" << currentMode.bits_per_pixel;
        std::cout << " with virtual resolution " << currentMode.xres_virtual << "x" << currentMode.yres_virtual << "." << std::endl;
    }
    //store screen mode for restoring it
    memcpy(&oldMode, &currentMode, sizeof(fb_var_screeninfo));

    //change screen mode. check if the user passed some values
    if (width != 0) {
        currentMode.xres = width;
    }
    if (height != 0) {
        currentMode.yres = height;
    }
    if (bitsPerPixel != 0) {
        currentMode.bits_per_pixel = bitsPerPixel;
    }
    currentMode.xres_virtual = currentMode.xres;
    currentMode.yres_virtual = currentMode.yres;
    if (ioctl(frameBufferDevice, FBIOPUT_VSCREENINFO, &currentMode)) {
		std::cout << ConsoleStyle(ConsoleStyle::RED) << "Failed to set mode to " << currentMode.xres << "x" << currentMode.yres << "@" << currentMode.bits_per_pixel << "!" << ConsoleStyle() << std::endl;
    }
    
    //get fixed screen information
    if (ioctl(frameBufferDevice, FBIOGET_FSCREENINFO, &fixedMode)) {
        std::cout << ConsoleStyle(ConsoleStyle::YELLOW) << "Failed to read fixed mode information!" << ConsoleStyle() << std::endl;
    }

    //map framebuffer into user memory
    bytesPerPixel = (currentMode.bits_per_pixel / 8);
    frameBufferSize = currentMode.yres * fixedMode.line_length;
    frameBuffer = (unsigned char *)mmap(nullptr, frameBufferSize, PROT_READ | PROT_WRITE, MAP_SHARED, frameBufferDevice, 0);
    if (frameBuffer == MAP_FAILED) {
		std::cout << ConsoleStyle(ConsoleStyle::RED) << "Failed to map framebuffer to user memory!" << ConsoleStyle() << std::endl;
        destroy();
        return;
    }
    
	std::cout << ConsoleStyle(ConsoleStyle::GREEN) << "Opened a " << currentMode.xres << "x" << currentMode.yres << "@" << currentMode.bits_per_pixel << " display";
    std::cout << " with virtual resolution " << currentMode.xres_virtual << "x" << currentMode.yres_virtual << "." << ConsoleStyle() << std::endl;

    //draw blue debug rectangle
    /*for (int y = 0; y < currentMode.yres; ++y) {
        for (int x = 0; x < currentMode.xres; ++x) {
            ((unsigned int *)frameBuffer)[y * currentMode.width + x] = 0xFF1199DD;
        }
    }*/
}

bool Framebuffer::isAvailable() const
{
	return (frameBuffer != nullptr && frameBufferDevice != 0);
}

uint32_t Framebuffer::getWidth() const
{
    return currentMode.xres;
}

uint32_t Framebuffer::getHeight() const
{
    return currentMode.yres;
}

uint32_t Framebuffer::getBitsPerPixel() const
{
    return currentMode.bits_per_pixel;
}

void Framebuffer::drawBuffer(uint32_t x, uint32_t y, const unsigned char * data, uint32_t width, uint32_t height, uint32_t bpp)
{
    //std::cout << "Frame " << width << "x" << height << "@" << bpp << std::endl;
    const uint32_t srcLineLength = width * (bpp / 8);
    //std::cout << "Bpp " << currentMode.bits_per_pixel << ", Bytespp " << bytesPerPixel << ", Bytespsl " << bytesPerScanline << std::endl;
    uint8_t * dest = frameBuffer + (y + currentMode.yoffset) * fixedMode.line_length + (x + currentMode.xoffset) * bytesPerPixel;
    if (bpp == currentMode.bits_per_pixel) {
        for (int line = 0; line < height; ++line) {
            memcpy(dest, data, srcLineLength);
            dest += fixedMode.line_length;
            data += srcLineLength;
        }
    }
    else if (currentMode.bits_per_pixel == 32) {
        if (bpp == 8) {
            for (int line = 0; line < height; ++line) {
                uint32_t * destLine = (uint32_t *)dest;
                const unsigned char * srcLine = data;
                for (int pixel = 0; pixel < width; ++pixel, destLine++, srcLine++) {
                    *destLine = srcLine[0] << 24 | srcLine[0] << 16 | srcLine[0] << 8 | 0xff; //rgba
                }
                dest += fixedMode.line_length;
                data += srcLineLength;
            }
        }
        else if (bpp == 24) {
            for (int line = 0; line < height; ++line) {
                uint32_t * destLine = (uint32_t *)dest;
                const unsigned char * srcLine = data;
                for (int pixel = 0; pixel < width; ++pixel, destLine++, srcLine += 3) {
                    *destLine = srcLine[3] << 24 | srcLine[2] << 16 | srcLine[1] << 8 | 0xff; //rgba
                }
                dest += fixedMode.line_length;
                data += srcLineLength;
            }
        }
    }
    else if (currentMode.bits_per_pixel == 24) {
        if (bpp == 8) {
            for (int line = 0; line < height; ++line) {
                unsigned char * destLine = dest;
                const unsigned char * srcLine = data;
                for (int pixel = 0; pixel < width; ++pixel, destLine+=3, srcLine++) {
                    destLine[0] = *srcLine;
                    destLine[1] = *srcLine;
                    destLine[2] = *srcLine;
                }
                dest += fixedMode.line_length;
                data += srcLineLength;
            }
        }
        else if (bpp == 32) {
            for (int line = 0; line < height; ++line) {
                unsigned char * destLine = dest;
                const uint32_t * srcLine = (uint32_t *)data;
                for (int pixel = 0; pixel < width; ++pixel, destLine+=3, srcLine++) {
                    const uint32_t px = *srcLine;
                    destLine[0] = px >> 24;
                    destLine[1] = px >> 16;
                    destLine[2] = px >> 8;
                }
                dest += fixedMode.line_length;
                data += srcLineLength;
            }
        }
    }
}

void Framebuffer::destroy()
{
	std::cout << "Closing framebuffer /dev/fb0..." << std::endl;
    
    if (frameBuffer != nullptr && frameBuffer != MAP_FAILED) {
        munmap(frameBuffer, frameBufferSize);
    }
    frameBuffer = nullptr;
    frameBufferSize = 0;

    if (frameBufferDevice != 0) {
        //reset old screen mode
        ioctl(frameBufferDevice, FBIOPUT_VSCREENINFO, &oldMode);
        frameBufferDevice = 0;
        //close device
        close(frameBufferDevice);
    }
}

Framebuffer::~Framebuffer()
{
    destroy();
}
