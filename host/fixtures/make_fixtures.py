#!/usr/bin/env python3
# KamTape PS5 - writes the made-up API answers and thumbnails the PC build
# shows (host/http_host.cpp). Nothing here comes from KamTape: invented videos
# in the API's XML layout, and abstract pictures drawn below.
# SPDX-License-Identifier: GPL-3.0-or-later
#
# usage: python3 host/fixtures/make_fixtures.py

import random
from pathlib import Path
from xml.sax.saxutils import escape

from PIL import Image, ImageDraw, ImageFilter

HERE = Path(__file__).resolve().parent
rng = random.Random(2005)

VIDEOS = [
    ("vhs01aaaaaa", "tapehead88", "Rainy Day Skate Session", 132, "skate rain friends music"),
    ("vhs02bbbbbb", "lunchbox", "My Cat Discovers the Printer", 47, "cats funny pets"),
    ("vhs03cccccc", "garagetones", "Basement Band Practice (Take 3)", 241, "music band garage rock"),
    ("vhs04dddddd", "pixelpam", "Drawing a Robot in MS Paint", 318, "art drawing computer"),
    ("vhs05eeeeee", "tapehead88", "Road Trip to the Big Lake", 506, "travel lake summer road"),
    ("vhs06ffffff", "kitchenkev", "Microwave S'mores Experiment", 88, "food science funny"),
    ("vhs07gggggg", "dvdlad", "Trying Every Button on the Old Remote", 173, "funny tv retro"),
    ("vhs08hhhhhh", "garagetones", "Synth Jam on a Toy Keyboard", 95, "music synth toys"),
    ("vhs09iiiiii", "birdwatcher", "Pigeons Having a Meeting", 61, "birds animals funny"),
    ("vhs10jjjjjj", "pixelpam", "Stop Motion: Clay Dinosaur", 74, "art animation clay"),
    ("vhs11kkkkkk", "snowdayz", "First Snow of the Year", 39, "snow winter weather"),
    ("vhs12llllll", "lunchbox", "Dog vs. Sprinkler", 52, "dogs funny pets summer"),
]

PALETTES = [
    [(255, 112, 67), (255, 202, 40), (38, 50, 56)],
    [(102, 187, 106), (255, 241, 118), (27, 94, 32)],
    [(66, 165, 245), (171, 71, 188), (13, 71, 161)],
    [(255, 167, 38), (239, 83, 80), (62, 39, 35)],
    [(38, 198, 218), (255, 255, 255), (0, 96, 100)],
    [(236, 64, 122), (255, 213, 79), (74, 20, 140)],
]


def thumb(path, seed):
    r = random.Random(seed)
    a, b, c = PALETTES[seed % len(PALETTES)]
    w, h = 320, 240
    im = Image.new("RGB", (w, h))
    d = ImageDraw.Draw(im)
    for y in range(h):
        t = y / h
        d.line([(0, y), (w, y)], fill=tuple(int(a[i] * (1 - t) + c[i] * t) for i in range(3)))
    for _ in range(6):
        x0, y0 = r.randint(-60, w), r.randint(-40, h)
        s = r.randint(30, 120)
        d.ellipse([x0, y0, x0 + s, y0 + s], fill=b if r.random() < 0.5 else a)
    d.rectangle([0, int(h * 0.72), w, h], fill=c)
    im = im.filter(ImageFilter.GaussianBlur(1.2))
    im.save(path, quality=82)


def video_xml(v, i):
    vid, author, title, length, tags = v
    return f"""    <video>
        <author>{escape(author)}</author>
        <id>{vid}</id>
        <title>{escape(title)}</title>
        <length_seconds>{length}</length_seconds>
        <rating_avg>{3 + (i * 37 % 20) / 10:.2f}</rating_avg>
        <rating_count>{5 + i * 3}</rating_count>
        <description>{escape(title)}. Filmed on a camcorder and uploaded the same night.</description>
        <view_count>{120 + i * 37}</view_count>
        <upload_time>{1121398533 + i * 86400 * 9}</upload_time>
        <comment_count>{i % 4}</comment_count>
        <tags>{escape(tags)}</tags>
        <url>http://www.kamtape.com/watch?v={vid}</url>
        <thumbnail_url>http://www.kamtape.com/get_still?video_id={vid}</thumbnail_url>
    </video>
"""


def video_list(videos):
    body = "".join(video_xml(v, VIDEOS.index(v)) for v in videos)
    return f'<?xml version="1.0" encoding="utf-8"?>\n<ut_response status="ok">\n<video_list>\n{body}</video_list>\n</ut_response>\n'


