using System.Drawing;
using System.Drawing.Imaging;

namespace Wii64TxViewer
{
    // Formatos de textura soportados.
    // Los GX_* son los nativos de GameCube/Wii (usados en los .tx del menú de Wii64).
    // RGBA5551 es el formato crudo del N64 (por si se usa para otra cosa).
    public enum TextureFormat
    {
        GX_I4,
        GX_I8,
        GX_IA8,
        GX_RGBA8,
        GX_RGB5A3,
        RGBA5551
    }

    // Definición conocida de una textura del menú: nombre, ancho, alto y formato.
    // Estos valores salen directo de GuiResources.cpp / MenuResources.h del proyecto wii64-ps3.
    public class KnownTexture
    {
        public string NameHint = "";
        public int Width;
        public int Height;
        public TextureFormat Format;

        public KnownTexture(string nameHint, int w, int h, TextureFormat fmt)
        {
            NameHint = nameHint;
            Width = w;
            Height = h;
            Format = fmt;
        }
    }

    public static class KnownTextures
    {
        // Lista basada en GuiResources.cpp (Resources::Resources()).
        public static readonly List<KnownTexture> All = new()
        {
            new KnownTexture("ButtonTexture", 16, 16, TextureFormat.GX_I8),
            new KnownTexture("ButtonFocusTexture", 16, 16, TextureFormat.GX_I8),
            new KnownTexture("StyleAButtonTexture", 8, 56, TextureFormat.GX_RGBA8),
            new KnownTexture("StyleAButtonFocusTexture", 8, 56, TextureFormat.GX_RGBA8),
            new KnownTexture("StyleAButtonSelectOffTexture", 8, 56, TextureFormat.GX_RGBA8),
            new KnownTexture("StyleAButtonSelectOffFocusTexture", 8, 56, TextureFormat.GX_RGBA8),
            new KnownTexture("StyleAButtonSelectOnTexture", 8, 56, TextureFormat.GX_RGBA8),
            new KnownTexture("StyleAButtonSelectOnFocusTexture", 8, 56, TextureFormat.GX_RGBA8),
            new KnownTexture("BackgroundTexture", 424, 240, TextureFormat.GX_I8),
            new KnownTexture("wii64", 144, 52, TextureFormat.GX_RGB5A3),   // LogoTexture (Wii)
            new KnownTexture("cube64", 192, 52, TextureFormat.GX_RGB5A3),  // LogoTexture (GameCube)
            new KnownTexture("LogoTexture", 144, 52, TextureFormat.GX_RGB5A3),
            new KnownTexture("ControlEmptyTexture", 48, 64, TextureFormat.GX_I4),
            new KnownTexture("ControlGamecubeTexture", 48, 64, TextureFormat.GX_I4),
            new KnownTexture("ControlClassicTexture", 48, 64, TextureFormat.GX_I4),
            new KnownTexture("ControlWiimoteNunchuckTexture", 48, 64, TextureFormat.GX_I4),
            new KnownTexture("ControlWiimoteTexture", 48, 64, TextureFormat.GX_I4),
            new KnownTexture("N64ControllerTexture", 208, 200, TextureFormat.GX_I4),
            new KnownTexture("CursorPoint", 40, 56, TextureFormat.GX_RGBA8),
            new KnownTexture("CursorGrab", 40, 56, TextureFormat.GX_RGBA8),
        };

        // Intenta encontrar una definición conocida en base al nombre de archivo.
        public static KnownTexture? Match(string fileName)
        {
            string baseName = Path.GetFileNameWithoutExtension(fileName);
            foreach (var t in All)
            {
                if (baseName.Contains(t.NameHint, StringComparison.OrdinalIgnoreCase))
                    return t;
            }
            return null;
        }
    }

    public static class TextureDecoder
    {
        // Decodifica RGB5A3 (16 bits): bit más significativo decide el modo.
        private static (byte r, byte g, byte b, byte a) DecodeRGB5A3(ushort v)
        {
            if ((v & 0x8000) != 0)
            {
                byte r = (byte)(((v >> 10) & 0x1F) * 255 / 31);
                byte g = (byte)(((v >> 5) & 0x1F) * 255 / 31);
                byte b = (byte)((v & 0x1F) * 255 / 31);
                return (r, g, b, 255);
            }
            else
            {
                byte a = (byte)(((v >> 12) & 0x7) * 255 / 7);
                byte r = (byte)(((v >> 8) & 0xF) * 255 / 15);
                byte g = (byte)(((v >> 4) & 0xF) * 255 / 15);
                byte b = (byte)((v & 0xF) * 255 / 15);
                return (r, g, b, a);
            }
        }

        private static (byte r, byte g, byte b, byte a) DecodeRGBA5551(ushort v)
        {
            byte r = (byte)(((v >> 11) & 0x1F) * 255 / 31);
            byte g = (byte)(((v >> 6) & 0x1F) * 255 / 31);
            byte b = (byte)(((v >> 1) & 0x1F) * 255 / 31);
            byte a = (byte)((v & 1) != 0 ? 255 : 0);
            return (r, g, b, a);
        }

