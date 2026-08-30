using System.Drawing.Imaging;
using SixLabors.ImageSharp.PixelFormats;

namespace Wii64TxViewer
{
    public class MainForm : Form
    {
        // ================= PESTAÑA 1: VISOR DE .tx =================
        private ListBox lstFolders = new();
        private Button btnAddFolder = new();
        private Button btnRemoveFolder = new();
        private Button btnScan = new();
        private ListBox lstFiles = new();
        private Label lblFileCount = new();

        private PictureBox picPreview = new();
        private Panel pnlPreviewBg = new();
        private NumericUpDown numWidth = new();
        private NumericUpDown numHeight = new();
        private ComboBox cmbFormat = new();
        private Label lblStatus = new();
        private Button btnSavePng = new();
        private Button btnMarkCorrect = new();
        private Label lblConfirmedBadge = new();
        private Label lblFileName = new();

        private readonly List<string> folders = new();
        private readonly List<string> foundFiles = new();
        private Bitmap? currentBitmap;

        // ================= PESTAÑA 2: CONVERTIDOR PNG -> .tx =================
        private Button btnOpenPng = new();
        private Label lblPngName = new();
        private PictureBox picOriginal = new();
        private PictureBox picResult = new();
        private ComboBox cmbKnownTarget = new();
        private NumericUpDown numConvWidth = new();
        private NumericUpDown numConvHeight = new();
        private ComboBox cmbConvFormat = new();
        private CheckBox chkPixelArt = new();
        private CheckBox chkCrop = new();
        private Label lblConvStatus = new();
        private Button btnConvert = new();
        private Button btnSaveTx = new();

        private Bitmap? sourcePng;
        private byte[]? encodedResult;
        private Bitmap? fittedBitmap;

        public MainForm()
        {
            Text = "Wii64 .tx Viewer / Convertidor";
            Width = 1080;
            Height = 700;
            StartPosition = FormStartPosition.CenterScreen;

            var tabs = new TabControl { Dock = DockStyle.Fill };
            var tabViewer = new TabPage("Visor de .tx");
            var tabConverter = new TabPage("Convertidor PNG → .tx");

            BuildViewerTab(tabViewer);
            BuildConverterTab(tabConverter);

            tabs.TabPages.Add(tabViewer);
            tabs.TabPages.Add(tabConverter);
            Controls.Add(tabs);
        }

