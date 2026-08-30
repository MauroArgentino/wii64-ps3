using System.Text.Json;

namespace Wii64TxViewer
{
    // Una entrada confirmada por el usuario: "este archivo, con este ancho/alto/formato, se ve bien".
    public class ConfirmedTexture
    {
        public int Width { get; set; }
        public int Height { get; set; }
        public string Format { get; set; } = "GX_RGB5A3";
        public long FileSizeBytes { get; set; }
    }

    // Guarda y carga el archivo de configuración con las texturas que el usuario ya confirmó
    // a mano (tildando "Marcar como correcto" en el visor). Se guarda en la carpeta de datos
    // de la aplicación, así persiste entre sesiones sin depender de dónde esté el .exe.
    //
    // La clave es el NOMBRE del archivo (en minúsculas), no la ruta completa — así, si el mismo
    // archivo aparece en varias carpetas (por ejemplo distintas copias del repo), la app lo
    // reconoce igual.
    public static class ConfigStore
    {
        private static readonly string ConfigDir = Path.Combine(
            Environment.GetFolderPath(Environment.SpecialFolder.ApplicationData),
            "Wii64TxViewer");

        private static readonly string ConfigPath = Path.Combine(ConfigDir, "confirmed_textures.json");

        private static Dictionary<string, ConfirmedTexture>? cache;

        public static Dictionary<string, ConfirmedTexture> LoadAll()
        {
            if (cache != null) return cache;

            try
            {
                if (File.Exists(ConfigPath))
                {
                    string json = File.ReadAllText(ConfigPath);
                    cache = JsonSerializer.Deserialize<Dictionary<string, ConfirmedTexture>>(json)
                            ?? new Dictionary<string, ConfirmedTexture>();
                }
                else
                {
                    cache = new Dictionary<string, ConfirmedTexture>();
                }
            }
            catch
            {
                // Si el archivo de config está corrupto o no se puede leer, arrancamos de cero
                // en vez de romper la app.
                cache = new Dictionary<string, ConfirmedTexture>();
            }

            return cache;
        }

        public static ConfirmedTexture? Get(string fileName)
        {
            var all = LoadAll();
            return all.TryGetValue(fileName.ToLowerInvariant(), out var v) ? v : null;
        }

        public static void Save(string fileName, int width, int height, TextureFormat format, long fileSizeBytes)
        {
            var all = LoadAll();
            all[fileName.ToLowerInvariant()] = new ConfirmedTexture
            {
                Width = width,
                Height = height,
                Format = format.ToString(),
                FileSizeBytes = fileSizeBytes
            };

            Directory.CreateDirectory(ConfigDir);
            var options = new JsonSerializerOptions { WriteIndented = true };
            File.WriteAllText(ConfigPath, JsonSerializer.Serialize(all, options));
        }

        public static string GetConfigPath() => ConfigPath;
    }
}
