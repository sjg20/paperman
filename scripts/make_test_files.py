#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-or-later

"""Create test files for exercising paperman functionality"""

import argparse
import os
import random

import numpy as np
from PIL import Image

# All generated files go here
OUTPUT_DIR = os.path.join(os.path.dirname(__file__), '..', 'test', 'files')


def _plasma_layer(rng, xgrid, ygrid, channels):
    """Add one layer of sine-based plasma noise to the RGB channels

    Args:
        rng (random.Random): Random number generator
        xgrid (np.ndarray): 2D array of x coordinates
        ygrid (np.ndarray): 2D array of y coordinates
        channels (list of np.ndarray): Three arrays [R, G, B] to accumulate into
    """
    xf = rng.uniform(1, 8)
    yf = rng.uniform(1, 8)
    xp = rng.uniform(0, 2 * np.pi)
    yp = rng.uniform(0, 2 * np.pi)

    val = np.sin(xgrid * xf + xp)
    val += np.sin(ygrid * yf + yp)
    val += np.sin((xgrid + ygrid) * (xf + yf) / 2)

    for ch in channels:
        cf = rng.uniform(0.5, 3)
        cp = rng.uniform(0, 2 * np.pi)
        ch += np.sin(val * cf + cp)


def _to_uint8(arr, num_layers):
    """Normalise a summed-sine array to 0-255

    Args:
        arr (np.ndarray): Array with values in [-num_layers, num_layers]
        num_layers (int): Number of layers that were summed

    Returns:
        np.ndarray: Array of uint8 values in [0, 255]
    """
    return np.clip((arr / num_layers + 1) / 2 * 255, 0, 255).astype(np.uint8)


def make_plasma_jpeg(fname, width=2400, height=3300, quality=90):
    """Create a colour JPEG with plasma-style fractal noise

    Generates a deterministic image by summing six layers of sine-based
    noise with random frequencies and phase offsets.

    Args:
        fname (str): Output file path
        width (int): Image width in pixels
        height (int): Image height in pixels
        quality (int): JPEG quality 1-100
    """
    rng = random.Random(42)
    num_layers = 6

    # Coordinate grids scaled to [0, pi]
    xgrid, ygrid = np.meshgrid(
        np.linspace(0, np.pi, width, dtype=np.float32),
        np.linspace(0, np.pi, height, dtype=np.float32))

    channels = [np.zeros((height, width), dtype=np.float32) for _ in range(3)]
    for _ in range(num_layers):
        _plasma_layer(rng, xgrid, ygrid, channels)

    rgb = np.stack([_to_uint8(ch, num_layers) for ch in channels], axis=2)
    img = Image.fromarray(rgb, 'RGB')

    img.save(fname, 'JPEG', quality=quality)
    print(f'Wrote {fname} ({width}x{height}, quality {quality})')


def main():
    """Create all test files"""
    parser = argparse.ArgumentParser(description='Create test files')
    parser.add_argument('-o', '--output-dir', default=OUTPUT_DIR,
                        help='Output directory (default: test/files)')
    args = parser.parse_args()

    os.makedirs(args.output_dir, exist_ok=True)

    make_plasma_jpeg(os.path.join(args.output_dir, 'colour_plasma.jpg'))


if __name__ == '__main__':
    main()