        // ============================================================
        //  PESTAÑA 1 — VISOR (igual que antes)
        // ============================================================
        private void BuildViewerTab(TabPage tab)
        {
            var leftPanel = new Panel { Dock = DockStyle.Left, Width = 380, Padding = new Padding(10) };

            var lblFolders = new Label { Text = "Carpetas:", Dock = DockStyle.Top, Height = 20 };

            lstFolders.Dock = DockStyle.Top;
            lstFolders.Height = 90;

            var folderButtonsPanel = new FlowLayoutPanel { Dock = DockStyle.Top, Height = 34, FlowDirection = FlowDirection.LeftToRight };
            btnAddFolder.Text = "Agregar carpeta...";
            btnAddFolder.AutoSize = true;
            btnAddFolder.Click += BtnAddFolder_Click;

            btnRemoveFolder.Text = "Quitar seleccionada";
            btnRemoveFolder.AutoSize = true;
            btnRemoveFolder.Click += BtnRemoveFolder_Click;

            folderButtonsPanel.Controls.Add(btnAddFolder);
            folderButtonsPanel.Controls.Add(btnRemoveFolder);

            btnScan.Text = "Buscar archivos .tx";
            btnScan.Dock = DockStyle.Top;
            btnScan.Height = 32;
            btnScan.Click += BtnScan_Click;

            lblFileCount.Dock = DockStyle.Top;
            lblFileCount.Height = 20;
            lblFileCount.Text = "0 archivos .tx encontrados";
            lblFileCount.Padding = new Padding(0, 6, 0, 0);

            var lblFiles = new Label { Text = "Archivos encontrados:", Dock = DockStyle.Top, Height = 20, Padding = new Padding(0, 8, 0, 0) };

            lstFiles.Dock = DockStyle.Fill;
            lstFiles.SelectedIndexChanged += LstFiles_SelectedIndexChanged;

            leftPanel.Controls.Add(lstFiles);
            leftPanel.Controls.Add(lblFiles);
            leftPanel.Controls.Add(lblFileCount);
            leftPanel.Controls.Add(btnScan);
            leftPanel.Controls.Add(folderButtonsPanel);
            leftPanel.Controls.Add(lstFolders);
            leftPanel.Controls.Add(lblFolders);

            var rightPanel = new Panel { Dock = DockStyle.Fill, Padding = new Padding(10) };

            lblFileName.Dock = DockStyle.Top;
            lblFileName.Height = 24;
            lblFileName.Font = new Font(lblFileName.Font, FontStyle.Bold);
            lblFileName.Text = "(ningún archivo seleccionado)";

            var controlsPanel = new FlowLayoutPanel { Dock = DockStyle.Top, Height = 40, FlowDirection = FlowDirection.LeftToRight };

            controlsPanel.Controls.Add(new Label { Text = "Ancho:", AutoSize = true, Padding = new Padding(0, 8, 4, 0) });
            numWidth.Minimum = 1;
            numWidth.Maximum = 4096;
            numWidth.Value = 64;
            numWidth.Width = 70;
            numWidth.ValueChanged += (s, e) => RenderCurrent();
            controlsPanel.Controls.Add(numWidth);

            controlsPanel.Controls.Add(new Label { Text = "Alto:", AutoSize = true, Padding = new Padding(10, 8, 4, 0) });
            numHeight.Minimum = 1;
            numHeight.Maximum = 4096;
            numHeight.Value = 64;
            numHeight.Width = 70;
            numHeight.ValueChanged += (s, e) => RenderCurrent();
            controlsPanel.Controls.Add(numHeight);

            controlsPanel.Controls.Add(new Label { Text = "Formato:", AutoSize = true, Padding = new Padding(10, 8, 4, 0) });
            cmbFormat.DropDownStyle = ComboBoxStyle.DropDownList;
            cmbFormat.Items.AddRange(new object[] { "GX_I4", "GX_I8", "GX_IA8", "GX_RGBA8", "GX_RGB5A3", "RGBA5551 (N64)" });
            cmbFormat.SelectedIndex = 4;
            cmbFormat.Width = 140;
            cmbFormat.SelectedIndexChanged += (s, e) => RenderCurrent();
            controlsPanel.Controls.Add(cmbFormat);

            btnSavePng.Text = "Guardar como PNG...";
            btnSavePng.AutoSize = true;
            btnSavePng.Margin = new Padding(15, 3, 0, 0);
            btnSavePng.Click += BtnSavePng_Click;
            controlsPanel.Controls.Add(btnSavePng);

            btnMarkCorrect.Text = "✓ Marcar estas dimensiones como correctas";
            btnMarkCorrect.AutoSize = true;
            btnMarkCorrect.Margin = new Padding(15, 3, 0, 0);
            btnMarkCorrect.BackColor = Color.FromArgb(210, 240, 210);
            btnMarkCorrect.Click += BtnMarkCorrect_Click;
            controlsPanel.Controls.Add(btnMarkCorrect);

            lblConfirmedBadge.Dock = DockStyle.Top;
            lblConfirmedBadge.Height = 20;
            lblConfirmedBadge.ForeColor = Color.FromArgb(30, 130, 30);
            lblConfirmedBadge.Font = new Font(lblConfirmedBadge.Font, FontStyle.Bold);
            lblConfirmedBadge.Text = "";

            lblStatus.Dock = DockStyle.Top;
            lblStatus.Height = 24;
            lblStatus.ForeColor = Color.DimGray;
            lblStatus.Text = "Elegí una carpeta, buscá archivos, y seleccioná uno de la lista.";

            pnlPreviewBg.Dock = DockStyle.Fill;
            pnlPreviewBg.BackColor = Color.FromArgb(230, 230, 230);
            pnlPreviewBg.AutoScroll = true;

            picPreview.SizeMode = PictureBoxSizeMode.AutoSize;
            picPreview.Location = new Point(10, 10);
            picPreview.BackColor = Color.Transparent;
            pnlPreviewBg.Controls.Add(picPreview);

            rightPanel.Controls.Add(pnlPreviewBg);
            rightPanel.Controls.Add(lblStatus);
            rightPanel.Controls.Add(lblConfirmedBadge);
            rightPanel.Controls.Add(controlsPanel);
            rightPanel.Controls.Add(lblFileName);

            tab.Controls.Add(rightPanel);
            tab.Controls.Add(leftPanel);
        }

