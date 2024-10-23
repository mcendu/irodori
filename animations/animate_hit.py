#!/usr/bin/env python3

# Copyright (c) 2024 McEndu.
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to
# deal in the Software without restriction, including without limitation the
# rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
# sell copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
# IN THE SOFTWARE.

"""
Generates a hit50/hit100 animation.
"""

from __future__ import annotations
from argparse import ArgumentParser
from math import pow
from typing import TYPE_CHECKING
from xml.etree.ElementTree import Element
import copy
import os.path
import xml.etree.ElementTree as etree

if TYPE_CHECKING:
    from _typeshed import SupportsWrite

etree.register_namespace("", "http://www.w3.org/2000/svg")


def opacity(frame: float) -> float:
    """The opacity animation."""
    return max(0.0, min(1.0, (29 - frame) / 6.0))


def scale(frame: float) -> float:
    """The scale animation."""
    curve = -(1 / 72) * pow(frame - 6, 2) + 0.5
    return max(1.0, curve)


def opacity_filter(frame: float) -> Element:
    """Create an opacity filter for a specific frame."""
    e = Element("filter", {"id": "opacityFilter"})
    e.append(
        Element(
            "feColorMatrix",
            {
                "in": "SourceGraphic",
                "type": "matrix",
                "values": f"""
                    1 0 0 0 0
                    0 1 0 0 0
                    0 0 1 0 0
                    0 0 0 {opacity(frame)} 0
                """,
            },
        )
    )
    return e


def apply_transforms(object: Element, frame: float):
    """Apply transformations of the animated object for a given frame."""
    object.set("transform", f"scale({scale(frame)})")
    object.set("filter", "url(#opacityFilter)")


def generate_frame(src: etree.ElementTree, frame: int, output: str | SupportsWrite):
    """Generate the SVG for a given frame."""
    tree = copy.deepcopy(src)
    tree.getroot().append(opacity_filter(frame))
    apply_transforms(tree.find(".//*[@id='animated']"), frame)
    tree.write(
        output,
        encoding="UTF-8",
        xml_declaration=True,
    )


if __name__ == "__main__":
    argparse = ArgumentParser(
        description="Generates a hit50/hit100 animation.", exit_on_error=True
    )
    argparse.add_argument("input")
    argparse.add_argument("outdir")

    args = argparse.parse_args()

    if not os.path.isdir(args.outdir):
        raise ValueError("output is not a directory")

    basename = os.path.splitext(os.path.basename(args.input))[0]
    tree = etree.parse(args.input)
    if tree.find(".//*[@id='animated']") is None:
        raise ValueError("input file has no animatable objects")

    for i in range(30):
        generate_frame(tree, i, os.path.join(args.outdir, f"{basename}-{i}.svg"))
