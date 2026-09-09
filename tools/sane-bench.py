#!/usr/bin/env python3
"""Time a batch scan through SANE, side by side, with scanimage.

Works over USB (fujitsu backend) and over the network (finet backend), so
the two can be compared like for like: scanimage fetches each side and
writes it out, and this prints when each side completed and the intervals
between them. For a fujitsu device it turns on buffermode and, in colour,
JPEG compression, without which the fi-8950 stops after every sheet and
sends 25 MB per side over USB.

    tools/sane-bench.py                              # last device, 50 sides
    tools/sane-bench.py -d 'fujitsu:fi-8950:1933'    # over USB
    tools/sane-bench.py -d finet:192.168.4.98 -n 100
    tools/sane-bench.py -d ... -- --resolution 200   # extra scanimage options
"""
import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time


def main():
    p = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    p.add_argument('-d', '--device', help='SANE device (default: scanimage picks)')
    p.add_argument('-n', '--sides', type=int, default=50, help='sides to scan (default 50)')
    p.add_argument('-r', '--resolution', type=int, default=300)
    p.add_argument('-m', '--mode', default='Color')
    p.add_argument('--source', default='ADF Duplex')
    p.add_argument('--keep', metavar='DIR', help='keep the images in DIR')
    p.add_argument('--no-fast', action='store_true',
                   help='do not add buffermode/compression for a fujitsu device')
    p.add_argument('-v', '--verbose', action='store_true', help='print every side')
    p.add_argument('extra', nargs='*', help='further scanimage options after --')
    a = p.parse_args()

    outdir = a.keep or tempfile.mkdtemp(prefix='sane-bench-')
    os.makedirs(outdir, exist_ok=True)
    fujitsu = a.device is not None and a.device.startswith('fujitsu')
    jpeg = fujitsu and not a.no_fast and a.mode == 'Color'
    cmd = ['scanimage']
    if a.device:
        cmd += ['-d', a.device]
    cmd += ['--source', a.source, '--mode', a.mode, '--resolution', str(a.resolution)]
    if fujitsu and not a.no_fast:
        cmd += ['--buffermode=On']
        if jpeg:
            cmd += ['--compression=JPEG', '--format=jpeg']
    cmd += ['--batch=%s/side%%03d.%s' % (outdir, 'jpg' if jpeg else 'pnm'),
            '--batch-count=%d' % a.sides, '--batch-print'] + a.extra
    print(' '.join(cmd))

    t0 = time.time()
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            text=True, bufsize=1)
    done = []       # completion time of each side
    errors = []
    try:
        for line in proc.stdout:
            line = line.rstrip('\n')
            if line.startswith(outdir):
                done.append(time.time() - t0)
                size = os.path.getsize(line) if os.path.exists(line) else 0
                if a.verbose:
                    gap = done[-1] - done[-2] if len(done) > 1 else 0
                    print('%6.2fs side %3d  +%4.0f ms  %5.0f KB' % (
                        done[-1], len(done), gap * 1000, size / 1024))
                elif len(done) % 10 == 0:
                    print('%3d sides, %.0f ms/side so far' % (
                        len(done), 1000 * (done[-1] - done[0]) / (len(done) - 1)))
            elif line and not re.match(r'^(Scanning|Scanned|Batch|Output)', line):
                errors.append(line)
    except KeyboardInterrupt:
        proc.terminate()
        print('\ninterrupted')
    proc.wait()
    for e in errors[-5:]:
        print('scanimage:', e)

    if len(done) > 1:
        el = done[-1] - done[0]
        n = len(done) - 1
        gaps = sorted(done[i + 1] - done[i] for i in range(n))
        print('%d sides in %.1fs after the first: %.0f ms/side (%.0f sides/min); '
              'first side after %.1fs' % (len(done), el, 1000 * el / n,
                                          60 * n / el, done[0]))
        print('interval between sides: median %.0f ms, min %.0f, max %.0f' % (
            1000 * gaps[n // 2], 1000 * gaps[0], 1000 * gaps[-1]))
        print('images in %s' % outdir)
    if not a.keep and len(done) <= 1:
        shutil.rmtree(outdir, ignore_errors=True)


if __name__ == '__main__':
    main()
