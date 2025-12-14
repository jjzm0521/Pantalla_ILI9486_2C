#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import argparse
import os
import numpy as np
from PIL import Image
import tkinter as tk
from tkinter import filedialog
import sys

def load_img(path, channel="gray"):
    """Carga imagen y devuelve intensidades float32 (0..255 aprox)."""
    img = Image.open(path).convert("RGB")
    arr = np.asarray(img).astype(np.float32)

    if channel == "gray":
        # luminancia (mejor que promedio simple)
        arr = 0.2126 * arr[..., 0] + 0.7152 * arr[..., 1] + 0.0722 * arr[..., 2]
    elif channel == "r":
        arr = arr[..., 0]
    elif channel == "g":
        arr = arr[..., 1]
    elif channel == "b":
        arr = arr[..., 2]
    else:
        raise ValueError("channel debe ser: gray, r, g, b")

    return arr

def subtract_bg(I, bg_rect=None):
    """
    Resta fondo usando un rectángulo (x,y,w,h) mediana.
    Útil para quitar offset / luz parásita.
    """
    if bg_rect is None:
        return I
    x, y, w, h = bg_rect
    roi = I[y:y+h, x:x+w]
    bg = np.median(roi)
    return I - bg

def ensure_same_shape(*imgs):
    shapes = [im.shape for im in imgs]
    if len(set(shapes)) != 1:
        raise ValueError(f"Las imágenes no tienen el mismo tamaño: {shapes}")

def save_u8(arr, path, vmin=None, vmax=None):
    """Guarda un mapa en 8-bit escalando [vmin,vmax] → [0,255]."""
    a = np.array(arr, dtype=np.float32)
    if vmin is None:
        vmin = np.nanpercentile(a, 1)
    if vmax is None:
        vmax = np.nanpercentile(a, 99)
    if vmax <= vmin:
        vmax = vmin + 1e-6
    a = (a - vmin) / (vmax - vmin)
    a = np.clip(a, 0, 1)
    im = Image.fromarray((a * 255).astype(np.uint8))
    im.save(path)

def save_signed_centered(arr, path, maxabs=None):
    """Guarda mapa con valores positivos/negativos centrando en 0."""
    a = np.array(arr, dtype=np.float32)
    if maxabs is None:
        maxabs = np.nanpercentile(np.abs(a), 99)
    if maxabs <= 0:
        maxabs = 1e-6
    a = (a / maxabs + 1.0) / 2.0  # [-maxabs,+maxabs] → [0,1]
    a = np.clip(a, 0, 1)
    im = Image.fromarray((a * 255).astype(np.uint8))
    im.save(path)

def main():
    p = argparse.ArgumentParser(
        description="Cálculo de Stokes lineales (S0,S1,S2) desde 0/45/90/135 grados."
    )
    p.add_argument("--i0", help="Imagen a 0°")
    p.add_argument("--i45", help="Imagen a 45°")
    p.add_argument("--i90", help="Imagen a 90°")
    p.add_argument("--i135", help="Imagen a 135°")
    p.add_argument("--out", default="out_stokes", help="Carpeta de salida")
    p.add_argument("--channel", default="gray", choices=["gray", "r", "g", "b"],
                   help="Canal a usar (gray recomendado)")
    p.add_argument("--bg_rect", default=None,
                   help="Rectángulo de fondo 'x,y,w,h' (opcional). Ej: 10,10,80,80")
    args = p.parse_args()

    # Si faltan argumentos de imagen, usar selector de archivos
    if not (args.i0 and args.i45 and args.i90 and args.i135):
        print("Faltan argumentos de imagen. Abriendo selector de archivos...")
        try:
            root = tk.Tk()
            root.withdraw() # Ocultar la ventana principal
        except Exception as e:
            print(f"No se pudo inicializar la interfaz gráfica: {e}")
            sys.exit(1)

        if not args.i0:
            print("Seleccione la imagen a 0°")
            args.i0 = filedialog.askopenfilename(title="Seleccione la imagen a 0°",
                                               filetypes=[("Imágenes", "*.png *.jpg *.jpeg *.bmp *.tiff *.tif")])
        if not args.i0: sys.exit("No se seleccionó imagen a 0°")

        if not args.i45:
            print("Seleccione la imagen a 45°")
            args.i45 = filedialog.askopenfilename(title="Seleccione la imagen a 45°",
                                                filetypes=[("Imágenes", "*.png *.jpg *.jpeg *.bmp *.tiff *.tif")])
        if not args.i45: sys.exit("No se seleccionó imagen a 45°")

        if not args.i90:
            print("Seleccione la imagen a 90°")
            args.i90 = filedialog.askopenfilename(title="Seleccione la imagen a 90°",
                                                filetypes=[("Imágenes", "*.png *.jpg *.jpeg *.bmp *.tiff *.tif")])
        if not args.i90: sys.exit("No se seleccionó imagen a 90°")

        if not args.i135:
            print("Seleccione la imagen a 135°")
            args.i135 = filedialog.askopenfilename(title="Seleccione la imagen a 135°",
                                                 filetypes=[("Imágenes", "*.png *.jpg *.jpeg *.bmp *.tiff *.tif")])
        if not args.i135: sys.exit("No se seleccionó imagen a 135°")

        root.destroy()

    os.makedirs(args.out, exist_ok=True)

    bg_rect = None
    if args.bg_rect:
        bg_rect = tuple(int(v) for v in args.bg_rect.split(","))

    I0 = subtract_bg(load_img(args.i0, args.channel), bg_rect)
    I45 = subtract_bg(load_img(args.i45, args.channel), bg_rect)
    I90 = subtract_bg(load_img(args.i90, args.channel), bg_rect)
    I135 = subtract_bg(load_img(args.i135, args.channel), bg_rect)

    ensure_same_shape(I0, I45, I90, I135)

    # --- Stokes lineales ---
    S0 = I0 + I90
    S1 = I0 - I90
    S2 = I45 - I135

    eps = 1e-9
    s1 = S1 / (S0 + eps)
    s2 = S2 / (S0 + eps)

    DoLP = np.sqrt(S1**2 + S2**2) / (S0 + eps)
    DoLP = np.clip(DoLP, 0, 1)

    AoP = 0.5 * np.arctan2(S2, S1)          # rad
    AoP_deg = (np.degrees(AoP) + 180) % 180 # 0..180 grados

    # --- Guardar resultados ---
    save_u8(S0, os.path.join(args.out, "S0.png"))
    save_signed_centered(S1, os.path.join(args.out, "S1.png"))
    save_signed_centered(S2, os.path.join(args.out, "S2.png"))
    save_u8(DoLP, os.path.join(args.out, "DoLP.png"), vmin=0, vmax=1)
    save_u8(AoP_deg, os.path.join(args.out, "AoP_deg.png"), vmin=0, vmax=180)

    # Guardar arrays para análisis
    np.savez_compressed(
        os.path.join(args.out, "stokes_lineales.npz"),
        S0=S0, S1=S1, S2=S2, s1=s1, s2=s2, DoLP=DoLP, AoP_deg=AoP_deg
    )

    print("Listo. Resultados guardados en:", args.out)
    print("Archivos: S0.png, S1.png, S2.png, DoLP.png, AoP_deg.png, stokes_lineales.npz")

if __name__ == "__main__":
    main()
