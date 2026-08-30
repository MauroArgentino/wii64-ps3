using System.Drawing;
using System.Drawing.Drawing2D;

namespace Wii64TxViewer
{
    // Convierte una imagen normal (Bitmap, por ejemplo cargada de un PNG hecho en GIMP)
    // al formato de bytes crudos .tx que usa el emulador, respetando el "tiling" en
    // bloques de cada formato GX (el mismo orden que usa TextureDecoder para leerlos).
    public static class TextureEncoder
    {
        // Redimensiona/recorta una imagen de cualquier tamaño a las dimensiones exactas
        // que necesita la textura de destino.
        public static Bitmap FitToSize(Bitmap source, int targetWidth, int targetHeight, bool pixelArt, bool cropInsteadOfStretch)
        {
            var result = new Bitmap(targetWidth, targetHeight, System.Drawing.Imaging.PixelFormat.Format32bppArgb);
            using var g = Graphics.FromImage(result);
            g.InterpolationMode = pixelArt ? InterpolationMode.NearestNeighbor : InterpolationMode.HighQualityBicubic;
            g.PixelOffsetMode = PixelOffsetMode.Half;
            g.Clear(Color.Transparent);

            if (!cropInsteadOfStretch)
            {
                // Estirar la imagen completa para que ocupe exactamente el destino.
                g.DrawImage(source, new Rectangle(0, 0, targetWidth, targetHeight));
            }
            else
            {
                // Recorte centrado: escala manteniendo proporción y recorta el sobrante.
                float scale = Math.Max((float)targetWidth / source.Width, (float)targetHeight / source.Height);
                int newW = (int)(source.Width * scale);
                int newH = (int)(source.Height * scale);
                int offX = (targetWidth - newW) / 2;
                int offY = (targetHeight - newH) / 2;
                g.DrawImage(source, new Rectangle(offX, offY, newW, newH));
            }

            return result;
        }

        // Codifica un Bitmap (ya del tamaño exacto) al formato .tx indicado.
        public static byte[] Encode(Bitmap bmp, TextureFormat fmt)
        {
            int w = bmp.Width, h = bmp.Height;
            var pixels = new Color[w, h];
            for (int y = 0; y < h; y++)
                for (int x = 0; x < w; x++)
                    pixels[x, y] = bmp.GetPixel(x, y);

            using var ms = new MemoryStream();
            using var bw = new BinaryWriter(ms);

            byte Luma(Color c) => (byte)(0.299 * c.R + 0.587 * c.G + 0.114 * c.B);

            switch (fmt)
            {
                case TextureFormat.GX_I4:
                    for (int by = 0; by < h; by += 8)
                    for (int bx = 0; bx < w; bx += 8)
                    for (int y = 0; y < 8; y++)
                    for (int x = 0; x < 8; x += 2)
                    {
                        byte i0 = (byte)(Luma(GetSafe(pixels, w, h, bx + x, by + y)) >> 4);
                        byte i1 = (byte)(Luma(GetSafe(pixels, w, h, bx + x + 1, by + y)) >> 4);
                        bw.Write((byte)((i0 << 4) | i1));
                    }
                    break;

                case TextureFormat.GX_I8:
                    for (int by = 0; by < h; by += 4)
                    for (int bx = 0; bx < w; bx += 8)
                    for (int y = 0; y < 4; y++)
                    for (int x = 0; x < 8; x++)
                        bw.Write(Luma(GetSafe(pixels, w, h, bx + x, by + y)));
                    break;

                case TextureFormat.GX_IA8:
                    for (int by = 0; by < h; by += 4)
                    for (int bx = 0; bx < w; bx += 4)
                    for (int y = 0; y < 4; y++)
                    for (int x = 0; x < 4; x++)
                    {
                        var c = GetSafe(pixels, w, h, bx + x, by + y);
                        bw.Write(Luma(c));
                        bw.Write(c.A);
                    }
                    break;

                case TextureFormat.GX_RGB5A3:
                    for (int by = 0; by < h; by += 4)
                    for (int bx = 0; bx < w; bx += 4)
                    for (int y = 0; y < 4; y++)
                    for (int x = 0; x < 4; x++)
                    {
                        var c = GetSafe(pixels, w, h, bx + x, by + y);
                        ushort v = EncodeRGB5A3(c);
                        bw.Write((byte)(v >> 8));
                        bw.Write((byte)(v & 0xFF));
                    }
                    break;

                case TextureFormat.GX_RGBA8:
                    for (int by = 0; by < h; by += 4)
                    for (int bx = 0; bx < w; bx += 4)
                    {
                        var block = new Color[16];
                        int p = 0;
                        for (int y = 0; y < 4; y++)
                        for (int x = 0; x < 4; x++)
                            block[p++] = GetSafe(pixels, w, h, bx + x, by + y);

                        // Sub-bloque 1: A,R intercalados
                        foreach (var c in block) { bw.Write(c.A); bw.Write(c.R); }
                        // Sub-bloque 2: G,B intercalados
                        foreach (var c in block) { bw.Write(c.G); bw.Write(c.B); }
                    }
                    break;

                case TextureFormat.RGBA5551:
                    for (int y = 0; y < h; y++)
                    for (int x = 0; x < w; x++)
                    {
                        var c = pixels[x, y];
                        ushort v = (ushort)(((c.R >> 3) << 11) | ((c.G >> 3) << 6) | ((c.B >> 3) << 1) | (c.A >= 128 ? 1 : 0));
                        bw.Write((byte)(v >> 8));
                        bw.Write((byte)(v & 0xFF));
                    }
                    break;
            }

            bw.Flush();
            return ms.ToArray();
        }

        private static Color GetSafe(Color[,] pixels, int w, int h, int x, int y)
        {
            if (x < 0 || x >= w || y < 0 || y >= h) return Color.FromArgb(0, 0, 0, 0);
            return pixels[x, y];
        }

        // RGB5A3: si el pixel es prácticamente opaco, usar el modo RGB555 (mejor color, sin alpha real).
        // Si tiene transparencia real, usar el modo RGB444+A3 (menos color, con alpha de 3 bits).
        private static ushort EncodeRGB5A3(Color c)
        {
            if (c.A >= 240)
            {
                int r5 = c.R * 31 / 255;
                int g5 = c.G * 31 / 255;
                int b5 = c.B * 31 / 255;
                return (ushort)(0x8000 | (r5 << 10) | (g5 << 5) | b5);
            }
            else
            {
                int a3 = c.A * 7 / 255;
                int r4 = c.R * 15 / 255;
                int g4 = c.G * 15 / 255;
                int b4 = c.B * 15 / 255;
                return (ushort)((a3 << 12) | (r4 << 8) | (g4 << 4) | b4);
            }
        }
    }
}
