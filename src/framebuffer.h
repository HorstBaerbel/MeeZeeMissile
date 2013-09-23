#pragma once

#include <inttypes.h>
#include <linux/fb.h>


class Framebuffer
{
	int frameBufferDevice; //!<Framebuffer device handle.
	uint8_t * frameBuffer; //!<Pointer to memory-mapped raw framebuffer pixel data.
	uint32_t frameBufferSize; //!<Size of whole framebuffer in Bytes.
    uint32_t bytesPerPixel; //!<Bytes per pixel on screen.

	struct fb_var_screeninfo oldMode; //!<Original framebuffer mode before mode switch.
	struct fb_var_screeninfo currentMode; //!<New framebuffer mode while application is running.
	struct fb_fix_screeninfo fixedMode; //Fixed mode information for various needs.
	
	void destroy();

public:
    /*!
    Construct framebuffer interface and switch to new mode.
    \param[in] width Optional. Width of new framebuffer mode. If 0 uses current width.
    \param[in] height Optional. Height of new framebuffer mode. If 0 uses current height.
    \param[in] bitsPerPixel Optional. Bit depth of new framebuffer mode. If 0 uses current bit depth.
    */
	Framebuffer(uint32_t width = 0, uint32_t height = 0, uint32_t bitsPerPixel = 0);
	
    /*!
    Check if framebuffer interface is available.
    \return Returns true if the framebuffer interface can be used.
    */
	bool isAvailable() const;
	
    uint32_t getWidth() const;
    uint32_t getHeight() const;
    uint32_t getBitsPerPixel() const;

    /*!
    Draw raw image to framebuffer at position.
    \param[in] x Horizontal position where to draw image.
    \param[in] y Vertical position where to draw image.
    \param[in] data Pointer to raw image data.
    \param[in] width Width of image in pixels.
    \param[in] height Height of image in pixels.
    \param[in] bpp Bite per pixel the data has. Supported depths are 24/32 bits atm.
    */
    void drawBuffer(uint32_t x, uint32_t y, const unsigned char * data, uint32_t width, uint32_t height, uint32_t bpp);
	
	~Framebuffer();
};
