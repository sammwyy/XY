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

HEADER_FMT = "<4sHHHHHBBBBIIIII6s" # 44 bytes total
HEADER_SIZE = 44
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
    # 0 is transparent, 128 is opaque.
    # We scale the 8-bit alpha (0..255) to 0..128.
    ps2_a = (a + 1) >> 1
    if ps2_a > 128: ps2_a = 128
    return struct.pack("BBBB", r, g, b, ps2_a)

def rotate_clut(clut_data):
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

def process_image(input_path, format_str, swizzle=False, scale=1.0):
    img = Image.open(input_path).convert("RGBA")
    
    if scale != 1.0:
        new_size = (int(img.width * scale), int(img.height * scale))
        img = img.resize(new_size, Image.Resampling.LANCZOS)

    orig_width, orig_height = img.size
    
    width = align(orig_width, 64)
    height = align(orig_height, 32)
    
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
        
        # Quantize to 256 colors
        indexed = img.quantize(colors=256, method=Image.Quantize.FASTOCTREE)
        
        # Extract palette with alpha
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
        pixels = bytes(pixel_indices)

    elif (format_str == "psmt4"):
        psm = GS_PSM_T4
        has_clut = 1
        clut_psm = GS_PSM_CT32
        
        # Quantize to 16 colors
        indexed = img.quantize(colors=16, method=Image.Quantize.FASTOCTREE)
        
        # Extract palette with alpha
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
        
        clut = clut_raw # No rotation needed for 16-color CLUT in many cases, but keep simple for now.
        
        # Pack 4-bit pixels (2 per byte)
        pixels_packed = bytearray()
        for i in range(0, len(pixel_indices), 2):
            low = pixel_indices[i] & 0x0F
            high = (pixel_indices[i+1] & 0x0F) if i+1 < len(pixel_indices) else 0
            pixels_packed.append((high << 4) | low)
        pixels = bytes(pixels_packed)
        
    elif (format_str == "ct32"):
        psm = GS_PSM_CT32
        raw_rgba = list(img.getdata())
        pixels = b"".join(rgba_to_ps2_rgba32(*p) for p in raw_rgba)

    return {
        "width": width,
        "height": height,
        "orig_width": orig_width,
        "orig_height": orig_height,
        "psm": psm,
        "has_clut": has_clut,
        "clut_psm": clut_psm,
        "pixels": pixels,
        "clut": clut,
        "flags": flags
    }

def main():
    parser = argparse.ArgumentParser(description="PS2 Texture Converter for Xy Engine")
    parser.add_argument("input", help="Input image (PNG, JPG, etc)")
    parser.add_argument("output", help="Output .ps2tex file")
    parser.add_argument("--format", choices=["psmt8", "psmt4", "ct32"], default="psmt8")
    parser.add_argument("--swizzle", action="store_true")
    parser.add_argument("--scale", type=float, default=1.0)
    
    args = parser.parse_args()
    res = process_image(args.input, args.format, args.swizzle, args.scale)
    
    pixels = res["pixels"]
    pixels = pixels + b"\x00" * (align(len(pixels), 128) - len(pixels))
    
    clut = res["clut"]
    if res["has_clut"]:
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
        res["orig_width"],
        res["orig_height"],
        res["psm"],
        res["has_clut"],
        res["clut_psm"],
        1, # mip count
        data_size,
        clut_size,
        data_offset,
        clut_offset,
        res["flags"],
        b"\x00" * 6
    )
    
    with open(args.output, "wb") as f:
        f.write(header)
        f.write(pixels)
        if clut:
            f.write(clut)
            
    print(f"Success: {args.input} -> {args.output}")

if __name__ == "__main__":
    main()
