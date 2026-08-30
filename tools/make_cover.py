#!/usr/bin/env python3
"""
make_cover.py - Genera portadas .tx (RGBA8 tiled de GameCube/Wii) para el menu de
tarjetas de wii64-ps3, con recorte "cover" y gradiente de difuminado en los bordes
(efecto "perfil de WhatsApp": opaco en el centro, difuminado hacia el borde
redondeado de la tarjeta, transparente fuera de ella, sin deformar la imagen).

Uso:
    py -3 tools/make_cover.py <imagen|directorio> [--size WxH] [--radius R] [--feather F] [--out salida.tx]

Ejemplos:
    # Un solo archivo (PNG/JPG/JPEG/WEBP):
    py -3 tools/make_cover.py SuperMario64.webp --size 140x100 --out SuperMario64.tx
    # Todo un directorio (usa el nombre de archivo como nombre de salida):
    py -3 tools/make_cover.py covers/ --size 140x100 --radius 6 --feather 6

El tamanio por defecto es el de las tarjetas: 140x100 (ambos multiplos de 4 para
el tiling RGBA8). --radius controla el radio de las esquinas redondeadas de la
mascara y --feather el ancho del degradado alfa hacia los bordes.
"""

import argparse
import os
import struct
import sys

try:
    from PIL import Image
except ImportError:
    print("ERROR: se requiere Pillow. Instalalo con: py -3 -m pip install Pillow")
    sys.exit(1)


def encode_rgba8_tiled(pixels, width, height):
    """Codifica un buffer de pixeles RGBA (bytes, row-major) al formato .tx
    RGBA8 tiled de GameCube/Wii que espera Image.cpp / TextureDecoder.cs.

    Layout real por bloque 4x4 (en orden de bloques fila-mayor):
      - Sub-bloque 1 (32 bytes): [A,R] intercalados por cada texel
        (A0,R0, A1,R1, ..., A15,R15).
      - Sub-bloque 2 (32 bytes): [G,B] intercalados por cada texel
        (G0,B0, G1,B1, ..., G15,B15).
    El texel i se reconstruye como RGBA = (ar[2i+1], gb[2i], gb[2i+1], ar[2i]).
    """
    out = bytearray()

    for by in range(0, height, 4):
        for bx in range(0, width, 4):
            block = []
            for yy in range(4):
                for xx in range(4):
                    px = by + yy
                    py2 = bx + xx
                    if px >= height or py2 >= width:
                        block.append((0, 0, 0, 0))
                    else:
                        idx = (px * width + py2) * 4
                        block.append((pixels[idx], pixels[idx + 1],
                                      pixels[idx + 2], pixels[idx + 3]))
            ar = bytearray()
            gb = bytearray()
            for (r, g, b, a) in block:
                ar += bytes((a, r))
                gb += bytes((g, b))
            out += ar
            out += gb
    return bytes(out)


def make_alpha_gradient(w, h, radius, feather):
    """Devuelve una lista [w*h] con los valores alpha (0-255) que dan el efecto
    de recorte redondeado + difuminado en los bordes (WhatsApp).

    - Fuera del rectangulo redondeado (esquinas con radio `radius`): alpha 0.
    - Dentro pero a menos de `feather` px del borde: degradado lineal 0..255.
    - Suficientemente adentro: alpha 255 (opaco).
    """
    out = [0] * (w * h)
    for y in range(h):
        for x in range(w):
            # Distancia al rectangulo redondeado (firmada; >0 dentro, <0 fuera)
            cx = min(max(x, radius), w - 1 - radius) - x
            cy = min(max(y, radius), h - 1 - radius) - y
            if cx >= 0 and cy >= 0:
                # Estamos dentro del area interior de una esquina o del cuerpo
                d = min(abs(x - radius), abs(w - 1 - x - radius),
                        abs(y - radius), abs(h - 1 - y - radius))
                # Distancia al borde inferior (0 en el borde interior)
                edge = min(min(abs(x - (radius - 1)), abs(w - x - radius)),
                           min(abs(y - (radius - 1)), abs(h - y - radius)))
                edge = max(0.0, float(edge))
            else:
                # Fuera del cuerpo principal: Mediante la distancia euclidea
                # a la "esquina" redondeada mas cercana.
                d = 1e9
                for ex in (radius, w - 1 - radius):
                    for ey in (radius, h - 1 - radius):
                        dd = ((x - ex) ** 2 + (y - ey) ** 2) ** 0.5 - radius
                        if dd < d:
                            d = dd
                edge = d
                if edge <= 0:
                    out[y * w + x] = 0
                    continue

            # edge > 0 -> estamos dentro del interior redondeado
            # Difuminado lineal a lo largo de `feather` px pegados al borde
            if edge >= feather:
                a = 255
            else:
                a = int(255.0 * edge / feather)
            if a < 0:
                a = 0
            if a > 255:
                a = 255
            out[y * w + x] = a
    return out


