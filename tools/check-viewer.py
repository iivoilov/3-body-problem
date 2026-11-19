"""Optional browser QA: requires Python Playwright, Pillow, and a Chromium installation."""
import argparse
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from io import BytesIO
import os
from pathlib import Path
import shutil
import struct
import threading

from PIL import Image, ImageChops
from playwright.sync_api import sync_playwright


class QuietHandler(SimpleHTTPRequestHandler):
    def log_message(self, *_args):
        pass


def changed(before, after):
    a = Image.open(BytesIO(before)).convert("RGB")
    b = Image.open(BytesIO(after)).convert("RGB")
    return ImageChops.difference(a, b).getbbox() is not None


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--screenshots", type=Path)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    server = ThreadingHTTPServer(("127.0.0.1", 0), partial(QuietHandler, directory=str(root / "doc")))
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    executable = os.environ.get("CHROMIUM_BIN") or shutil.which("chromium") or shutil.which("google-chrome")
    errors = []
    external = []
    try:
        with sync_playwright() as playwright:
            browser = playwright.chromium.launch(executable_path=executable, headless=True,
                args=["--no-sandbox", "--use-angle=swiftshader", "--enable-unsafe-swiftshader"])
            page = browser.new_page(viewport={"width": 1360, "height": 1000}, device_scale_factor=1)
            page.on("pageerror", lambda error: errors.append(str(error)))
            page.on("console", lambda message: errors.append(message.text) if message.type == "error" else None)

            def local_only(route):
                if route.request.url.startswith(f"http://127.0.0.1:{server.server_port}/"):
                    route.continue_()
                else:
                    external.append(route.request.url)
                    route.abort()

            page.route("**/*", local_only)
            page.goto(f"http://127.0.0.1:{server.server_port}/", wait_until="networkidle")
            page.locator('#scene[data-ready="true"]').wait_for(timeout=20000)
            page.wait_for_function("document.getElementById('ratio').textContent !== '—'")
            page.locator("#play").click()
            page.wait_for_timeout(150)
            frozen = page.locator("#time").input_value()
            page.wait_for_timeout(150)
            assert page.locator("#time").input_value() == frozen, "pause did not freeze replay"
            canvas = page.locator("#scene canvas")
            pixels = Image.open(BytesIO(canvas.screenshot())).convert("RGB")
            warm = sum(r > 140 and r > b * 2 and g > 40 for r, g, b in pixels.getdata())
            assert warm > 100, "WebGL canvas has no rendered sun"
            page.locator("#time").evaluate("element => { element.value = '512'; element.dispatchEvent(new Event('input', {bubbles:true})); }")
            page.wait_for_function("document.getElementById('time-label').textContent.startsWith('512.0')")
            circular = (root / "doc/data/circular.bin").read_bytes()
            expected = struct.unpack_from("<f", circular, (int(512 / 0.125) * 14 + 9) * 4)[0]
            assert abs(float(page.locator("#ratio").inner_text()) - expected) < 1e-4, "scrubbed metric differs from native export"
            page.locator("#preset").select_option("resonance")
            page.wait_for_function("!document.getElementById('preset').disabled && document.getElementById('preset-description').textContent.startsWith('Chosen')")
            page.wait_for_timeout(100)
            assert float(page.locator("#azure-e").inner_text().split()[1]) > 0.1, "preset switch did not update trajectory"
            with_trails = canvas.screenshot()
            page.locator("#trails").uncheck()
            page.wait_for_timeout(100)
            assert changed(with_trails, canvas.screenshot()), "trail toggle did not change the rendered scene"
            page.locator("#trails").check()
            page.locator("#shadows").uncheck()
            page.locator("#shadows").check()
            before_camera = canvas.screenshot()
            box = canvas.bounding_box()
            page.mouse.move(box["x"] + box["width"] * 0.5, box["y"] + box["height"] * 0.5)
            page.mouse.down()
            page.mouse.move(box["x"] + box["width"] * 0.65, box["y"] + box["height"] * 0.58, steps=12)
            page.mouse.up()
            page.wait_for_timeout(200)
            assert changed(before_camera, canvas.screenshot()), "camera drag did not change the view"
            page.locator("#speed").select_option("4")
            page.locator("#play").click()
            page.wait_for_function("Number(document.getElementById('time').value) > 513")
            assert "16 simulated" in page.locator("#status").inner_text(), "playback speed does not use native base rate"
            page.locator("#play").click()
            page.locator("#restart").click()
            page.wait_for_function("document.getElementById('time-label').textContent.startsWith('0.0')")
            page.locator("#time").evaluate("element => { element.value = '1000'; element.dispatchEvent(new Event('input', {bubbles:true})); }")
            page.wait_for_function("document.getElementById('time-label').textContent.startsWith('1000.0')")
            if args.screenshots:
                args.screenshots.mkdir(parents=True, exist_ok=True)
                page.screenshot(path=str(args.screenshots / "webgl-desktop.png"), full_page=True)
            page.set_viewport_size({"width": 390, "height": 844})
            page.wait_for_timeout(200)
            assert page.evaluate("document.documentElement.scrollWidth <= window.innerWidth + 1"), "mobile page overflows horizontally"
            if args.screenshots:
                page.screenshot(path=str(args.screenshots / "webgl-mobile.png"), full_page=True)
            assert not errors, "Browser errors: " + "\n".join(errors)
            assert not external, "Viewer tried to use external resources: " + str(external)
            browser.close()
        print("Browser QA passed: local-only resources, WebGL pixels, pause, native-data scrubbing, presets, trails, shadows, camera, speed, restart, responsive layout.")
    finally:
        server.shutdown()
        server.server_close()


if __name__ == "__main__":
    main()
