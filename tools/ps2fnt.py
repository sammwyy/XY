#!/usr/bin/env python3
import sys
import os
import struct
import argparse
from dataclasses import dataclass, field
from typing import List, Dict, Optional, Tuple
from PIL import Image, ImageFont, ImageDraw

# ============================================================
# CONSTANTS & STRUCTS
# ============================================================

MAGIC_FNT = b"P2FN"
VERSION_FNT = 1

MAGIC_TEX = b"P2TX"
VERSION_TEX = 1

@dataclass
class Glyph:
    codepoint: int
    page: int
    x: int
    y: int
    width: int
    height: int
    x_offset: int
    y_offset: int
    x_advance: int
    u0: float = 0.0
    v0: float = 0.0
    u1: float = 0.0
    v1: float = 0.0

@dataclass
class Kerning:
    first: int
    second: int
    amount: int

@dataclass
class Page:
    id: int
    width: int
    height: int
    name: str
    data: Optional[bytes] = None

class XYP2FFlags:
    EMBEDDED_ATLAS = 1 << 0
    EXTERNAL_ATLAS = 1 << 1
    HAS_KERNING    = 1 << 2
    MONOSPACE      = 1 << 3
    UTF8           = 1 << 4

# ============================================================
# UTILS
# ============================================================

def align_offset(offset, alignment):
    return (offset + alignment - 1) & ~(alignment - 1)

def pack_string(s):
    return s.encode('utf-8') + b'\0'

# ============================================================
# P2TX ENCODER
# ============================================================

class P2TXEncoder:
    GS_PSM_CT32 = 0x00
    GS_PSM_T8   = 0x13
    
    @staticmethod
    def rotate_clut(clut_data):
        count = len(clut_data) // 4
        new_clut = bytearray(len(clut_data))
        for i in range(count):
            block = i // 32
            offset = i % 32
            if 8 <= offset < 16: src_offset = offset + 8
            elif 16 <= offset < 24: src_offset = offset - 8
            else: src_offset = offset
            src_i = block * 32 + src_offset
            if src_i < count: new_clut[i*4 : i*4 + 4] = clut_data[src_i*4 : src_i*4 + 4]
        return bytes(new_clut)

    @staticmethod
    def encode(img: Image.Image, swizzle=False):
        img = img.convert("RGBA")
        width, height = img.size
        
        aw, ah = align_offset(width, 64), align_offset(height, 32)
        if aw != width or ah != height:
            new_img = Image.new("RGBA", (aw, ah), (0,0,0,0))
            new_img.paste(img, (0,0))
            img = new_img
            width, height = aw, ah

        indexed = img.quantize(colors=256, method=Image.Quantize.FASTOCTREE)
        pixel_indices = list(indexed.getdata())
        
        rgba_sums = [[0,0,0,0,0] for _ in range(256)]
        orig_data = list(img.getdata())
        for idx, rgba in zip(pixel_indices, orig_data):
            rgba_sums[idx][0] += rgba[0]
            rgba_sums[idx][1] += rgba[1]
            rgba_sums[idx][2] += rgba[2]
            rgba_sums[idx][3] += rgba[3]
            rgba_sums[idx][4] += 1
            
        clut_raw = bytearray()
        for i in range(256):
            r, g, b, a, count = rgba_sums[i]
            if count > 0:
                r, g, b, a = r//count, g//count, b//count, a//count
            else:
                r = g = b = a = 0
            ps2_a = (a + 1) >> 1
            clut_raw += struct.pack("BBBB", r, g, b, ps2_a)

        clut = P2TXEncoder.rotate_clut(clut_raw)
        pixels = bytes(pixel_indices)
        
        header = struct.pack(
            "<4sHHHBBBBIIIII6x",
            MAGIC_TEX, VERSION_TEX, width, height,
            P2TXEncoder.GS_PSM_T8, 1, P2TXEncoder.GS_PSM_CT32, 1,
            len(pixels), len(clut), 40, 40 + len(pixels),
            (1 << 1) | (1 << 2) # CLUT_ROTATED | HAS_ALPHA
        )
        
        return header + pixels + clut

# ============================================================
# CONVERTERS
# ============================================================