(HERE / "featured.xml").write_text(video_list(VIDEOS))
(HERE / "by_tag.xml").write_text(video_list([v for v in VIDEOS if "music" in v[4] or "funny" in v[4]]))
(HERE / "by_user.xml").write_text(video_list([v for v in VIDEOS if v[1] == "tapehead88"]))
(HERE / "details.xml").write_text("""<?xml version="1.0" encoding="utf-8"?>
<ut_response status="ok">
<video_details>
    <author>dvdlad</author>
    <title>Trying Every Button on the Old Remote</title>
    <rating_avg>4.50</rating_avg>
    <rating_count>22</rating_count>
    <tags>funny tv retro remote</tags>
    <description>Found the remote for our old TV in a drawer. Some of these buttons have never been pressed. Some of them should not have been.</description>
    <update_time>1123000000</update_time>
    <view_count>1342</view_count>
    <upload_time>1122900000</upload_time>
    <length_seconds>173</length_seconds>
    <recording_date>None</recording_date>
    <recording_location/>
    <recording_country/>
    <comment_list>
        <comment>
            <author>tapehead88</author>
            <text>the MUTE button at the end got me</text>
            <time>1122950000</time>
        </comment>
        <comment>
            <author>lunchbox</author>
            <text>we had the exact same tv &amp; remote!!</text>
            <time>1122960000</time>
        </comment>
    </comment_list>
    <channel_list>
        <channel>Humor</channel>
        <channel>Odd &amp; Outrageous</channel>
    </channel_list>
    <thumbnail_url>http://www.kamtape.com/get_still?video_id=vhs07gggggg</thumbnail_url>
</video_details>
</ut_response>
""")
(HERE / "fail.xml").write_text("""<?xml version="1.0" encoding="utf-8"?>
<ut_response status="fail">
<error>
    <code>8</code>
    <description>Missing or unknown dev_id.</description>
</error>
</ut_response>
""")
for i, v in enumerate(VIDEOS):
    thumb(HERE / "thumbs" / f"{v[0]}.jpg", i)
print("fixtures written")

# ---- a release for the updater: a zip laid out like the real one, and GitHub's
# answer listing it (plus an older release from before the app could update)
import hashlib
import io
import json
import zipfile

param = json.loads((HERE.parent.parent / "sce_sys" / "param.json").read_text())
param["contentVersion"] = "01.000.099"
buffer = io.BytesIO()
with zipfile.ZipFile(buffer, "w", zipfile.ZIP_DEFLATED) as z:
    z.writestr("PPSA52005/sce_sys/param.json", json.dumps(param, indent=2))
    z.writestr("PPSA52005/eboot.bin", bytes(rng.getrandbits(8) for _ in range(300000)))
data = buffer.getvalue()
(HERE / "update.zip").write_bytes(data)
digest = hashlib.sha256(data).hexdigest()
notes = (
    "## What's new in 0.9.9\n\n"
    "- Sample release for the PC build: this text stands in for real notes\n"
    "- Plays the update screens from start to finish\n"
    "- **Nothing** here is downloaded from GitHub\n\n"
    f"`PPSA52005.zip` sha256: `{digest}`\n"
    "contentVersion: `01.000.099`\n"
)
releases = [
    {
        "tag_name": "v0.9.9-beta", "name": "KamTape for PS5 0.9.9-beta", "draft": False, "prerelease": True,
        "html_url": "https://github.com/Richie133/Kamtape-PS5/releases/tag/v0.9.9-beta", "body": notes,
        "assets": [{
            "name": "PPSA52005.zip", "size": len(data), "digest": "sha256:" + digest,
            "browser_download_url": "https://github.com/Richie133/Kamtape-PS5/releases/download/v0.9.9-beta/PPSA52005.zip",
        }],
    },
    {
        "tag_name": "v0.3.0-beta", "name": "KamTape for PS5 0.3.0-beta", "draft": False, "prerelease": True,
        "html_url": "https://github.com/Richie133/Kamtape-PS5/releases/tag/v0.3.0-beta",
        "body": "## What's new in 0.3.0\n\n`PPSA52005.zip` sha256: `" + "0" * 64 + "`\n",
        "assets": [{"name": "PPSA52005.zip", "size": 1, "browser_download_url":
                    "https://github.com/Richie133/Kamtape-PS5/releases/download/v0.3.0-beta/PPSA52005.zip"}],
    },
]
(HERE / "releases.json").write_text(json.dumps(releases, indent=2) + "\n")
(HERE / "releases-none.json").write_text("[]\n")
print("update fixtures written")
