#!/usr/bin/env python3
"""An HTTP proxy for testing the item editor's Download Online in xemu.

The xbox-skins.net replacement server has no game icons, so the icon path
never runs. Set UnleashX's proxy to this one (Config.xml:
<Settings><HTTP UseProxy="Yes" Proxy="10.0.2.2" Port="8888"/>, or Control
Panel > Network > HTTP Connection), and for www.xbox-skins.net it answers
  games/xml/<id>.xml     the real entry with Thumb="1" added (or a made-up one)
  games/thumbs/<id>.png  a generated 128x128 icon showing the title ID
Everything else is fetched from the internet as asked.

  fake_xboxskins_proxy.py [port]       (default 8888, listens on 127.0.0.1)

Needs Pillow for the icons.
"""
import http.server
import io
import re
import sys
import urllib.request

SITE = "www.xbox-skins.net"


def icon(title_id):
    from PIL import Image, ImageDraw, ImageFont
    image = Image.new("RGB", (128, 128), (30, 90, 40))
    draw = ImageDraw.Draw(image)
    draw.rectangle([3, 3, 124, 124], outline=(255, 255, 255), width=3)
    try:
        font = ImageFont.truetype("DejaVuSans-Bold.ttf", 18)
    except OSError:
        font = ImageFont.load_default()
    for y, text in ((34, "FAKE ICON"), (74, title_id.upper())):
        box = draw.textbbox((0, 0), text, font=font)
        draw.text(((128 - (box[2] - box[0])) / 2, y), text, font=font, fill=(255, 255, 255))
    out = io.BytesIO()
    image.save(out, "PNG")
    return out.getvalue()


class Proxy(http.server.BaseHTTPRequestHandler):
    def reply(self, status, body, content_type):
        self.send_response(status)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        # Through a proxy the request line has the whole URL.
        url = self.path if self.path.startswith("http://") else "http://" + self.headers.get("Host", "") + self.path
        match = re.match(r"http://([^/]+)(/.*)?$", url)
        host, path = (match.group(1), match.group(2) or "/") if match else ("", "/")
        if host.lower() == SITE:
            info = re.match(r"/games/xml/([0-9A-Fa-f]{8})\.xml$", path)
            thumb = re.match(r"/games/thumbs/([0-9A-Fa-f]{8})\.png$", path)
            if info:
                try:
                    body = urllib.request.urlopen(url, timeout=15).read().decode("utf-8", "replace")
                except Exception:
                    body = ""
                if "<xbg" not in body.lower():
                    body = '<gdbase><xbg title="Test" hexid="%s" video="0" vidid=""/></gdbase>' % info.group(1).upper()
                body = re.sub(r"<xbg ", '<xbg Thumb="1" ', body, count=1, flags=re.I)
                print("faked", path, flush=True)
                return self.reply(200, body.encode(), "text/xml")
            if thumb:
                print("faked", path, flush=True)
                return self.reply(200, icon(thumb.group(1)), "image/png")
        try:
            with urllib.request.urlopen(url, timeout=30) as upstream:
                body = upstream.read()
                status, content_type = upstream.status, upstream.headers.get("Content-Type", "application/octet-stream")
        except urllib.error.HTTPError as error:
            body, status, content_type = error.read(), error.code, "text/html"
        except Exception as error:
            body, status, content_type = str(error).encode(), 502, "text/plain"
        print(status, url, flush=True)
        self.reply(status, body, content_type)

    def log_message(self, *args):
        pass


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8888
    print("proxy on 127.0.0.1:%d (the Xbox sees it as 10.0.2.2:%d)" % (port, port), flush=True)
    http.server.ThreadingHTTPServer(("127.0.0.1", port), Proxy).serve_forever()