class FontConverter:
    def __init__(self):
        self.glyphs: List[Glyph] = []
        self.kernings: List[Kerning] = []
        self.pages: List[Page] = []
        self.line_height = 0
        self.base = 0
        self.ascent = 0
        self.descent = 0
        self.atlas_width = 0
        self.atlas_height = 0

    def sort_data(self):
        self.glyphs.sort(key=lambda g: g.codepoint)
        self.kernings.sort(key=lambda k: (k.first, k.second))

    def calculate_uvs(self):
        for g in self.glyphs:
            page = self.pages[g.page]
            g.u0 = g.x / page.width
            g.v0 = g.y / page.height
            g.u1 = (g.x + g.width) / page.width
            g.v1 = (g.y + g.height) / page.height

    def write_p2f(self, output_path, flags=0, embed_atlas=False):
        self.sort_data()
        self.calculate_uvs()
        header_size = 80 
        glyph_count, kerning_count, page_count = len(self.glyphs), len(self.kernings), len(self.pages)
        glyphs_offset = align_offset(header_size, 16)
        kernings_offset = align_offset(glyphs_offset + glyph_count * 40, 16)
        pages_offset = align_offset(kernings_offset + kerning_count * 10, 16)
        strings_offset = align_offset(pages_offset + page_count * 24, 16)
        string_data = b""
        page_name_offsets = []
        for p in self.pages:
            page_name_offsets.append(len(string_data))
            string_data += pack_string(p.name)

        embedded_atlas_offset, embedded_atlas_size = 0, 0
        if embed_atlas and self.pages:
            embedded_atlas_offset = align_offset(strings_offset + len(string_data), 16)
            embedded_atlas_size = len(self.pages[0].data) if self.pages[0].data else 0
            flags |= XYP2FFlags.EMBEDDED_ATLAS
        else:
            flags |= XYP2FFlags.EXTERNAL_ATLAS

        total_size = strings_offset + len(string_data)
        if embed_atlas: total_size = embedded_atlas_offset + embedded_atlas_size

        header = struct.pack(
            "<4sHH HHHH hhhh HH IIII II I 8I",
            MAGIC_FNT, VERSION_FNT, header_size,
            glyph_count, kerning_count, page_count, flags,
            self.line_height, self.base, self.ascent, self.descent,
            self.atlas_width, self.atlas_height,
            glyphs_offset, kernings_offset, pages_offset, strings_offset,
            embedded_atlas_offset, embedded_atlas_size,
            total_size, 0,0,0,0, 0,0,0,0 
        )

        with open(output_path, "wb") as f:
            f.write(header)
            f.seek(glyphs_offset)
            for g in self.glyphs:
                f.write(struct.pack("<I HHHHH hhh I ffff",
                    g.codepoint, g.page, g.x, g.y, g.width, g.height,
                    g.x_offset, g.y_offset, g.x_advance, 0,
                    g.u0, g.v0, g.u1, g.v1
                ))
            f.seek(kernings_offset)
            for k in self.kernings:
                f.write(struct.pack("<II h", k.first, k.second, k.amount))
            f.seek(pages_offset)
            for i, p in enumerate(self.pages):
                f.write(struct.pack("<HHHH IIII", p.id, p.width, p.height, 0, 0, 0, page_name_offsets[i], 0))
            f.seek(strings_offset)
            f.write(string_data)
            if embed_atlas and self.pages[0].data:
                f.seek(embedded_atlas_offset)
                f.write(self.pages[0].data)
        print(f"Created {output_path} ({total_size} bytes)")

