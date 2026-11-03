"""Restore the JACK bundle inputs from the repository's embedded game data."""

import argparse

import re

from pathlib import Path

from shutil import copyfile, copytree



parser = argparse.ArgumentParser(description=__doc__)

parser.add_argument("output", type=Path)

parser.add_argument("--release", type=Path, help="Optional remaster release containing JACK artwork and font")

args = parser.parse_args()

embedded = Path(__file__).resolve().parents[2] / "FitdLib" / "embedded"

registry = (embedded / "embeddedJackData.cpp").read_text()

args.output.mkdir(parents=True, exist_ok=True)



for name, symbol in re.findall(r'\{\s*"([^"]+)",\s*embdata_(jack_\w+),', registry):

    source = (embedded / ("embedded_" + symbol + ".cpp")).read_text()

    array = source.split("{", 1)[1].split("}", 1)[0]

    data = bytes(int(value, 16) for value in re.findall(r"0x([0-9A-Fa-f]{2})", array))

    declared_size = int(re.search(r"_size\s*=\s*(\d+)", source)[1])

    if len(data) != declared_size:

        raise ValueError(f"{name}: embedded size mismatch")

    (args.output / name).write_bytes(data)



if args.release:

    backgrounds = args.release / "backgrounds_hd"

    output = args.output / "backgrounds_hd"

    output.mkdir(exist_ok=True)

    for artwork in backgrounds.glob("CAMERA16_*.png"):

        copyfile(artwork, output / artwork.name)

    for source, destination in [(backgrounds / "JACKBOOK_000.png", output),

                                (args.release / "BLKCHCRY.TTF", args.output)]:

        if source.is_file():

            copyfile(source, destination / source.name)


    for folder, patterns in {
        "backgrounds_hd": ("anim_CAMERA16_*", "anim_JACKBOOK_*"),
        "atlases": ("*_JACK_*",),
        "masks_hd": ("FLOOR16",),
    }.items():
        destination = args.output / folder
        destination.mkdir(exist_ok=True)
        for pattern in patterns:
            for source in (args.release / folder).glob(pattern):
                if source.is_file():
                    copyfile(source, destination / source.name)
                elif source.is_dir():
                    copytree(source, destination / source.name, dirs_exist_ok=True)
