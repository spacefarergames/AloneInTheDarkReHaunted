import sys
filepath = "FitdLib/hdBackground.cpp"
with open(filepath, "rb") as f:
    content = f.read()
old = b"// Ensure the HD archive is open (lazy init, called on first use)\r\nstatic void ensureArchiveOpen()\r\n{\r\n    static bool s_tried = false;\r\n    if (s_tried) return;\r\n    s_tried = true;\r\n    HDArchive::open(\x22backgrounds_hd.hda\x22);\r\n}"
rep = b"// Ensure the HD archive is open (lazy init, called on first use).\r\n// The HDArchive namespace is a singleton - only one archive can be open at a\r\n// time.  Other subsystems (e.g. modelAtlas) may clobber it by opening a\r\n// different archive, so we must be prepared to reopen backgrounds_hd.hda\r\n// whenever it has been closed out from under us.\r\nstatic void ensureArchiveOpen()\r\n{\r\n    if (HDArchive::isOpen())\r\n        return; // still open from a previous call\r\n\r\n    static bool s_failed = false;\r\n    if (s_failed) return; // file does not exist - no point retrying every frame\r\n\r\n    if (!HDArchive::open(\x22backgrounds_hd.hda\x22))\r\n        s_failed = true;\r\n}"
if old in content:
    content = content.replace(old, rep, 1)
    with open(filepath, "wb") as f:
        f.write(content)
    print("REPLACED OK")
else:
    print("NOT FOUND")
    sys.exit(1)