#!/usr/bin/env python3
"""Regenerates lucide-icons.json from a lucide-static release.

BookOrbit's collections take any Lucide icon by name, as its web client's @lucide/vue
exports them, so the app carries the whole set: {"icons": {name: [[tag, attrs], ...]},
"aliases": {other name: name}}, names in kebab case. Use the @lucide/vue version in
BookOrbit's client/package.json.

    python3 -I lucide/update.py 1.54.0
"""
import io
import json
import os
import re
import sys
import tarfile
import urllib.request

version = sys.argv[1]
url = f"https://registry.npmjs.org/lucide-static/-/lucide-static-{version}.tgz"
with urllib.request.urlopen(url) as response:
    archive = tarfile.open(fileobj=io.BytesIO(response.read()))
files = {member.name: member for member in archive.getmembers() if member.isfile()}


def read(name):
    return archive.extractfile(files[name]).read().decode()


nodes = json.loads(read("package/icon-nodes.json"))


# An icon's SVG without its comment and class, which name it
def body(name):
    return re.sub(r'class="[^"]*"', "", re.sub(r"<!--.*?-->", "", read(name), flags=re.S)).strip()


canonical = {body(f"package/icons/{name}.svg"): name for name in nodes}
aliases = {}
for path in sorted(files):
    match = re.fullmatch(r"package/icons/(.+)\.svg", path)
    if match and match[1] not in nodes:
        name = canonical.get(body(path))
        if name:
            aliases[match[1]] = name
        else:
            print("no icon matches", match[1], file=sys.stderr)

here = os.path.dirname(os.path.abspath(__file__))
with open(os.path.join(here, "lucide-icons.json"), "w") as out:
    json.dump({"icons": nodes, "aliases": aliases}, out, separators=(",", ":"), sort_keys=True)
with open(os.path.join(here, "LICENSE"), "w") as out:
    out.write(read("package/LICENSE"))
print(f"{len(nodes)} icons, {len(aliases)} aliases")
