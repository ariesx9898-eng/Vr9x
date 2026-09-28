"""Vectorised 2-D gradient noise (Perlin-style, quintic fade) and fractal combinations, numpy only.

All functions take coordinates in metres and a wavelength in metres, so results do not depend on grid resolution.
Every octave is rotated and offset so lattice alignment and the 4096-cell hash period never show.
"""
import numpy as np

_MASK = 4095


class Noise2D:
    def __init__(self, seed):
        rng = np.random.default_rng(seed)
        self.perm = rng.permutation(_MASK + 1).astype(np.int32)
        ang = rng.random(_MASK + 1) * 2.0 * np.pi
        self.gx = np.cos(ang).astype(np.float32)
        self.gy = np.sin(ang).astype(np.float32)
        self.rng = rng
        # per-octave rotation / offset tables (deterministic)
        self.oct_ang = rng.random(32) * 2.0 * np.pi
        self.oct_off = rng.random((32, 2)).astype(np.float64) * 4096.0

    def raw(self, x, y):
        """Gradient noise at lattice coordinates (x, y) (float32 arrays), result roughly in [-0.7, 0.7]."""
        xf0 = np.floor(x)
        yf0 = np.floor(y)
        fx = (x - xf0).astype(np.float32)
        fy = (y - yf0).astype(np.float32)
        xi = xf0.astype(np.int64) & _MASK
        yi = yf0.astype(np.int64) & _MASK
        perm = self.perm
        px0 = perm[xi]
        px1 = perm[(xi + 1) & _MASK]
        yi1 = (yi + 1) & _MASK
        h00 = perm[(px0 + yi) & _MASK]
        h01 = perm[(px0 + yi1) & _MASK]
        h10 = perm[(px1 + yi) & _MASK]
        h11 = perm[(px1 + yi1) & _MASK]
        gx, gy = self.gx, self.gy
        fx1 = fx - 1.0
        fy1 = fy - 1.0
        n00 = gx[h00] * fx + gy[h00] * fy
        n10 = gx[h10] * fx1 + gy[h10] * fy
        n01 = gx[h01] * fx + gy[h01] * fy1
        n11 = gx[h11] * fx1 + gy[h11] * fy1
        u = fx * fx * fx * (fx * (fx * 6.0 - 15.0) + 10.0)
        v = fy * fy * fy * (fy * (fy * 6.0 - 15.0) + 10.0)
        nx0 = n00 + u * (n10 - n00)
        nx1 = n01 + u * (n11 - n01)
        return (nx0 + v * (nx1 - nx0)) * 1.4142

    def octave(self, x, y, wavelength, k):
        """One rotated/offset octave; x, y in metres."""
        a = self.oct_ang[k % 32]
        c, s = np.cos(a), np.sin(a)
        f = 1.0 / wavelength
        ox, oy = self.oct_off[k % 32]
        xx = (x * c - y * s) * f + ox
        yy = (x * s + y * c) * f + oy
        return self.raw(xx.astype(np.float64), yy.astype(np.float64))

    def fbm(self, x, y, wavelength, octaves=6, gain=0.5, lacunarity=2.0, k0=0):
        amp, total, norm = 1.0, 0.0, 0.0
        wl = wavelength
        for i in range(octaves):
            total = total + amp * self.octave(x, y, wl, k0 + i)
            norm += amp
            amp *= gain
            wl /= lacunarity
        return (total / norm).astype(np.float32)

    def ridged(self, x, y, wavelength, octaves=6, gain=2.0, lacunarity=2.05, offset=1.0, h=0.95, k0=0):
        """Musgrave ridged multifractal, roughly in [0, 1]."""
        total = np.zeros(np.shape(x), np.float32)
        weight = np.ones(np.shape(x), np.float32)
        wl = wavelength
        norm = 0.0
        for i in range(octaves):
            sig = offset - np.abs(self.octave(x, y, wl, k0 + i))
            sig = sig * sig * weight
            weight = np.clip(sig * gain, 0.0, 1.0)
            a = (lacunarity ** (-h * i))
            total += sig * a
            norm += a
            wl /= lacunarity
        return total / norm

    def billow(self, x, y, wavelength, octaves=5, gain=0.5, k0=0):
        amp, total, norm = 1.0, 0.0, 0.0
        wl = wavelength
        for i in range(octaves):
            total = total + amp * (np.abs(self.octave(x, y, wl, k0 + i)) * 2.0 - 0.5)
            norm += amp
            amp *= gain
            wl /= 2.0
        return (total / norm).astype(np.float32)

    def warp(self, x, y, wavelength, amp, octaves=4, k0=0):
        """Domain warp: returns displaced (x, y)."""
        wx = self.fbm(x, y, wavelength, octaves, k0=k0)
        wy = self.fbm(x + 5371.3, y - 2917.9, wavelength, octaves, k0=k0 + 7)
        return x + amp * wx, y + amp * wy
