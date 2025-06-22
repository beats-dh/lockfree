#!/usr/bin/env python3
"""
Script to create a simple ICO file from scratch for the LockFree project.
This creates a basic 32x32 icon with the project colors.
"""

import struct
import os

def create_simple_ico():
    """Create a simple 32x32 ICO file with LockFree branding colors."""
    
    # ICO file header (6 bytes)
    # Reserved (2 bytes) + Type (2 bytes) + Count (2 bytes)
    ico_header = struct.pack('<HHH', 0, 1, 1)  # Reserved=0, Type=1 (ICO), Count=1
    
    # ICO directory entry (16 bytes)
    # Width, Height, ColorCount, Reserved, Planes, BitCount, BytesInRes, ImageOffset
    width, height = 32, 32
    color_count = 0  # 0 means 256+ colors
    reserved = 0
    planes = 1
    bit_count = 32  # 32-bit RGBA
    
    # Calculate bitmap data size
    # BITMAPINFOHEADER (40 bytes) + pixel data (32*32*4 bytes)
    bitmap_size = 40 + (width * height * 4)
    image_offset = 6 + 16  # After ICO header and directory entry
    
    ico_dir_entry = struct.pack('<BBBBHHLL', 
                                width, height, color_count, reserved,
                                planes, bit_count, bitmap_size, image_offset)
    
    # BITMAPINFOHEADER (40 bytes)
    bmp_header = struct.pack('<LLLHHLLLLLL',
                            40,          # biSize
                            width,       # biWidth
                            height * 2,  # biHeight (doubled for ICO)
                            1,           # biPlanes
                            bit_count,   # biBitCount
                            0,           # biCompression (BI_RGB)
                            width * height * 4,  # biSizeImage
                            0,           # biXPelsPerMeter
                            0,           # biYPelsPerMeter
                            0,           # biClrUsed
                            0)           # biClrImportant
    
    # Create pixel data (32x32 RGBA)
    pixels = []
    center_x, center_y = width // 2, height // 2
    
    for y in range(height - 1, -1, -1):  # ICO format is bottom-up
        for x in range(width):
            # Calculate distance from center
            dx = x - center_x
            dy = y - center_y
            distance = (dx * dx + dy * dy) ** 0.5
            
            if distance <= 14:  # Main circle
                if distance <= 4:  # Center core (yellow)
                    r, g, b, a = 251, 191, 36, 255  # #fbbf24
                elif distance <= 8:  # Inner ring (blue)
                    r, g, b, a = 37, 99, 235, 255   # #2563eb
                else:  # Outer ring (darker blue)
                    r, g, b, a = 30, 64, 175, 255   # #1e40af
            else:
                # Transparent background
                r, g, b, a = 0, 0, 0, 0
            
            # Add some atomic nodes at specific positions
            if (abs(dx) == 10 and abs(dy) <= 2) or (abs(dy) == 10 and abs(dx) <= 2):
                r, g, b, a = 16, 185, 129, 255  # #10b981 (green nodes)
            
            # BGRA format for ICO
            pixels.extend([b, g, r, a])
    
    # Combine all parts
    ico_data = ico_header + ico_dir_entry + bmp_header + bytes(pixels)
    
    return ico_data

def main():
    """Create the ICO file."""
    ico_data = create_simple_ico()
    
    # Write to file
    output_path = os.path.join(os.path.dirname(__file__), 'icon.ico')
    with open(output_path, 'wb') as f:
        f.write(ico_data)
    
    print(f"ICO file created: {output_path}")
    print(f"File size: {len(ico_data)} bytes")

if __name__ == '__main__':
    main()