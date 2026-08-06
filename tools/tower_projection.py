#!/usr/bin/env python3
"""
tools/tower_projection.py
Genera una proyección 3D cilíndrica → 2D y escribe un SVG de ejemplo.

Uso: python tools/tower_projection.py --out tower_projection.svg
"""

import math
import argparse
import sys

def make_perspective(fov_deg, aspect, near, far):
    f = 1.0 / math.tan(math.radians(fov_deg) / 2.0)
    A = (far + near) / (near - far)
    B = (2 * far * near) / (near - far)
    # 4x4 matrix as list of lists
    return [
        [f / aspect, 0, 0, 0],
        [0, f, 0, 0],
        [0, 0, A, B],
        [0, 0, -1, 0]
    ]

def normalize(v):
    l = math.sqrt(sum([c*c for c in v]))
    if l == 0: return v
    return [c / l for c in v]

def make_lookat(eye, center, up):
    f = normalize([center[i] - eye[i] for i in range(3)])
    u = normalize(up)
    s = [f[1]*u[2] - f[2]*u[1], f[2]*u[0] - f[0]*u[2], f[0]*u[1] - f[1]*u[0]]
    # Build 4x4 view matrix (row-major)
    M = [
        [ s[0],  s[1],  s[2], 0],
        [ u[0],  u[1],  u[2], 0],
        [-f[0], -f[1], -f[2], 0],
        [   0 ,    0 ,    0 , 1]
    ]
    # Translate
    T = [
        [1,0,0,-eye[0]],
        [0,1,0,-eye[1]],
        [0,0,1,-eye[2]],
        [0,0,0,1]
    ]
    return mat4_mul(M, T)

def mat4_mul(a, b):
    res = [[0]*4 for _ in range(4)]
    for i in range(4):
        for j in range(4):
            s = 0
            for k in range(4):
                s += a[i][k] * b[k][j]
            res[i][j] = s
    return res

def mat4_vec_mul(m, v4):
    res = [0,0,0,0]
    for i in range(4):
        res[i] = m[i][0]*v4[0] + m[i][1]*v4[1] + m[i][2]*v4[2] + m[i][3]*v4[3]
    return res

def project_point(P, V, M, x3d, y3d, z3d, width, height):
    v = [x3d, y3d, z3d, 1]
    mv = mat4_vec_mul(M, v)
    mvv = mat4_vec_mul(V, mv)
    pv = mat4_vec_mul(P, mvv)
    if pv[3] == 0:
        return None
    ndc = [pv[0]/pv[3], pv[1]/pv[3], pv[2]/pv[3]]
    # Map NDC [-1,1] to screen coords
    x = (ndc[0] * 0.5 + 0.5) * width
    y = (1 - (ndc[1] * 0.5 + 0.5)) * height
    return (x, y, ndc[2])

def generate_cylinder_points(rho, rings, samples_per_ring, height):
    points = []
    for ri in range(rings):
        z = -height/2 + (height * ri) / max(1, rings-1)
        for s in range(samples_per_ring):
            phi = (2*math.pi) * (s / samples_per_ring)
            x = rho * math.cos(phi)
            y = rho * math.sin(phi)
            points.append((x, y, z, rho, phi))
    return points

def write_svg(filename, coords, width, height):
    with open(filename, 'w', encoding='utf-8') as f:
        f.write('<?xml version="1.0" encoding="UTF-8"?>\n')
        f.write(f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}">\n')
        # background
        f.write(f'<rect width="100%" height="100%" fill="#0b1020"/>\n')
            # draw points
            for (x,y,depth) in coords:
                f.write(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="2" fill="#66ffcc"/>\n')
        f.write('</svg>\n')

def main():
    parser = argparse.ArgumentParser(description='Tower projection demo (cylindrical -> 2D)')
    parser.add_argument('--out', default='tower_projection.svg')
    parser.add_argument('--width', type=int, default=800)
    parser.add_argument('--height', type=int, default=600)
    parser.add_argument('--rings', type=int, default=6)
    parser.add_argument('--samples', type=int, default=48)
    parser.add_argument('--rho', type=float, default=2.0)
    args = parser.parse_args()

    width = args.width
    height = args.height

    P = make_perspective(60.0, float(width)/height, 0.1, 100.0)
    eye = [0.0, -6.0, 1.5]
    center = [0.0, 0.0, 0.5]
    up = [0.0, 0.0, 1.0]
    V = make_lookat(eye, center, up)
    M = [[1,0,0,0],[0,1,0,0],[0,0,1,0],[0,0,0,1]]

    points = generate_cylinder_points(args.rho, args.rings, args.samples, 4.0)
    screen_coords = []
    for (x3d,y3d,z3d,rho,phi) in points:
        p = project_point(P, V, M, x3d, y3d, z3d, width, height)
        if p:
            x2,y2,depth = p
            # Use depth to decide stacking; keep values simple
            screen_coords.append((x2,y2,depth))

    # Sort by depth (far -> near)
    screen_coords.sort(key=lambda t: t[2], reverse=True)
    write_svg(args.out, screen_coords, width, height)
    print(f"Wrote {args.out} ({len(screen_coords)} points)")

if __name__ == '__main__':
    main()
