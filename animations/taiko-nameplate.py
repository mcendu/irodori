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

"""Generates a customized Taiko no Tatsujin style name plate."""

from __future__ import annotations
from argparse import ArgumentParser, FileType
from typing import TYPE_CHECKING
from xml.etree.ElementTree import Element
import copy
import os.path
import xml.etree.ElementTree as etree

etree.register_namespace("", "http://www.w3.org/2000/svg")


def generate_nameplate(
    svg: etree.ElementTree, title: str, title_lang: str, player: str, player_lang: str
):
    title_element = svg.find(".//*[@id='title']")
    player_element = svg.find(".//*[@id='player']")

    if title_element is not None and player_element is not None:
        title_element.text = title
        title_element.set("lang", title_lang)
        player_element.text = player
        player_element.set("lang", player_lang)
        return svg

    raise ValueError("input does not have the required SVG elements")


if __name__ == "__main__":
    argparse = ArgumentParser(
        description="Generates a customized Taiko no Tatsujin style name plate.",
        exit_on_error=True,
    )
    argparse.add_argument("--title", nargs="?", default="太鼓の初心者")
    argparse.add_argument("--title-lang", nargs="?", default="ja")
    argparse.add_argument("--player", nargs="?", default="Mocha")
    argparse.add_argument("--player-lang", nargs="?", default="en")
    argparse.add_argument("input", type=FileType('r', encoding='UTF-8'))
    argparse.add_argument("output")

    args = argparse.parse_args()

    tree = etree.parse(args.input)
    generate_nameplate(
        tree,
        title=args.title,
        title_lang=args.title_lang,
        player=args.player,
        player_lang=args.player_lang,
    )
    tree.write(
        args.output,
        encoding="UTF-8",
        xml_declaration=True,
    )
