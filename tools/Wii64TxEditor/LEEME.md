# Wii64 .tx Viewer

App de escritorio en C# (WinForms) para explorar y previsualizar archivos `.tx`
del proyecto wii64-ps3 (texturas del menú, formato GX de GameCube/Wii).

## Requisitos

- Windows 10/11
- [.NET 8 SDK](https://dotnet.microsoft.com/download) instalado (gratis, de Microsoft)

Para verificar si ya lo tenés instalado, abrí PowerShell y corré:
```powershell
dotnet --version
```
Si te devuelve un número (ej. `8.0.xxx`), ya lo tenés. Si no, descargalo del link de arriba
(elegí la versión SDK, no solo el Runtime).

## Cómo compilar y correr

1. Descomprimí esta carpeta en cualquier lugar, por ejemplo:
   `C:\Users\tupri\Desktop\Wii64TxViewer`

2. Abrí PowerShell en esa carpeta:
   ```powershell
   cd C:\Users\tupri\Desktop\Wii64TxViewer
   ```

3. Compilá y ejecutá:
   ```powershell
   dotnet run
   ```
   La primera vez puede tardar un poco (descarga paquetes). Después de eso se abre la ventana
   de la app directamente.

## Cómo generar un .exe standalone (para no depender de `dotnet run` cada vez)

```powershell
dotnet publish -c Release -r win-x64 --self-contained true -p:PublishSingleFile=true
```

El ejecutable va a quedar en:
```
bin\Release\net8.0-windows\win-x64\publish\Wii64TxViewer.exe
```
Ese `.exe` lo podés mover a donde quieras y correrlo con doble click, sin necesidad de instalar
nada más en esa PC.

## Cómo se usa (pestaña "Visor de .tx")

1. Click en **"Agregar carpeta..."** y elegí la carpeta de tu repo donde están los `.tx`
   (por ejemplo `src\ui\resources`). Podés agregar varias carpetas.
2. Click en **"Buscar archivos .tx"** — escanea todas las carpetas agregadas, incluyendo subcarpetas.
3. Hacé click en un archivo de la lista de la izquierda.
4. La app intenta **adivinar automáticamente** ancho, alto y formato, en este orden de prioridad:
   - Primero, si vos ya lo confirmaste antes con el botón "✓ Marcar como correcto" (ver abajo).
   - Si no, comparando el nombre del archivo contra la lista de texturas conocidas de `GuiResources.cpp`.
   - Si tampoco matchea, quedan los valores por defecto y los ajustás a mano.
5. Si el tamaño no coincide (bytes de más o de menos), aparece un aviso en la parte de abajo —
   probá ajustar ancho/alto hasta que diga "✓ El tamaño coincide exacto".
6. **Una vez que encontraste la combinación correcta a ojo**, hacé click en
   **"✓ Marcar estas dimensiones como correctas"**. Eso queda guardado en un archivo de configuración
   permanente (fuera de la carpeta del proyecto, en los datos de la app), así que la próxima vez que
   abras ese mismo archivo — aunque cierres el programa, reinicies la PC, o lo tengas en otra carpeta —
   la app va a configurar automáticamente esas dimensiones y formato. Vas a ver un aviso verde
   "✓ Configuración guardada previamente por vos" cuando esto pase.
7. **"Guardar como PNG..."** exporta la imagen ya decodificada, útil como referencia visual mientras
   diseñás los nuevos assets para PS3.

El archivo de configuración queda en:
```
%APPDATA%\Wii64TxViewer\confirmed_textures.json
```
Es un JSON simple, así que si algún día querés revisarlo, editarlo a mano, o hacerle backup, lo podés
abrir con cualquier editor de texto.

## Cómo se usa (pestaña "Convertidor PNG → .tx")

Para crear tus propios botones y fondos con GIMP (o cualquier editor) y convertirlos al formato que
lee el emulador:

1. Diseñá tu imagen en GIMP con el **tamaño exacto** de la textura que vas a reemplazar (por ejemplo,
   144x52 para el logo). Exportala como PNG con canal alfa si necesita transparencia.
2. En la app, andá a la pestaña **"Convertidor PNG → .tx"**.
3. **"Abrir PNG..."** y elegí tu archivo.
4. En **"Textura de destino"**, elegí de la lista la textura que estás reemplazando (autocompleta
   ancho/alto/formato), o dejalo en "Manual" y cargá los valores vos mismo.
5. Si tu PNG no tiene exactamente el mismo tamaño que el destino, elegí:
   - **"Pixel art"** tildado: redimensiona sin suavizar (mejor para íconos/botones con bordes duros).
   - **"Recortar centrado"**: en vez de estirar la imagen (deformándola), la escala manteniendo
     proporción y recorta los bordes sobrantes.
6. Click en **"Convertir y previsualizar"** — el panel de la derecha muestra el resultado
   **decodificado de vuelta desde los bytes generados**, es decir, exactamente cómo se va a ver en el
   emulador (incluyendo la pérdida de color por la cuantización de cada formato — RGB5A3 no tiene
   tantos colores como un PNG normal, por ejemplo).
7. Si te convence, **"Guardar como .tx..."** — reemplazá el archivo original en tu repo con este,
   y volvé a generar el `.s`/recompilar para que el cambio se vea en el emulador.

### Sobre la calidad del color

Como los formatos GX tienen menos precisión de color que un PNG (por ejemplo RGB5A3 son máximo
5 bits por canal, y algunos modos usan solo 4 bits + 3 de alpha), colores muy sutiles o degradados
suaves en tu diseño original pueden verse con "bandas" después de convertir — es una limitación del
hardware original, no un bug del conversor. La vista previa te muestra el resultado real así podés
ajustar el diseño en GIMP si hace falta (por ejemplo, usando colores más planos/saturados).

## Formatos soportados

- **GX_I4**: escala de grises, 4 bits por pixel, bloques de 8x8 (usado en los íconos de controles).
- **GX_I8**: escala de grises, 8 bits por pixel, bloques de 8x4 (botones, fondo).
- **GX_RGBA8**: color completo con alpha, bloques de 4x4 en dos sub-bloques (los botones "Style A").
- **GX_RGB5A3**: color con alpha variable, bloques de 4x4, 16 bits por pixel (el logo).
- **RGBA5551**: formato crudo de textura del N64 (por si lo necesitás para otro archivo, no es
  el que usan los assets del menú).

## Nota sobre los formatos y bloques

Estos archivos `.tx` no tienen cabecera — son datos de textura crudos, así que el ancho, alto y
formato no están guardados en el archivo mismo. Los valores por defecto de esta app salen de
revisar `GuiResources.cpp` en el repo, que es donde el código C++ especifica esos parámetros al
cargar cada textura.
