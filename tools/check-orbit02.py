"""Optional local browser QA for Orbit / 02: Playwright, Pillow, Chromium."""
import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from io import BytesIO
import os
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import threading

from PIL import Image, ImageChops, ImageFilter
from playwright.sync_api import sync_playwright


class QuietHandler(SimpleHTTPRequestHandler):
    def log_message(self, *_args):
        pass


def changed(before, after):
    return ImageChops.difference(Image.open(BytesIO(before)).convert("RGB"),
                                Image.open(BytesIO(after)).convert("RGB")).getbbox() is not None


def sun_count(screenshot):
    pixels = Image.open(BytesIO(screenshot)).convert("RGB")
    width, height = pixels.size
    # Locator screenshots include overlaid view buttons; exclude that header
    # so its bright text cannot be counted as a sun.
    points = {(x, y) for y in range(80, int(height * 0.55)) for x in range(width)
              if (lambda c: c[0] > 195 and c[1] > 75 and c[2] > 40)(pixels.getpixel((x, y)))}
    components = 0
    while points:
        start = points.pop()
        pending = [start]
        size = 0
        while pending:
            x, y = pending.pop()
            size += 1
            for neighbor in ((x-1, y), (x+1, y), (x, y-1), (x, y+1)):
                if neighbor in points:
                    points.remove(neighbor)
                    pending.append(neighbor)
        if size >= 3:
            components += 1
    return components


