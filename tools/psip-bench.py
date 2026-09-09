#!/usr/bin/env python3
"""Measure how fast an fi-8000 series scanner hands over images on the network.

This talks the scanner's HTTP/JSON protocol directly (see
doc/finet-protocol.md in the sane-backends tree), with nothing of the SANE
backend or paperman in the way: it starts a batch, fetches sides as fast as
it can without decoding them, and prints the timing of every side and a
summary. It always stops the feeder and closes the session, so it can be
interrupted with Ctrl-C.

    tools/psip-bench.py                       # 50 sides at 300 dpi colour duplex
    tools/psip-bench.py -n 100 -r 200         # 100 sides at 200 dpi
    tools/psip-bench.py --no-prepick          # let the feeder wait on delivery
    tools/psip-bench.py --save /tmp/sides     # keep the JPEGs
"""
import argparse
import http.client
import json
import sys
import time

TOKEN = 'Bearer ***CLIENTACCESSTOKEN***'   # the scanner's fixed token, not a secret
UNITS = 1200                               # the scanner measures in 1/1200 inch


class Scanner:
    def __init__(self, host):
        self.host = host
        self.conn = http.client.HTTPConnection(host, 80, timeout=90)
        self.sid = None
        self.capturing = False

    def call(self, method, params, timeout=60):
        body = json.dumps({'commandId': 'bench', 'commandTimeOut': str(timeout),
                           'method': method, 'parameters': params})
        self.conn.request('POST', '/api/privet/session', body=body,
                          headers={'Authorization': TOKEN,
                                   'Content-Type': 'application/json'})
        return json.loads(self.conn.getresponse().read())

    def get(self, uri):
        self.conn.request('GET', uri, headers={'Authorization': TOKEN})
        return self.conn.getresponse().read()

    def info(self):
        self.conn.request('GET', '/api/privet/info', headers={'Authorization': TOKEN})
        return json.loads(self.conn.getresponse().read())

    def open(self):
        r = self.call('createSession', {'connect': 'PSIP'})
        if r.get('status') != 'success':
            raise SystemExit('createSession: %s (a session is open: close it '
                             'or power-cycle the scanner)' % r.get('status'))
        self.sid = r['results']['session']['sessionId']

    def session(self):
        return self.call('getSession', {'operationCode': 1, 'sessionId': self.sid},
                         5)['results']['session']

    def close(self):
        if not self.sid:
            return
        if self.capturing:
            self.call('stopCapturing', {'pauseScanning': 'false', 'sessionId': self.sid}, 5)
            self.capturing = False
        self.call('closeSession', {'sessionId': self.sid}, 5)
        self.sid = None


def attrs(pairs):
    return {'attributes': [{'attribute': k, 'values': {'value': str(v)}}
                           for k, v in pairs]}


def make_task(a):
    """The task the finet backend sends, with the options this tool varies"""
    width = int(a.width * UNITS)
    height = int(a.height * UNITS)
    read = {
        'imageCacheMode': attrs([('imageCacheMode', 'scannerMemory')]),
        'imageTransferMethod': attrs([('imageTransferMethod', 'alternate')]),
    }
    if a.divided:
        read['devidedSize'] = attrs([('devidedSize', '12')])
    return {'actions': {'streams': {'sources': {
        'feedControls': {
            'numberOfSheets': attrs([('sheetCounts', '0')]),
            'doubleFeed': attrs([('overlap', 'enable'), ('length', 'disable'),
                                 ('response', 'recovery'),
                                 ('deviceSpecification', 'disable'),
                                 ('iOMFLength', '0')]),
            'background': attrs([('bgColor', a.background)]),
            'prePick': attrs([('prePickControl',
                               'enable' if a.prepick else 'disable')]),
        },
        'pixelFormats': attrs([
            ('resolution', a.resolution), ('width', width), ('height', height),
            ('offsetWidth', 0), ('offsetHeight', 0),
            ('automaticSize', 'disable'), ('compression', 'jpeg'),
            ('jpegQuality', a.quality), ('jpgSubSampling', '422'),
            ('overscan', 'off'), ('automaticDeskew', 'disable'),
            ('endOfPageDetection', 'on' if a.end_of_page else 'off'),
            ('paperWidth', width), ('paperLength', height)]),
        'readControls': read,
    }}}}


