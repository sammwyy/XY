import subprocess
import struct
import sys
import argparse
from pathlib import Path
from PIL import Image

# GS Constants
GS_PSM_CT32 = 0x00
GS_PSM_CT24 = 0x01
GS_PSM_CT16 = 0x02
GS_PSM_T8   = 0x13
GS_PSM_T4   = 0x14

HEADER_FMT = "<4sHHHBBBBIIIII6x" # 40 bytes total
HEADER_SIZE = 40
MAGIC = b"P2TX"
VERSION = 1

# Flags
P2TX_SWIZZLED      = 1 << 0
P2TX_CLUT_ROTATED  = 1 << 1
P2TX_HAS_ALPHA     = 1 << 2

def align(size, alignment):
    return (size + alignment - 1) & ~(alignment - 1)

def rgba_to_ps2_rgba32(r, g, b, a):
    # PS2 GS expects Alpha in range 0..128 for blending.
    # We scale the 8-bit alpha (0..255) to 0..128.
    ps2_a = (a + 1) >> 1
    # Standard PS2 byte order for CT32 is RGBA (R at byte 0).
    return struct.pack("BBBB", r, g, b, ps2_a)

def rgba_to_ps2_rgba16(r, g, b, a):
    # 1:5:5:5 format
    val = ((r >> 3) & 0x1F) | (((g >> 3) & 0x1F) << 5) | (((b >> 3) & 0x1F) << 10) | ((1 if a > 128 else 0) << 15)
    return struct.pack("<H", val)

def rotate_clut(clut_data):
    """
    PS2 hardware requires CLUT entries for T8 textures to be rearranged.
    The GS reads palette indices in a 'swizzled' order:
    Colors 8-15 and 16-23 are swapped in each 32-color block.
    """
    count = len(clut_data) // 4
    new_clut = bytearray(len(clut_data))
    for i in range(count):
        block = i // 32
        offset = i % 32
        
        if 8 <= offset < 16:
            src_offset = offset + 8
        elif 16 <= offset < 24:
            src_offset = offset - 8
        else:
            src_offset = offset
            
        src_i = block * 32 + src_offset
        if src_i < count:
            new_clut[i*4 : i*4 + 4] = clut_data[src_i*4 : src_i*4 + 4]
        else:
            new_clut[i*4 : i*4 + 4] = b"\x00\x00\x00\x00"
            
    return bytes(new_clut)