def convert_ttf(input_path, output_path, args):
    conv = FontConverter()
    font = ImageFont.truetype(input_path, args.font_size)
    charset = [chr(i) for i in range(32, 127)]
    if args.charset == "latin1": charset = [chr(i) for i in range(32, 256)]
    elif args.charset == "custom" and args.charset_file:
        with open(args.charset_file, "r", encoding="utf-8") as f: charset = list(f.read())
    ascent, descent = font.getmetrics()
    conv.line_height, conv.base, conv.ascent, conv.descent = ascent + descent, ascent, ascent, descent
    margin, max_w = args.padding, args.page_width
    curr_x, curr_y, row_h = margin, margin, 0
    temp_glyphs = []
    for char in charset:
        bbox = font.getbbox(char)
        if not bbox: continue
        temp_glyphs.append({"cp": ord(char), "w": bbox[2]-bbox[0], "h": bbox[3]-bbox[1], "off_x": bbox[0], "off_y": bbox[1], "adv": int(font.getlength(char)), "char": char})
    final_glyphs = []
    for tg in temp_glyphs:
        if curr_x + tg["w"] + margin > max_w:
            curr_x, curr_y = margin, curr_y + row_h + margin
            row_h = 0
        final_glyphs.append(Glyph(tg["cp"], 0, curr_x, curr_y, tg["w"], tg["h"], tg["off_x"], tg["off_y"], tg["adv"]))
        curr_x += tg["w"] + margin
        row_h = max(row_h, tg["h"])
    atlas_w, atlas_h = max_w, align_offset(curr_y + row_h + margin, 16)
    conv.atlas_width, conv.atlas_height, conv.glyphs = atlas_w, atlas_h, final_glyphs
    img = Image.new("RGBA", (atlas_w, atlas_h), (0,0,0,0))
    draw = ImageDraw.Draw(img)
    for g, tg in zip(final_glyphs, temp_glyphs):
        draw.text((g.x - g.x_offset, g.y - g.y_offset), tg["char"], font=font, fill=(255,255,255,255))
    
    ext = ".p2t" if args.tex_format == "p2t" else ".png"
    atlas_name = os.path.basename(output_path).replace(".ps2fnt", ext).replace(".p2f", ext)
    if args.tex_format == "p2t":
        atlas_data = P2TXEncoder.encode(img)
        if not args.embed_atlas:
            atlas_path = os.path.join(os.path.dirname(output_path), atlas_name)
            with open(atlas_path, "wb") as f: f.write(atlas_data)
            atlas_data = None
    else:
        atlas_data = None
        atlas_path = os.path.join(os.path.dirname(output_path), atlas_name)
        img.save(atlas_path)
    conv.pages.append(Page(0, atlas_w, atlas_h, atlas_name, atlas_data))
    conv.write_p2f(output_path, embed_atlas=args.embed_atlas)

def convert_png(input_path, output_path, args):
    conv = FontConverter()
    img = Image.open(input_path).convert("RGBA")
    w, h = img.size
    cell_w, cell_h, start_cp = args.cell_width, args.cell_height, args.first_codepoint
    cols, rows = w // cell_w, h // cell_h
    conv.line_height, conv.base, conv.atlas_width, conv.atlas_height = cell_h, cell_h, w, h
    for r in range(rows):
        for c in range(cols):
            cp = start_cp + (r * cols + c)
            conv.glyphs.append(Glyph(cp, 0, c * cell_w, r * cell_h, cell_w, cell_h, 0, 0, cell_w))
    
    ext = ".p2t" if args.tex_format == "p2t" else ".png"
    atlas_name = os.path.basename(output_path).replace(".ps2fnt", ext).replace(".p2f", ext)
    if args.tex_format == "p2t":
        atlas_data = P2TXEncoder.encode(img)
        if not args.embed_atlas:
            atlas_path = os.path.join(os.path.dirname(output_path), atlas_name)
            with open(atlas_path, "wb") as f: f.write(atlas_data)
            atlas_data = None
    else:
        atlas_data = None
        img.save(os.path.join(os.path.dirname(output_path), atlas_name))
    conv.pages.append(Page(0, w, h, atlas_name, atlas_data))
    conv.write_p2f(output_path, embed_atlas=args.embed_atlas)

def main():
    parser = argparse.ArgumentParser(description="XY Font Converter for PS2")
    subparsers = parser.add_subparsers(dest="command")
    p_ttf = subparsers.add_parser("convert-ttf")
    p_ttf.add_argument("input", help="Input .ttf/.otf file")
    p_ttf.add_argument("output", help="Output .p2f/.ps2fnt file")
    p_ttf.add_argument("--size", dest="font_size", type=int, default=24)
    p_ttf.add_argument("--charset", default="ascii", choices=["ascii", "latin1", "custom"])
    p_ttf.add_argument("--charset-file", help="Path to text file with custom charset")
    p_png = subparsers.add_parser("convert-png")
    p_png.add_argument("input", help="Input .png grid file")
    p_png.add_argument("output", help="Output .p2f/.ps2fnt file")
    p_png.add_argument("--cell-width", type=int, required=True)
    p_png.add_argument("--cell-height", type=int, required=True)
    p_png.add_argument("--first-codepoint", type=int, default=32)
    for p in [p_ttf, p_png]:
        p.add_argument("--tex-format", default="p2t", choices=["png", "p2t"], help="Atlas texture format")
        p.add_argument("--embed-atlas", action="store_true")
        p.add_argument("--padding", type=int, default=2)
        p.add_argument("--page-width", type=int, default=512)
        p.add_argument("--verbose", action="store_true")
    args = parser.parse_args()
    if args.command == "convert-ttf": convert_ttf(args.input, args.output, args)
    elif args.command == "convert-png": convert_png(args.input, args.output, args)
    else: parser.print_help()

if __name__ == "__main__": main()