        // Decodifica los bytes crudos de un .tx a un Bitmap, según el formato indicado.
        // Devuelve también cuántos bytes se usaron realmente (útil para validar que
        // el ancho/alto elegido coincide con el tamaño del archivo).
        public static (Bitmap bitmap, int bytesUsed) Decode(byte[] data, int width, int height, TextureFormat fmt)
        {
            var bmp = new Bitmap(width, height, PixelFormat.Format32bppArgb);
            var rect = new Rectangle(0, 0, width, height);
            var bmpData = bmp.LockBits(rect, ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);

            // Buffer temporal en formato BGRA (el que espera Bitmap en Windows).
            byte[] buffer = new byte[width * height * 4];
            int idx = 0;

            void SetPixel(int x, int y, byte r, byte g, byte b, byte a)
            {
                if (x < 0 || x >= width || y < 0 || y >= height) return;
                int o = (y * width + x) * 4;
                buffer[o + 0] = b;
                buffer[o + 1] = g;
                buffer[o + 2] = r;
                buffer[o + 3] = a;
            }

            byte ReadByte()
            {
                if (idx < data.Length) return data[idx++];
                idx++;
                return 0;
            }

            switch (fmt)
            {
                case TextureFormat.GX_I4:
                    // Bloques de 8x8, 2 texeles por byte (nibble alto / nibble bajo).
                    for (int by = 0; by < height; by += 8)
                    for (int bx = 0; bx < width; bx += 8)
                    for (int y = 0; y < 8; y++)
                    for (int x = 0; x < 8; x += 2)
                    {
                        byte b0 = ReadByte();
                        byte hi = (byte)((b0 >> 4) * 17);
                        byte lo = (byte)((b0 & 0xF) * 17);
                        SetPixel(bx + x, by + y, hi, hi, hi, 255);
                        SetPixel(bx + x + 1, by + y, lo, lo, lo, 255);
                    }
                    break;

                case TextureFormat.GX_I8:
                    // Bloques de 8x4, 1 texel por byte.
                    for (int by = 0; by < height; by += 4)
                    for (int bx = 0; bx < width; bx += 8)
                    for (int y = 0; y < 4; y++)
                    for (int x = 0; x < 8; x++)
                    {
                        byte v = ReadByte();
                        SetPixel(bx + x, by + y, v, v, v, 255);
                    }
                    break;

                case TextureFormat.GX_RGB5A3:
                    // Bloques de 4x4, 16 bits por texel.
                    for (int by = 0; by < height; by += 4)
                    for (int bx = 0; bx < width; bx += 4)
                    for (int y = 0; y < 4; y++)
                    for (int x = 0; x < 4; x++)
                    {
                        byte b0 = ReadByte();
                        byte b1 = ReadByte();
                        ushort v = (ushort)((b0 << 8) | b1);
                        var (r, g, b, a) = DecodeRGB5A3(v);
                        SetPixel(bx + x, by + y, r, g, b, a);
                    }
                    break;

                case TextureFormat.GX_IA8:
                    // Bloques de 4x4, 16 bits por texel: byte alto = intensidad, byte bajo = alpha.
                    // Formato típico para cursores e íconos que solo necesitan blanco/negro + transparencia.
                    for (int by = 0; by < height; by += 4)
                    for (int bx = 0; bx < width; bx += 4)
                    for (int y = 0; y < 4; y++)
                    for (int x = 0; x < 4; x++)
                    {
                        byte i = ReadByte();
                        byte a = ReadByte();
                        SetPixel(bx + x, by + y, i, i, i, a);
                    }
                    break;

                case TextureFormat.GX_RGBA8:
                    // Bloques de 4x4, pero en DOS sub-bloques de 32 bytes:
                    // primero AR (16 texeles x 2 bytes), después GB (16 texeles x 2 bytes).
                    for (int by = 0; by < height; by += 4)
                    for (int bx = 0; bx < width; bx += 4)
                    {
                        byte[] ar = new byte[32];
                        byte[] gb = new byte[32];
                        for (int i = 0; i < 32; i++) ar[i] = ReadByte();
                        for (int i = 0; i < 32; i++) gb[i] = ReadByte();

                        int p = 0;
                        for (int y = 0; y < 4; y++)
                        for (int x = 0; x < 4; x++)
                        {
                            byte a = ar[p * 2];
                            byte r = ar[p * 2 + 1];
                            byte g = gb[p * 2];
                            byte b = gb[p * 2 + 1];
                            SetPixel(bx + x, by + y, r, g, b, a);
                            p++;
                        }
                    }
                    break;

                case TextureFormat.RGBA5551:
                    // Formato crudo del N64, sin bloques (lineal).
                    for (int y = 0; y < height; y++)
                    for (int x = 0; x < width; x++)
                    {
                        byte b0 = ReadByte();
                        byte b1 = ReadByte();
                        ushort v = (ushort)((b0 << 8) | b1);
                        var (r, g, b, a) = DecodeRGBA5551(v);
                        SetPixel(x, y, r, g, b, a);
                    }
                    break;
            }

            System.Runtime.InteropServices.Marshal.Copy(buffer, 0, bmpData.Scan0, buffer.Length);
            bmp.UnlockBits(bmpData);

            return (bmp, Math.Min(idx, data.Length));
        }
    }
}