def process_one(src, dst, size, radius, feather):
    im = Image.open(src)
    if im.mode != "RGBA":
        im = im.convert("RGBA")

    w, h = size

    # Recorte "cover": rellenar el rectangulo manteniendo el aspect ratio
    # (como encuadrar la foto de perfil en WhatsApp), sin deformar.
    iw, ih = im.size
    scale = max(w / iw, h / ih)
    nw = int(round(iw * scale))
    nh = int(round(ih * scale))
    im = im.resize((nw, nh), Image.LANCZOS)
    left = (nw - w) // 2
    top = (nh - h) // 2
    im = im.crop((left, top, left + w, top + h))

    raw = im.tobytes()  # RGBA row-major

    # Aplicar el gradiente alfa (WhatsApp) sobre cada pixel
    alphas = make_alpha_gradient(w, h, radius, feather)
    pix = bytearray(raw)
    for i in range(w * h):
        pix[i * 4 + 3] = alphas[i]

    tx = encode_rgba8_tiled(bytes(pix), w, h)
    with open(dst, "wb") as f:
        f.write(tx)
    print("OK  %-45s -> %s (%dx%d, %d bytes)" % (os.path.basename(src), dst, w, h, len(tx)))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("input", help="imagen (PNG/JPG/JPEG/WEBP) o directorio")
    ap.add_argument("--size", default="140x100", help="tamano de salida WxH (multiplos de 4)")
    ap.add_argument("--radius", type=int, default=6, help="radio de esquinas redondeadas (px)")
    ap.add_argument("--feather", type=int, default=6, help="ancho del difuminado alfa al borde (px)")
    ap.add_argument("--out", default=None, help="archivo .tx de salida (solo con un archivo)")
    args = ap.parse_args()

    w, h = args.size.lower().split("x")
    size = (int(w), int(h))
    if size[0] % 4 or size[1] % 4:
        print("ERROR: el tamanio debe ser multiplo de 4 (tiling RGBA8)")
        sys.exit(1)

    if os.path.isdir(args.input):
        os.makedirs(args.out, exist_ok=True) if args.out else None
        outdir = args.out if args.out else args.input
        ok = 0
        for fn in sorted(os.listdir(args.input)):
            base, ext = os.path.splitext(fn)
            if ext.lower() in (".png", ".jpg", ".jpeg", ".webp", ".bmp"):
                dst = os.path.join(outdir, base + ".tx")
                process_one(os.path.join(args.input, fn), dst, size, args.radius, args.feather)
                ok += 1
        if ok == 0:
            print("No se encontraron imagenes (PNG/JPG/JPEG/WEBP/BMP) en el directorio.")
            sys.exit(1)
    else:
        if not args.out:
            base, _ = os.path.splitext(args.input)
            args.out = base + ".tx"
        process_one(args.input, args.out, size, args.radius, args.feather)


if __name__ == "__main__":
    main()
