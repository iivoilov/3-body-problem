"""Browser QA for numbered worlds and absolute-epoch survey/detail playback."""
import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from io import BytesIO
import json
import os
from pathlib import Path
import shutil
import struct
import threading

from PIL import Image, ImageStat
from playwright.sync_api import sync_playwright


class QuietHandler(SimpleHTTPRequestHandler):
    def log_message(self,*_args):
        pass


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--screenshots',type=Path,default=Path('/tmp/orbit-catalog-check'))
    args=parser.parse_args()
    root=Path(__file__).resolve().parents[1]
    args.screenshots.mkdir(parents=True,exist_ok=True)
    server=ThreadingHTTPServer(('127.0.0.1',0),partial(QuietHandler,directory=str(root/'doc')))
    threading.Thread(target=server.serve_forever,daemon=True).start()
    base=f'http://127.0.0.1:{server.server_port}/'
    errors=[]
    try:
        with sync_playwright() as pw:
            browser=pw.chromium.launch(executable_path=os.environ.get('CHROMIUM_BIN') or shutil.which('chromium'),
                                      headless=True,args=['--no-sandbox','--use-angle=swiftshader','--enable-unsafe-swiftshader'])
            context=browser.new_context(viewport={'width':1440,'height':1100},device_scale_factor=1)
            def local(route):
                if route.request.url.startswith(base):
                    route.continue_()
                else:
                    errors.append('External request: '+route.request.url);route.abort()
            context.route('**/*',local)
            page=context.new_page()
            page.on('pageerror',lambda e: errors.append(str(e)))
            page.on('console',lambda m: errors.append(m.text) if m.type=='error' else None)
            def ready():
                page.locator('#world[data-ready="true"]').wait_for(timeout=30000)
                page.wait_for_function("document.getElementById('flux').textContent !== '—'")
            page.goto(base+'orbit-02.html?paused=1',wait_until='networkidle');ready()
            assert page.locator('#preset').input_value()=='02-01'
            page.locator('#preset').select_option('02-02');page.wait_for_load_state('networkidle');ready()
            assert page.locator('#mass-2').inner_text()=='1.0000'
            assert page.locator('#orbit-notes').get_attribute('href')=='orbits/Orbit-02-02.md'
            im=Image.open(BytesIO(page.locator('#world canvas').screenshot())).convert('RGB')
            assert max(ImageStat.Stat(im).stddev)>10,'canvas must contain a rendered scene'
            page.screenshot(path=str(args.screenshots/'02-02-surface.png'),full_page=True)
            page.locator('#record').select_option('4');page.wait_for_load_state('networkidle');ready()
            assert page.locator('#time').get_attribute('min')=='999936'
            assert float(page.locator('#year').inner_text())==999936
            page.locator('#remote').click()
            page.wait_for_function("document.getElementById('year').textContent === '1000000.000'")
            path=root/'doc/data/orbit-02-02/year-999936'
            manifest=json.loads((path/'manifest.json').read_text())
            expected=struct.unpack_from('<f',(path/'world.bin').read_bytes(),((manifest['frameCount']-1)*20+12)*4)[0]
            assert abs(float(page.locator('#flux').inner_text())-expected)<.0006
            page.locator('#restart').click()
            page.locator('#play').click()
            page.wait_for_function("Number(document.getElementById('year').textContent)>999936.01")
            page.locator('#play').click()
            page.locator('#record').select_option('5');page.wait_for_load_state('networkidle');ready()
            assert page.locator('#surface-view').is_disabled()
            assert page.locator('#trails').is_disabled()
            assert page.locator('#orbit-view').get_attribute('aria-pressed')=='true'
            assert 'snapshots' in page.locator('#record-caption').inner_text()
            page.locator('#remote').click()
            page.wait_for_function("document.getElementById('year').textContent === '1000000.000'")
            page.screenshot(path=str(args.screenshots/'02-02-survey.png'),full_page=True)
            page.locator('#preset').select_option('02-03');page.wait_for_load_state('networkidle');ready()
            assert page.locator('#mass-2').inner_text()=='0.1221'
            assert page.locator('#time').get_attribute('min')=='0'
            assert page.locator('#surface-view').is_enabled()
            page.locator('#surface-view').click()
            page.locator('#record').select_option('2');page.wait_for_load_state('networkidle');ready()
            assert float(page.locator('#year').inner_text())==500000
            page.set_viewport_size({'width':390,'height':844})
            page.wait_for_timeout(200)
            assert page.evaluate('document.documentElement.scrollWidth<=innerWidth'),'mobile catalogue must not overflow'
            page.screenshot(path=str(args.screenshots/'02-03-mobile.png'),full_page=True)
            browser.close()
        assert not errors,errors
        print('Catalogue browser passed: preset masses, notes, real WebGL pixels, epoch offsets, million-year endpoints, native flux, survey restrictions, playback, and mobile layout.')
    finally:
        server.shutdown();server.server_close()


if __name__=='__main__':
    main()
