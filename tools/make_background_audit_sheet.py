"""Arrange existing runtime captures for visual QA; never alter source images."""
import argparse
import math
from pathlib import Path
from PIL import Image, ImageDraw


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('captures', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--choices', type=int, nargs='+', default=list(range(37)) + [99])
    parser.add_argument('--levels', action='store_true', help='Arrange compact per-level PNG captures instead of EX menu choices')
    parser.add_argument('--overrides', type=Path, action='append', default=[],
                        help='Use newer whole-frame captures with matching names, without copying or altering the originals')
    args = parser.parse_args()
    frames = []
    if args.overrides and not args.levels:
        parser.error('--overrides requires --levels')
    if args.levels:
        sources = {path.name: path for path in args.captures.glob('*.png')}
        for directory in args.overrides:
            updates = {path.name: path for path in directory.glob('*.png')}
            if not updates or updates.keys() - sources.keys():
                parser.error('Override directory must contain matching existing capture names')
            sources.update(updates)
        captures = [(name.removesuffix('.png'), path) for name, path in sorted(sources.items())]
    else:
        captures = [(f'BG {choice}', args.captures / f'choice-{choice}-final.bmp') for choice in args.choices]
    if not captures:
        parser.error('No captures found')
    for label, path in captures:
        with Image.open(path) as source:
            frame = source.convert('RGB')
        frame.thumbnail((400, 224), Image.Resampling.LANCZOS)
        frames.append((label, frame))
    cell_height = max(frame.height for _, frame in frames) + 24
    sheet = Image.new('RGB', (1600, math.ceil(len(frames) / 4) * cell_height), '#181818')
    labels = ImageDraw.Draw(sheet)
    for index, (label, frame) in enumerate(frames):
        x, y = (index % 4) * 400, (index // 4) * cell_height
        labels.text((x + 8, y + 5), label, fill='white')
        sheet.paste(frame, (x, y + 24))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    sheet.save(args.output)
    print(f'{len(frames)} runtime captures; QA sheet: {args.output}')


if __name__ == '__main__':
    main()