def swizzle_psmt8(width, height, pixels):
    """
    PSMT8 Swizzle logic.
    PSMT8 Page: 128x64 pixels (8KB).
    PSMT8 Block: 16x16 pixels (256 bytes).
    """
    swizzled = bytearray(len(pixels))
    for y in range(height):
        for x in range(width):
            # Page layout (128x64 pixels for 8bpp)
            page_x = x // 128
            page_y = y // 64
            pages_per_row = width // 128
            page_idx = page_y * pages_per_row + page_x
            
            # Local in page
            lx = x % 128
            ly = y % 64
            
            # Block in page (16x16 pixels)
            # A page (128x64) is 8x4 blocks.
            bx = lx // 16
            by = ly // 16
            
            # Block arrangement in 128x64 page (8x4 grid):
            # 0  1  4  5  16 17 20 21
            # 2  3  6  7  18 19 22 23
            # 8  9  12 13 24 25 28 29
            # 10 11 14 15 26 27 30 31
            block_idx = (bx // 4) * 16 + (by // 2) * 8 + ((bx % 4) // 2) * 4 + (by % 2) * 2 + (bx % 2)
            
            # Column in block (8x2 pixels = 16 bytes)
            blx = lx % 16
            bly = ly % 16
            column_idx = (bly // 2) * 2 + (blx // 8)
            
            # Pixel in column
            pixel_idx = (bly % 2) * 8 + (blx % 8)
            
            addr = page_idx * 8192 + block_idx * 256 + column_idx * 16 + pixel_idx
            if addr < len(swizzled):
                swizzled[addr] = pixels[y * width + x]
    return bytes(swizzled)

def swizzle_psmt4(width, height, pixels):
    """
    PSMT4 Swizzle logic.
    PSMT4 Page: 128x128 pixels (8KB).
    PSMT4 Block: 32x16 pixels (256 bytes).
    """
    swizzled_packed = bytearray(len(pixels) // 2)
    for y in range(height):
        for x in range(width):
            # Page layout (128x128 pixels for 4bpp)
            page_x = x // 128
            page_y = y // 128
            pages_per_row = width // 128
            page_idx = page_y * pages_per_row + page_x
            
            # Local in page
            lx = x % 128
            ly = y % 128
            
            # Block in page (32x16 pixels)
            # A page (128x128) is 4x8 blocks.
            bx = lx // 32
            by = ly // 16
            
            # Block arrangement in 128x128 page (4x8 grid):
            block_idx = (by // 2) * 8 + (bx // 2) * 4 + (by % 2) * 2 + (bx % 2)
            
            # Column in block (16x2 pixels = 16 bytes)
            blx = lx % 32
            bly = ly % 16
            column_idx = (bly // 2) * 2 + (blx // 16)
            
            # Pixel in column (16x2)
            pixel_idx = (bly % 2) * 16 + (blx % 16)
            
            byte_addr = page_idx * 8192 + block_idx * 256 + column_idx * 16 + (pixel_idx // 2)
            
            pixel_val = pixels[y * width + x] & 0x0F
            if byte_addr < len(swizzled_packed):
                if pixel_idx % 2 == 0:
                    swizzled_packed[byte_addr] |= pixel_val
                else:
                    swizzled_packed[byte_addr] |= (pixel_val << 4)
    return bytes(swizzled_packed)

def process_image(input_path, format_str, swizzle=False):
    img = Image.open(input_path).convert("RGBA")
    width, height = img.size
    
    # PS2 GS Alignment requirements
    # PSMT8: 16x16 blocks
    # PSMT4: 32x16 blocks
    # We use 64x32 for safety and page alignment
    orig_width, orig_height = width, height
    width = align(width, 64)
    height = align(height, 32)
    
    if width != orig_width or height != orig_height:
        new_img = Image.new("RGBA", (width, height), (0, 0, 0, 0))
        new_img.paste(img, (0, 0))
        img = new_img

    pixels = b""
    clut = b""
    psm = GS_PSM_CT32
    has_clut = 0
    clut_psm = 0
    flags = 0
    
    # Check for alpha
    has_alpha = False
    for p in img.getdata():
        if p[3] < 255:
            has_alpha = True
            break
    if has_alpha:
        flags |= P2TX_HAS_ALPHA

    if (format_str == "psmt8"):
        psm = GS_PSM_T8
        has_clut = 1
        clut_psm = GS_PSM_CT32
        
        indexed = img.quantize(colors=256, method=Image.Quantize.FASTOCTREE)
        
        palette_rgba = [ [0, 0, 0, 0, 0] for _ in range(256) ]
        raw_rgba = list(img.getdata())
        pixel_indices = list(indexed.getdata())
        
        for idx, (r, g, b, a) in zip(pixel_indices, raw_rgba):
            palette_rgba[idx][0] += r
            palette_rgba[idx][1] += g
            palette_rgba[idx][2] += b
            palette_rgba[idx][3] += a
            palette_rgba[idx][4] += 1
            
        clut_raw = bytearray()
        for i in range(256):
            r, g, b, a, count = palette_rgba[i]
            if count > 0:
                r //= count
                g //= count
                b //= count
                a //= count
            else:
                r = g = b = a = 0
            clut_raw += rgba_to_ps2_rgba32(r, g, b, a)
        
        clut = rotate_clut(clut_raw)
        flags |= P2TX_CLUT_ROTATED
        
        if swizzle:
            pixels = swizzle_psmt8(width, height, pixel_indices)
            flags |= P2TX_SWIZZLED
        else:
            pixels = bytes(pixel_indices)
        
    elif format_str == "psmt4":
        psm = GS_PSM_T4
        has_clut = 1
        clut_psm = GS_PSM_CT32
        
        indexed = img.quantize(colors=16, method=Image.Quantize.FASTOCTREE)
        palette_rgba = [ [0, 0, 0, 0, 0] for _ in range(16) ]
        
        raw_rgba = list(img.getdata())
        pixel_indices = list(indexed.getdata())
        
        for idx, (r, g, b, a) in zip(pixel_indices, raw_rgba):
            palette_rgba[idx][0] += r
            palette_rgba[idx][1] += g
            palette_rgba[idx][2] += b
            palette_rgba[idx][3] += a
            palette_rgba[idx][4] += 1
            
        clut_raw = bytearray()
        for i in range(16):
            r, g, b, a, count = palette_rgba[i]
            if count > 0:
                r //= count
                g //= count
                b //= count
                a //= count
            else:
                r = g = b = a = 0
            clut_raw += rgba_to_ps2_rgba32(r, g, b, a)
        
        clut = clut_raw # PSMT4 CLUT doesn't need rotation for 16 entries
        
        if swizzle:
            pixels = swizzle_psmt4(width, height, pixel_indices)
            flags |= P2TX_SWIZZLED
        else:
            packed = bytearray()
            for i in range(0, len(pixel_indices), 2):
                p0 = pixel_indices[i] & 0x0F
                p1 = pixel_indices[i+1] & 0x0F if i+1 < len(pixel_indices) else 0
                packed.append(p0 | (p1 << 4))
            pixels = bytes(packed)
        
    elif format_str == "ct16":
        psm = GS_PSM_CT16
        raw_data = list(img.getdata())
        pixels = b"".join(rgba_to_ps2_rgba16(*p) for p in raw_data)
        
    else: # ct32
        psm = GS_PSM_CT32
        raw_data = list(img.getdata())
        pixels = b"".join(rgba_to_ps2_rgba32(*p) for p in raw_data)
        
    return {
        "width": width,
        "height": height,
        "psm": psm,
        "has_clut": has_clut,
        "clut_psm": clut_psm,
        "pixels": pixels,
        "clut": clut,
        "flags": flags
    }

def main():
    parser = argparse.ArgumentParser(description="PS2 Texture Converter for Xyon Engine")
    parser.add_argument("input", help="Input image (PNG, JPG, etc)")
    parser.add_argument("output", help="Output .ps2tex file")
    parser.add_argument("--format", choices=["psmt8", "psmt4", "ct16", "ct32"], default="psmt8")
    parser.add_argument("--swizzle", action="store_true", help="Force offline swizzling (use only if runtime upload is linear)")
    
    args = parser.parse_args()
    res = process_image(args.input, args.format, args.swizzle)
    
    pixels = res["pixels"]
    # Align pixels to 128 bytes for DMA
    pixels = pixels + b"\x00" * (align(len(pixels), 128) - len(pixels))
    
    clut = res["clut"]
    if res["has_clut"]:
        # Align CLUT to 128 bytes for DMA
        clut = clut + b"\x00" * (align(len(clut), 128) - len(clut))
    
    data_size = len(pixels)
    clut_size = len(clut)
    
    data_offset = HEADER_SIZE
    clut_offset = data_offset + data_size if res["has_clut"] else 0
    
    header = struct.pack(
        HEADER_FMT,
        MAGIC,
        VERSION,
        res["width"],
        res["height"],
        res["psm"],
        res["has_clut"],
        res["clut_psm"],
        1, # mip count
        data_size,
        clut_size,
        data_offset,
        clut_offset,
        res["flags"]
    )
    
    with open(args.output, "wb") as f:
        f.write(header)
        f.write(pixels)
        if clut:
            f.write(clut)
            
    print(f"Success: {args.input} -> {args.output}")
    print(f"  Format:   {args.format.upper()}")
    print(f"  Size:     {res['width']}x{res['height']}")
    print(f"  Flags:    0x{res['flags']:02X}")
    print(f"  VRAM:     {data_size + clut_size} bytes")

if __name__ == "__main__":
    main()