def main():
    p = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    p.add_argument('host', nargs='?', default='192.168.4.98', help='scanner address')
    p.add_argument('-n', '--sides', type=int, default=50, help='sides to fetch (default 50)')
    p.add_argument('-r', '--resolution', type=int, default=300)
    p.add_argument('-q', '--quality', type=int, default=80, help='JPEG quality')
    p.add_argument('--width', type=float, default=12.0, help='window width in inches')
    p.add_argument('--height', type=float, default=17.0, help='window height in inches')
    p.add_argument('--background', default='black', choices=['black', 'white'])
    p.add_argument('--no-prepick', dest='prepick', action='store_false',
                   help='let the feeder wait on delivery instead of running ahead')
    p.add_argument('--no-divided', dest='divided', action='store_false',
                   help='fetch without devidedSize/imagePartNum')
    p.add_argument('--end-of-page', action='store_true',
                   help='endOfPageDetection on')
    p.add_argument('--save', metavar='DIR', help='save the JPEGs in DIR')
    p.add_argument('-v', '--verbose', action='store_true', help='print every side')
    a = p.parse_args()

    s = Scanner(a.host)
    inf = s.info()
    print('%s %s, state %s' % (inf.get('model'), inf.get('serialNumber'),
                               inf.get('deviceState')))
    s.open()
    try:
        st = s.session()
        if st['deviceStatus']['sensor']['hopperEmpty'] == 'on':
            raise SystemExit('the hopper is empty')
        s.call('sendTask', {'sessionId': s.sid, 'task': make_task(a)}, 5)
        r = s.call('startCapturing', {'ignore_mf_detection': 'false',
                                      'restartCapturing': 'false',
                                      'sessionId': s.sid}, 35)
        if r['results']['session'].get('detected') != 'success':
            raise SystemExit('startCapturing: %s' % r['results']['session'].get('detected'))
        s.capturing = True

        t0 = time.time()
        block = 1
        sides = []          # (t_done, wait, fetch, size)
        first = None
        waiting_max = 0
        while len(sides) < a.sides:
            params = {'duplexMetadata': 'disable', 'imageBlockNum': block,
                      'sessionId': s.sid, 'withMetadata': 'enable'}
            if a.divided:
                params['imagePartNum'] = 1
            t = time.time()
            r = s.call('readImageBlock', params, 60)
            wait = time.time() - t
            if r.get('status') == 'noImage':
                st = s.session()
                if st['deviceStatus']['sensor']['hopperEmpty'] == 'on' and not st['imageBlocks']:
                    print('hopper empty after %d sides' % len(sides))
                    break
                continue
            md = r['results']['metadata']
            uri = md['address']['uri']
            t = time.time()
            data = s.get(uri)
            fetch = time.time() - t
            if first is None:
                first = time.time()
            s.call('releaseImageBlocks', {'imageBlockNum': block, 'sessionId': s.sid}, 60)
            sides.append((time.time() - t0, wait, fetch, len(data)))
            if a.save:
                open('%s/side-%03d.jpg' % (a.save, block), 'wb').write(data)
            if a.verbose:
                print('%6.2fs side %3d %-5s wait %4.0f ms, fetch %3.0f ms, %4.0f KB' % (
                    sides[-1][0], block, md['address'].get('source', ''),
                    wait * 1000, fetch * 1000, len(data) / 1024))
            elif len(sides) % 10 == 0:
                el = time.time() - first
                print('%3d sides, %.0f ms/side so far' % (len(sides), 1000 * el / len(sides)))
            block += 1
            if block % 10 == 0:
                waiting_max = max(waiting_max, len(s.session()['imageBlocks']))

        # stop the feeder but keep what has gone through, and count it
        s.call('stopCapturing', {'pauseScanning': 'true', 'sessionId': s.sid}, 5)
        time.sleep(1.5)
        left = len(s.session()['imageBlocks'])

        if len(sides) > 1:
            el = sides[-1][0] - sides[0][0]
            n = len(sides) - 1
            fronts = [w for i, (_, w, _, _) in enumerate(sides) if i % 2 == 0]
            backs = [w for i, (_, w, _, _) in enumerate(sides) if i % 2 == 1]
            print('%d sides in %.1fs: %.0f ms/side (%.0f sides/min); first side after %.1fs' % (
                len(sides), el, 1000 * el / n, 60000 * n / (el * 1000), sides[0][0]))
            print('readImageBlock wait: fronts %.0f ms, backs %.0f ms; GET %.0f ms; %.0f KB/side' % (
                1000 * sum(fronts) / len(fronts), 1000 * sum(backs) / max(len(backs), 1),
                1000 * sum(f for _, _, f, _ in sides) / len(sides),
                sum(z for _, _, _, z in sides) / len(sides) / 1024))
            print('images waiting in the scanner: up to %d during the run, %d still '
                  'buffered when the feeder was stopped' % (waiting_max, left))
    finally:
        s.close()


if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        print('\ninterrupted')
        sys.exit(1)
