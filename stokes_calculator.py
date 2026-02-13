#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
import os
import numpy as np
from PIL import Image
import tkinter as tk
from tkinter import filedialog
import sys
import platform
import subprocess

def load_img(path, channel="gray"):
    """
    Carga una imagen desde la ruta especificada y devuelve sus intensidades como array float32.
    El rango de valores resultante es aproximadamente 0..255.

    Argumentos:
        path (str): Ruta al archivo de imagen.
        channel (str): Canal a extraer ('gray', 'r', 'g', 'b').
                       'gray' calcula la luminancia estándar.
    """
    # Abrir imagen y asegurar que esté en modo RGB
    img = Image.open(path).convert("RGB")
    arr = np.asarray(img).astype(np.float32)

    # Seleccionar el canal o calcular luminancia
    if channel == "gray":
        # Cálculo de luminancia usando coeficientes estándar (ITU-R BT.709)
        # Esto es más preciso para la percepción humana que un promedio simple.
        arr = 0.2126 * arr[..., 0] + 0.7152 * arr[..., 1] + 0.0722 * arr[..., 2]
    elif channel == "r":
        arr = arr[..., 0] # Canal Rojo
    elif channel == "g":
        arr = arr[..., 1] # Canal Verde
    elif channel == "b":
        arr = arr[..., 2] # Canal Azul
    else:
        raise ValueError("channel debe ser: gray, r, g, b")

    return arr

def subtract_bg(I, bg_rect=None):
    """
    Resta el fondo de la imagen para corregir offset o luz parásita (dark current/stray light).
    Se usa la mediana de una región rectangular especificada como valor de fondo.

    Argumentos:
        I (numpy.ndarray): Imagen de entrada.
        bg_rect (tuple): Rectángulo de referencia (x, y, ancho, alto) o None.
    """
    if bg_rect is None:
        return I
    x, y, w, h = bg_rect
    # Extraer la región de interés (ROI) para el fondo
    roi = I[y:y+h, x:x+w]
    # Calcular la mediana de esa región (robusto ante ruido)
    bg = np.median(roi)
    # Restar el valor de fondo a toda la imagen
    return I - bg

def ensure_same_shape(*imgs):
    """
    Verifica que todas las imágenes pasadas como argumento tengan las mismas dimensiones.
    Lanza un ValueError si hay discrepancias.
    """
    shapes = [im.shape for im in imgs]
    if len(set(shapes)) != 1:
        raise ValueError(f"Las imágenes no tienen el mismo tamaño: {shapes}")

def save_u8(arr, path, vmin=None, vmax=None):
    """
    Guarda un array numérico como imagen de 8 bits (0-255).

    Realiza un escalado min-max:
      - Los valores <= vmin se convierten en 0 (negro).
      - Los valores >= vmax se convierten en 255 (blanco).
      - Los valores intermedios se interpolan linealmente.

    Si vmin/vmax no se especifican, se usan los percentiles 1% y 99% para mejorar el contraste
    y evitar que outliers (píxeles muertos/calientes) arruinen la visualización.
    """
    a = np.array(arr, dtype=np.float32)
    if vmin is None:
        vmin = np.nanpercentile(a, 1)
    if vmax is None:
        vmax = np.nanpercentile(a, 99)
    if vmax <= vmin:
        vmax = vmin + 1e-6

    # Normalización a [0, 1]
    a = (a - vmin) / (vmax - vmin)
    a = np.clip(a, 0, 1)

    # Conversión a uint8 y guardado
    im = Image.fromarray((a * 255).astype(np.uint8))
    im.save(path)

def save_signed_centered(arr, path, maxabs=None):
    """
    Guarda un mapa con valores positivos y negativos, centrando el 0 en gris medio (128).
    Útil para visualizar parámetros Stokes S1 y S2 que pueden ser negativos.

    El rango [-maxabs, +maxabs] se mapea a [0, 255].
    """
    a = np.array(arr, dtype=np.float32)
    if maxabs is None:
        # Usar el percentil 99 del valor absoluto para definir el rango dinámico
        maxabs = np.nanpercentile(np.abs(a), 99)
    if maxabs <= 0:
        maxabs = 1e-6

    # Mapeo: 0 -> 0.5 (gris), +maxabs -> 1.0 (blanco), -maxabs -> 0.0 (negro)
    a = (a / maxabs + 1.0) / 2.0
    a = np.clip(a, 0, 1)

    im = Image.fromarray((a * 255).astype(np.uint8))
    im.save(path)

def open_file_externally(filepath):
    """
    Intenta abrir un archivo con la aplicación predeterminada del sistema operativo.
    Funciona en Windows, macOS y Linux.
    """
    if not os.path.exists(filepath):
        return

    try:
        if platform.system() == 'Darwin':       # macOS
            subprocess.call(('open', filepath))
        elif platform.system() == 'Windows':    # Windows
            os.startfile(filepath)
        else:                                   # Linux y otros Unix-like
            subprocess.call(('xdg-open', filepath))
    except Exception as e:
        print(f"No se pudo abrir el archivo {filepath}: {e}")