def assert_smooth_sky(image, crop):
    # Median filtering removes isolated stars and film grain while retaining
    # the long cell-boundary seams seen in the original year-12.865 sky.
    sky = image.convert("RGB").crop(crop).filter(ImageFilter.MedianFilter(3))
    adjacent = ImageChops.difference(sky.crop((3, 0, sky.width, sky.height)),
                                    sky.crop((0, 0, sky.width-3, sky.height)))
    edges = sum(max(pixel) > 3 for pixel in adjacent.getdata())
    assert edges <= 20, f"night sky has {edges} abrupt edges; procedural noise must be continuous"


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--screenshots", type=Path)
    parser.add_argument("--native", type=Path, help="Also check this native executable's night sky")
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    if args.native:
        with tempfile.TemporaryDirectory(prefix="orbit02-night-") as temporary:
            capture = Path(temporary) / "night.ppm"
            subprocess.run([str(args.native.resolve()), "--orbit", "02", "--time", "12.865", "--capture", str(capture)],
                           check=True, capture_output=True, text=True)
            image = Image.open(capture)
            width, height = image.size
            assert_smooth_sky(image, (50, int(height*0.20), width-50, int(height*0.32)))
            if args.screenshots:
                args.screenshots.mkdir(parents=True, exist_ok=True)
                image.save(args.screenshots / "orbit-02-native-night.png")
        print("Orbit / 02 native night-sky regression passed at year 12.865.")
    server = ThreadingHTTPServer(("127.0.0.1", 0), partial(QuietHandler, directory=str(root / "doc")))
    threading.Thread(target=server.serve_forever, daemon=True).start()
    errors, external = [], []
    base = f"http://127.0.0.1:{server.server_port}/"
    try:
        with sync_playwright() as playwright:
            browser = playwright.chromium.launch(executable_path=os.environ.get("CHROMIUM_BIN") or shutil.which("chromium"),
                headless=True, args=["--no-sandbox", "--use-angle=swiftshader", "--enable-unsafe-swiftshader"])
            context = browser.new_context(viewport={"width": 1440, "height": 1100}, device_scale_factor=1)

            def local_only(route):
                if route.request.url.startswith(base):
                    route.continue_()
                else:
                    external.append(route.request.url)
                    route.abort()

            context.route("**/*", local_only)
            page = context.new_page()
            page.on("pageerror", lambda error: errors.append(str(error)))
            page.on("console", lambda message: errors.append(message.text) if message.type == "error" else None)
            page.goto(base + "orbit-02.html?paused=1", wait_until="networkidle")
            page.locator('#world[data-ready="true"]').wait_for(timeout=30000)
            page.wait_for_function("document.getElementById('flux').textContent !== '—'")
            canvas = page.locator("#world canvas")
            first = canvas.screenshot()
            assert sun_count(first) >= 3, "initial desktop sky must render three separate suns"
            assert page.locator("#play").inner_text() == "Play"
            year = page.locator("#time").input_value()
            page.wait_for_timeout(200)
            assert page.locator("#time").input_value() == year, "pause must freeze time"
            if args.screenshots:
                args.screenshots.mkdir(parents=True, exist_ok=True)
                page.screenshot(path=str(args.screenshots / "orbit-02-desktop.png"), full_page=True)

            page.locator("#shadows").uncheck()
            page.wait_for_timeout(100)
            assert changed(first, canvas.screenshot()), "terrain shadow toggle must change pixels"
            page.locator("#shadows").check()
            page.locator("#time").evaluate("el => {el.value='1.25';el.dispatchEvent(new Event('input',{bubbles:true}));}")
            page.wait_for_function("document.getElementById('year').textContent === '1.250'")
            data = (root / "doc/data/orbit-02/world.bin").read_bytes()
            expected = struct.unpack_from("<f", data, (int(1.25*512)*20+12)*4)[0]
            assert abs(float(page.locator("#flux").inner_text())-expected) < 0.0006, "scrubbed light must match C++ export"
            page.locator("#orbit-view").click()
            page.wait_for_timeout(200)
            assert page.locator("#orbit-view").get_attribute("aria-pressed") == "true"
            orbital = canvas.screenshot()
            assert changed(first, orbital), "orbital view must render a different scene"
            page.locator("#trails").uncheck()
            page.wait_for_timeout(100)
            assert changed(orbital, canvas.screenshot()), "orbital trails must affect pixels"
            page.locator("#trails").check()
            box = canvas.bounding_box()
            before = canvas.screenshot()
            page.mouse.move(box["x"]+box["width"]*0.5, box["y"]+box["height"]*0.5)
            page.mouse.down()
            page.mouse.move(box["x"]+box["width"]*0.6, box["y"]+box["height"]*0.6, steps=10)
            page.mouse.up()
            page.wait_for_timeout(200)
            assert changed(before, canvas.screenshot()), "orbital camera drag must change pixels"
            if args.screenshots:
                page.screenshot(path=str(args.screenshots / "orbit-02-orbital.png"), full_page=True)

            page.locator("#encounter").click()
            page.wait_for_function("Number(document.getElementById('separation').textContent) < 0.2")
            page.locator("#brightest").click()
            page.wait_for_function("Number(document.getElementById('flux').textContent) > 20")
            page.locator("#remote").click()
            page.wait_for_function("Number(document.getElementById('nearest').textContent) > 10")
            page.locator("#first-dawn").click()
            page.locator("#surface-view").click()
            page.locator("#restart").click()
            page.wait_for_function("document.getElementById('year').textContent === '0.000'")
            page.locator("#speed").select_option("4")
            page.locator("#play").click()
            page.wait_for_function("Number(document.getElementById('time').value) > 0.02")
            page.locator("#play").click()
            page.locator("#restart").click()

            page.set_viewport_size({"width": 390, "height": 844})
            page.wait_for_timeout(200)
            assert page.evaluate("document.documentElement.scrollWidth <= innerWidth"), "mobile page must not overflow horizontally"
            assert sun_count(canvas.screenshot()) >= 3, "mobile field of view must show all three initial suns"
            before = canvas.screenshot()
            box = canvas.bounding_box()
            page.mouse.move(box["x"]+box["width"]*0.5, box["y"]+box["height"]*0.5)
            page.mouse.down()
            page.mouse.move(box["x"]+box["width"]*0.7, box["y"]+box["height"]*0.55, steps=10)
            page.mouse.up()
            page.wait_for_timeout(200)
            assert changed(before, canvas.screenshot()), "surface camera drag must change the sky"
            page.locator("#restart").click()
            if args.screenshots:
                page.screenshot(path=str(args.screenshots / "orbit-02-mobile.png"), full_page=True)
            page.set_viewport_size({"width": 1440, "height": 1100})
            page.locator("#time").evaluate("el => {el.value='12.865';el.dispatchEvent(new Event('input',{bubbles:true}));}")
            page.wait_for_function("Number(document.getElementById('year').textContent) > 12.86")
            image = Image.open(BytesIO(canvas.screenshot()))
            width, height = image.size
            assert_smooth_sky(image, (30, int(height*0.14), width-30, int(height*0.26)))
            if args.screenshots:
                page.screenshot(path=str(args.screenshots / "orbit-02-browser-night.png"), full_page=True)
            context.close()

            reduced = browser.new_context(reduced_motion="reduce")
            reduced.route("**/*", local_only)
            page = reduced.new_page()
            page.goto(base+"orbit-02.html", wait_until="networkidle")
            page.locator('#world[data-ready="true"]').wait_for(timeout=30000)
            assert page.locator("#play").inner_text() == "Play", "reduced motion must start paused"
            browser.close()
        assert not errors, errors
        assert not external, external
        print("Orbit / 02 browser passed: three-sun pixels on desktop/mobile, terrain shadows, native-data scrubbing, both views, trails, camera input, playback, jumps, reduced motion, local-only resources.")
    finally:
        server.shutdown()
        server.server_close()


if __name__ == "__main__":
    main()