        private void BtnAddFolder_Click(object? sender, EventArgs e)
        {
            using var dlg = new FolderBrowserDialog();
            dlg.Description = "Elegí una carpeta donde buscar archivos .tx";
            if (dlg.ShowDialog() == DialogResult.OK)
            {
                if (!folders.Contains(dlg.SelectedPath))
                {
                    folders.Add(dlg.SelectedPath);
                    lstFolders.Items.Add(dlg.SelectedPath);
                }
            }
        }

        private void BtnRemoveFolder_Click(object? sender, EventArgs e)
        {
            if (lstFolders.SelectedItem is string sel)
            {
                folders.Remove(sel);
                lstFolders.Items.Remove(sel);
            }
        }

        private void BtnScan_Click(object? sender, EventArgs e)
        {
            foundFiles.Clear();
            lstFiles.Items.Clear();

            foreach (var folder in folders)
            {
                try
                {
                    var files = Directory.GetFiles(folder, "*.tx", SearchOption.AllDirectories);
                    foreach (var f in files)
                    {
                        foundFiles.Add(f);
                        lstFiles.Items.Add(Path.GetRelativePath(folder, f) + "   [" + folder + "]");
                    }
                }
                catch (Exception ex)
                {
                    MessageBox.Show($"No se pudo leer la carpeta:\n{folder}\n\n{ex.Message}", "Error", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                }
            }

            lblFileCount.Text = $"{foundFiles.Count} archivo(s) .tx encontrado(s)";
        }

        private void LstFiles_SelectedIndexChanged(object? sender, EventArgs e)
        {
            int i = lstFiles.SelectedIndex;
            if (i < 0 || i >= foundFiles.Count) return;

            string path = foundFiles[i];
            lblFileName.Text = Path.GetFileName(path);

            // Prioridad 1: si el usuario ya confirmó manualmente este archivo antes, usar eso.
            var confirmed = ConfigStore.Get(Path.GetFileName(path));
            if (confirmed != null)
            {
                numWidth.Value = confirmed.Width;
                numHeight.Value = confirmed.Height;
                if (Enum.TryParse<TextureFormat>(confirmed.Format, out var f))
                    cmbFormat.SelectedIndex = FormatToIndex(f);

                lblConfirmedBadge.Text = "✓ Configuración guardada previamente por vos para este archivo";
            }
            else
            {
                // Prioridad 2: intentar adivinar por el nombre del archivo (lista conocida del proyecto).
                var known = KnownTextures.Match(Path.GetFileName(path));
                if (known != null)
                {
                    numWidth.Value = known.Width;
                    numHeight.Value = known.Height;
                    cmbFormat.SelectedIndex = FormatToIndex(known.Format);
                }

                lblConfirmedBadge.Text = "";
            }

            RenderCurrent();
        }

        private static int FormatToIndex(TextureFormat fmt) => fmt switch
        {
            TextureFormat.GX_I4 => 0,
            TextureFormat.GX_I8 => 1,
            TextureFormat.GX_IA8 => 2,
            TextureFormat.GX_RGBA8 => 3,
            TextureFormat.GX_RGB5A3 => 4,
            TextureFormat.RGBA5551 => 5,
            _ => 4
        };

        private static TextureFormat IndexToFormat(int i) => i switch
        {
            0 => TextureFormat.GX_I4,
            1 => TextureFormat.GX_I8,
            2 => TextureFormat.GX_IA8,
            3 => TextureFormat.GX_RGBA8,
            4 => TextureFormat.GX_RGB5A3,
            5 => TextureFormat.RGBA5551,
            _ => TextureFormat.GX_RGB5A3
        };

        private void RenderCurrent()
        {
            int i = lstFiles.SelectedIndex;
            if (i < 0 || i >= foundFiles.Count) return;

            string path = foundFiles[i];
            byte[] data;
            try
            {
                data = File.ReadAllBytes(path);
            }
            catch (Exception ex)
            {
                lblStatus.Text = $"Error leyendo el archivo: {ex.Message}";
                return;
            }

            int w = (int)numWidth.Value;
            int h = (int)numHeight.Value;
            var fmt = IndexToFormat(cmbFormat.SelectedIndex);

            currentBitmap?.Dispose();

            try
            {
                var (bmp, bytesUsed) = TextureDecoder.Decode(data, w, h, fmt);
                currentBitmap = bmp;

                int scale = Math.Max(1, Math.Min(8, 480 / Math.Max(w, h)));
                picPreview.Image = new Bitmap(bmp, new Size(w * scale, h * scale));

                string warn = bytesUsed > data.Length
                    ? $"  ⚠ Faltan bytes para {w}x{h} (necesita {bytesUsed}, hay {data.Length})"
                    : (bytesUsed < data.Length ? $"  ⚠ Sobran bytes ({data.Length - bytesUsed} sin usar) — probá otras dimensiones" : "  ✓ El tamaño coincide exacto");

                lblStatus.Text = $"{w}x{h} · {cmbFormat.SelectedItem} · {data.Length} bytes en archivo{warn}";
            }
            catch (Exception ex)
            {
                lblStatus.Text = $"Error decodificando: {ex.Message}";
            }
        }

        private void BtnSavePng_Click(object? sender, EventArgs e)
        {
            if (currentBitmap == null)
            {
                MessageBox.Show("No hay ninguna imagen decodificada para guardar.", "Aviso", MessageBoxButtons.OK, MessageBoxIcon.Information);
                return;
            }

            using var dlg = new SaveFileDialog();
            dlg.Filter = "Imagen PNG (*.png)|*.png";
            dlg.FileName = Path.GetFileNameWithoutExtension(lblFileName.Text) + ".png";
            if (dlg.ShowDialog() == DialogResult.OK)
            {
                currentBitmap.Save(dlg.FileName, ImageFormat.Png);
            }
        }

        private void BtnMarkCorrect_Click(object? sender, EventArgs e)
        {
            int i = lstFiles.SelectedIndex;
            if (i < 0 || i >= foundFiles.Count)
            {
                MessageBox.Show("Primero seleccioná un archivo de la lista.", "Aviso", MessageBoxButtons.OK, MessageBoxIcon.Information);
                return;
            }

            string path = foundFiles[i];
            string fileName = Path.GetFileName(path);
            long fileSize;
            try { fileSize = new FileInfo(path).Length; }
            catch { fileSize = 0; }

            int w = (int)numWidth.Value;
            int h = (int)numHeight.Value;
            var fmt = IndexToFormat(cmbFormat.SelectedIndex);

            ConfigStore.Save(fileName, w, h, fmt, fileSize);

            lblConfirmedBadge.Text = "✓ Configuración guardada previamente por vos para este archivo";
            MessageBox.Show(
                $"Guardado. La próxima vez que abras \"{fileName}\" (en esta u otra carpeta), " +
                $"la app va a configurar automáticamente {w}x{h}, {fmt}.\n\n" +
                $"Archivo de configuración: {ConfigStore.GetConfigPath()}",
                "Configuración confirmada", MessageBoxButtons.OK, MessageBoxIcon.Information);
        }

        // ============================================================
        //  PESTAÑA 2 — CONVERTIDOR PNG -> .tx
        // ============================================================
        private void BuildConverterTab(TabPage tab)
        {
            var leftPanel = new Panel { Dock = DockStyle.Left, Width = 420, Padding = new Padding(10) };

            btnOpenPng.Text = "Abrir PNG...";
            btnOpenPng.Dock = DockStyle.Top;
            btnOpenPng.Height = 32;
            btnOpenPng.Click += BtnOpenPng_Click;

            lblPngName.Dock = DockStyle.Top;
            lblPngName.Height = 22;
            lblPngName.Text = "(ningún PNG cargado)";
            lblPngName.Padding = new Padding(0, 6, 0, 0);

            var lblOriginal = new Label { Text = "Original:", Dock = DockStyle.Top, Height = 20, Padding = new Padding(0, 6, 0, 0) };

            var pnlOriginal = new Panel { Dock = DockStyle.Top, Height = 180, BackColor = Color.FromArgb(230, 230, 230), AutoScroll = true };
            picOriginal.SizeMode = PictureBoxSizeMode.Zoom;
            picOriginal.Dock = DockStyle.Fill;
            pnlOriginal.Controls.Add(picOriginal);

            var lblTarget = new Label { Text = "Textura de destino:", Dock = DockStyle.Top, Height = 20, Padding = new Padding(0, 10, 0, 0) };

            cmbKnownTarget.Dock = DockStyle.Top;
            cmbKnownTarget.DropDownStyle = ComboBoxStyle.DropDownList;
            cmbKnownTarget.Items.Add("(Manual — elegir ancho/alto/formato abajo)");
            foreach (var k in KnownTextures.All)
                cmbKnownTarget.Items.Add($"{k.NameHint}  ({k.Width}x{k.Height}, {k.Format})");
            cmbKnownTarget.SelectedIndex = 0;
            cmbKnownTarget.SelectedIndexChanged += CmbKnownTarget_SelectedIndexChanged;

            var dimsPanel = new FlowLayoutPanel { Dock = DockStyle.Top, Height = 36, FlowDirection = FlowDirection.LeftToRight };
            dimsPanel.Controls.Add(new Label { Text = "Ancho:", AutoSize = true, Padding = new Padding(0, 8, 4, 0) });
            numConvWidth.Minimum = 1; numConvWidth.Maximum = 4096; numConvWidth.Value = 64; numConvWidth.Width = 70;
            dimsPanel.Controls.Add(numConvWidth);
            dimsPanel.Controls.Add(new Label { Text = "Alto:", AutoSize = true, Padding = new Padding(10, 8, 4, 0) });
            numConvHeight.Minimum = 1; numConvHeight.Maximum = 4096; numConvHeight.Value = 64; numConvHeight.Width = 70;
            dimsPanel.Controls.Add(numConvHeight);

            var fmtPanel = new FlowLayoutPanel { Dock = DockStyle.Top, Height = 36, FlowDirection = FlowDirection.LeftToRight };
            fmtPanel.Controls.Add(new Label { Text = "Formato:", AutoSize = true, Padding = new Padding(0, 8, 4, 0) });
            cmbConvFormat.DropDownStyle = ComboBoxStyle.DropDownList;
            cmbConvFormat.Items.AddRange(new object[] { "GX_I4", "GX_I8", "GX_IA8", "GX_RGBA8", "GX_RGB5A3", "RGBA5551 (N64)" });
            cmbConvFormat.SelectedIndex = 4;
            cmbConvFormat.Width = 140;
            fmtPanel.Controls.Add(cmbConvFormat);

            chkPixelArt.Text = "Pixel art (sin suavizado al redimensionar)";
            chkPixelArt.Dock = DockStyle.Top;
            chkPixelArt.Height = 24;
            chkPixelArt.Checked = true;

            chkCrop.Text = "Recortar centrado en vez de estirar (si la proporción no coincide)";
            chkCrop.Dock = DockStyle.Top;
            chkCrop.Height = 24;

            btnConvert.Text = "Convertir y previsualizar";
            btnConvert.Dock = DockStyle.Top;
            btnConvert.Height = 34;
            btnConvert.Margin = new Padding(0, 10, 0, 0);
            btnConvert.Click += BtnConvert_Click;

            btnSaveTx.Text = "Guardar como .tx...";
            btnSaveTx.Dock = DockStyle.Top;
            btnSaveTx.Height = 34;
            btnSaveTx.Enabled = false;
            btnSaveTx.Click += BtnSaveTx_Click;

            leftPanel.Controls.Add(btnSaveTx);
            leftPanel.Controls.Add(btnConvert);
            leftPanel.Controls.Add(chkCrop);
            leftPanel.Controls.Add(chkPixelArt);
            leftPanel.Controls.Add(fmtPanel);
            leftPanel.Controls.Add(dimsPanel);
            leftPanel.Controls.Add(cmbKnownTarget);
            leftPanel.Controls.Add(lblTarget);
            leftPanel.Controls.Add(pnlOriginal);
            leftPanel.Controls.Add(lblOriginal);
            leftPanel.Controls.Add(lblPngName);
            leftPanel.Controls.Add(btnOpenPng);

            var rightPanel = new Panel { Dock = DockStyle.Fill, Padding = new Padding(10) };

            var lblResultTitle = new Label
            {
                Text = "Resultado (decodificado desde el .tx generado — así se va a ver en el emulador):",
                Dock = DockStyle.Top,
                Height = 24,
                Font = new Font(Font, FontStyle.Bold)
            };

            lblConvStatus.Dock = DockStyle.Top;
            lblConvStatus.Height = 40;
            lblConvStatus.ForeColor = Color.DimGray;
            lblConvStatus.Text = "Abrí un PNG, elegí la textura de destino, y hacé click en \"Convertir y previsualizar\".";

            var pnlResultBg = new Panel { Dock = DockStyle.Fill, BackColor = Color.FromArgb(230, 230, 230), AutoScroll = true };
            picResult.SizeMode = PictureBoxSizeMode.AutoSize;
            picResult.Location = new Point(10, 10);
            picResult.BackColor = Color.Transparent;
            pnlResultBg.Controls.Add(picResult);

            rightPanel.Controls.Add(pnlResultBg);
            rightPanel.Controls.Add(lblConvStatus);
            rightPanel.Controls.Add(lblResultTitle);

            tab.Controls.Add(rightPanel);
            tab.Controls.Add(leftPanel);
        }

        private void CmbKnownTarget_SelectedIndexChanged(object? sender, EventArgs e)
        {
            int idx = cmbKnownTarget.SelectedIndex;
            if (idx <= 0) return;

            var known = KnownTextures.All[idx - 1];
            numConvWidth.Value = known.Width;
            numConvHeight.Value = known.Height;
            cmbConvFormat.SelectedIndex = FormatToIndex(known.Format);
        }

        private void BtnOpenPng_Click(object? sender, EventArgs e)
        {
            using var dlg = new OpenFileDialog();
            dlg.Filter = "Imágenes (PNG / JPG / WEBP / BMP)|*.png;*.bmp;*.jpg;*.jpeg;*.webp|Imagen PNG (*.png)|*.png|Imagen JPG (*.jpg;*.jpeg)|*.jpg;*.jpeg|Imagen WEBP (*.webp)|*.webp|Imagen BMP (*.bmp)|*.bmp";
            if (dlg.ShowDialog() == DialogResult.OK)
            {
                sourcePng?.Dispose();
                sourcePng = LoadImageFromFile(dlg.FileName);
                lblPngName.Text = Path.GetFileName(dlg.FileName) + $"  ({sourcePng.Width}x{sourcePng.Height})";
                picOriginal.Image = sourcePng;
                btnSaveTx.Enabled = false;
            }
        }

        // Carga una imagen como Bitmap. System.Drawing no decodifica WEBP de forma
        // nativa, así que para esos archivos usamos SixLabors.ImageSharp y luego
        // copiamos los píxeles a un Bitmap (el código del resto del viewer trabaja
        // con System.Drawing.Bitmap).
        private static Bitmap LoadImageFromFile(string path)
        {
            if (!path.EndsWith(".webp", StringComparison.OrdinalIgnoreCase))
                return new Bitmap(path);

            using var webp = SixLabors.ImageSharp.Image.Load<Rgba32>(path);
            int w = webp.Width, h = webp.Height;
            var bmp = new Bitmap(w, h, System.Drawing.Imaging.PixelFormat.Format32bppArgb);
            var rect = new Rectangle(0, 0, w, h);
            var bmpData = bmp.LockBits(rect, ImageLockMode.WriteOnly, System.Drawing.Imaging.PixelFormat.Format32bppArgb);
            try
            {
                // Bitmap en Windows espera BGRA; ImageSharp da Rgba32 (R,G,B,A).
                const int stride = 4;
                int scanLen = w * stride;
                webp.ProcessPixelRows(accessor =>
                {
                    for (int y = 0; y < h; y++)
                    {
                        var row = accessor.GetRowSpan(y);
                        var dst = new byte[scanLen];
                        for (int x = 0; x < w; x++)
                        {
                            Rgba32 px = row[x];
                            dst[x * stride + 0] = px.B;
                            dst[x * stride + 1] = px.G;
                            dst[x * stride + 2] = px.R;
                            dst[x * stride + 3] = px.A;
                        }
                        System.Runtime.InteropServices.Marshal.Copy(dst, 0,
                            IntPtr.Add(bmpData.Scan0, y * bmpData.Stride), scanLen);
                    }
                });
            }
            finally
            {
                bmp.UnlockBits(bmpData);
            }
            return bmp;
        }

        private void BtnConvert_Click(object? sender, EventArgs e)
        {
            if (sourcePng == null)
            {
                MessageBox.Show("Primero abrí un archivo PNG.", "Aviso", MessageBoxButtons.OK, MessageBoxIcon.Information);
                return;
            }

            int targetW = (int)numConvWidth.Value;
            int targetH = (int)numConvHeight.Value;
            var fmt = IndexToFormat(cmbConvFormat.SelectedIndex);

            try
            {
                fittedBitmap?.Dispose();
                fittedBitmap = TextureEncoder.FitToSize(sourcePng, targetW, targetH, chkPixelArt.Checked, chkCrop.Checked);

                encodedResult = TextureEncoder.Encode(fittedBitmap, fmt);

                var (roundTrip, _) = TextureDecoder.Decode(encodedResult, targetW, targetH, fmt);
                int scale = Math.Max(1, Math.Min(8, 400 / Math.Max(targetW, targetH)));
                picResult.Image = new Bitmap(roundTrip, new Size(targetW * scale, targetH * scale));

                lblConvStatus.Text = $"Convertido a {targetW}x{targetH}, formato {cmbConvFormat.SelectedItem}. " +
                                     $"Tamaño del .tx resultante: {encodedResult.Length} bytes.";
                btnSaveTx.Enabled = true;
            }
            catch (Exception ex)
            {
                lblConvStatus.Text = $"Error al convertir: {ex.Message}";
                btnSaveTx.Enabled = false;
            }
        }

        private void BtnSaveTx_Click(object? sender, EventArgs e)
        {
            if (encodedResult == null)
            {
                MessageBox.Show("Primero hacé click en \"Convertir y previsualizar\".", "Aviso", MessageBoxButtons.OK, MessageBoxIcon.Information);
                return;
            }

            using var dlg = new SaveFileDialog();
            dlg.Filter = "Archivo de textura (*.tx)|*.tx";

            int idx = cmbKnownTarget.SelectedIndex;
            dlg.FileName = idx > 0
                ? KnownTextures.All[idx - 1].NameHint + ".tx"
                : Path.GetFileNameWithoutExtension(lblPngName.Text.Split("  (")[0]) + ".tx";

            if (dlg.ShowDialog() == DialogResult.OK)
            {
                File.WriteAllBytes(dlg.FileName, encodedResult);
                MessageBox.Show("Archivo .tx guardado correctamente.\n\nRecordá reemplazar el archivo original en tu repo " +
                    "y volver a generar el .s / recompilar el proyecto para que el cambio se refleje en el emulador.",
                    "Listo", MessageBoxButtons.OK, MessageBoxIcon.Information);
            }
        }
    }
}