def main():
    # Configuración de argumentos de línea de comandos
    p = argparse.ArgumentParser(
        description="Cálculo de Stokes lineales (S0,S1,S2) desde 0/45/90/135 grados."
    )
    p.add_argument("--i0", help="Imagen con polarizador a 0°")
    p.add_argument("--i45", help="Imagen con polarizador a 45°")
    p.add_argument("--i90", help="Imagen con polarizador a 90°")
    p.add_argument("--i135", help="Imagen con polarizador a 135°")
    p.add_argument("--out", default="out_stokes", help="Carpeta donde se guardarán los resultados")
    p.add_argument("--channel", default="gray", choices=["gray", "r", "g", "b"],
                   help="Canal de color a procesar (gray es recomendado para intensidad total)")
    p.add_argument("--bg_rect", default=None,
                   help="Rectángulo de fondo 'x,y,w,h' para corrección (opcional). Ej: 10,10,80,80")
    args = p.parse_args()

    # --- Lógica de selección de archivos (GUI) ---
    # Si faltan argumentos de imagen, usar selector de archivos visual
    if not (args.i0 and args.i45 and args.i90 and args.i135):
        print("Faltan argumentos de imagen. Abriendo selector de archivos...")
        try:
            root = tk.Tk()
            root.withdraw() # Ocultar la ventana principal de Tkinter
        except Exception as e:
            print(f"No se pudo inicializar la interfaz gráfica: {e}")
            sys.exit(1)

        file_opts = {"filetypes": [("Imágenes", "*.png *.jpg *.jpeg *.bmp *.tiff *.tif *.fit *.fits")]}

        if not args.i0:
            print("Seleccione la imagen a 0°")
            args.i0 = filedialog.askopenfilename(title="Seleccione la imagen a 0°", **file_opts)
        if not args.i0: sys.exit("No se seleccionó imagen a 0°")

        if not args.i45:
            print("Seleccione la imagen a 45°")
            args.i45 = filedialog.askopenfilename(title="Seleccione la imagen a 45°", **file_opts)
        if not args.i45: sys.exit("No se seleccionó imagen a 45°")

        if not args.i90:
            print("Seleccione la imagen a 90°")
            args.i90 = filedialog.askopenfilename(title="Seleccione la imagen a 90°", **file_opts)
        if not args.i90: sys.exit("No se seleccionó imagen a 90°")

        if not args.i135:
            print("Seleccione la imagen a 135°")
            args.i135 = filedialog.askopenfilename(title="Seleccione la imagen a 135°", **file_opts)
        if not args.i135: sys.exit("No se seleccionó imagen a 135°")

        root.destroy()

    # Crear carpeta de salida si no existe
    os.makedirs(args.out, exist_ok=True)

    # Parsear rectángulo de fondo si se proporcionó
    bg_rect = None
    if args.bg_rect:
        bg_rect = tuple(int(v) for v in args.bg_rect.split(","))

    # --- Carga y preprocesamiento de imágenes ---
    print("Cargando y procesando imágenes...")
    I0 = subtract_bg(load_img(args.i0, args.channel), bg_rect)
    I45 = subtract_bg(load_img(args.i45, args.channel), bg_rect)
    I90 = subtract_bg(load_img(args.i90, args.channel), bg_rect)
    I135 = subtract_bg(load_img(args.i135, args.channel), bg_rect)

    ensure_same_shape(I0, I45, I90, I135)

    # --- Cálculo de Parámetros de Stokes Lineales ---
    # S0: Intensidad total
    S0 = I0 + I90
    # S1: Diferencia Horizontal - Vertical
    S1 = I0 - I90
    # S2: Diferencia 45° - 135°
    S2 = I45 - I135

    eps = 1e-9 # Evitar división por cero

    # Stokes normalizados (s1, s2)
    s1 = S1 / (S0 + eps)
    s2 = S2 / (S0 + eps)

    # --- Cálculo de Métricas de Polarización ---
    # DoLP: Grado de Polarización Lineal (0..1)
    DoLP = np.sqrt(S1**2 + S2**2) / (S0 + eps)
    DoLP = np.clip(DoLP, 0, 1)

    # AoP: Ángulo de Polarización Lineal
    AoP = 0.5 * np.arctan2(S2, S1)          # Resultado en radianes (-pi/2 a pi/2)
    AoP_deg = (np.degrees(AoP) + 180) % 180 # Convertir a grados [0..180)

    # --- Guardar resultados visuales ---
    print(f"Guardando resultados en: {args.out}")

    path_S0 = os.path.join(args.out, "S0.png")
    path_S1 = os.path.join(args.out, "S1.png")
    path_S2 = os.path.join(args.out, "S2.png")
    path_DoLP = os.path.join(args.out, "DoLP.png")
    path_AoP = os.path.join(args.out, "AoP_deg.png")

    save_u8(S0, path_S0)
    save_signed_centered(S1, path_S1)
    save_signed_centered(S2, path_S2)
    save_u8(DoLP, path_DoLP, vmin=0, vmax=1)
    save_u8(AoP_deg, path_AoP, vmin=0, vmax=180)

    # Guardar datos crudos (arrays numpy) para análisis posterior científico
    np.savez_compressed(
        os.path.join(args.out, "stokes_lineales.npz"),
        S0=S0, S1=S1, S2=S2, s1=s1, s2=s2, DoLP=DoLP, AoP_deg=AoP_deg
    )

    print("Listo.")
    print("Archivos generados: S0.png, S1.png, S2.png, DoLP.png, AoP_deg.png, stokes_lineales.npz")

    # --- Abrir resultados automáticamente ---
    print("Abriendo imágenes resultantes...")
    # Abrir las imágenes más relevantes para el usuario
    open_file_externally(path_S0)
    open_file_externally(path_DoLP)
    open_file_externally(path_AoP)
    # open_file_externally(path_S1) # Opcional
    # open_file_externally(path_S2) # Opcional

if __name__ == "__main__":
    main()
