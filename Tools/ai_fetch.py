"""Download finished Higgsfield generations into SourceArt/AI and record their provenance.

Usage: python Tools/ai_fetch.py <subdir> <name>=<job_id>=<url> [...]
The manifest (SourceArt/AI/manifest.json) keeps job ids, file names and the model so every paid asset can be traced.
"""
import json, os, sys, urllib.request

root = os.path.join(os.path.dirname(__file__), "..", "SourceArt", "AI")
sub = sys.argv[1]
os.makedirs(os.path.join(root, sub), exist_ok=True)
manifest_path = os.path.join(root, "manifest.json")
manifest = json.load(open(manifest_path)) if os.path.exists(manifest_path) else {"assets": []}
known = {a["file"] for a in manifest["assets"]}
for arg in sys.argv[2:]:
    name, job, url = arg.split("=", 2)
    ext = os.path.splitext(url.split("?")[0])[1] or ".png"
    rel = f"{sub}/{name}{ext}"
    dest = os.path.join(root, rel)
    urllib.request.urlretrieve(url, dest)
    print(f"{rel}  {os.path.getsize(dest) // 1024} KB")
    if rel not in known:
        manifest["assets"].append({"file": rel, "job_id": job, "source": "Higgsfield"})
json.dump(manifest, open(manifest_path, "w"), indent=1)
