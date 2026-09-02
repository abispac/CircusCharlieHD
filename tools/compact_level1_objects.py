#!/usr/bin/env python3
"""Reduce a capture-objects.csv from the manual capture script to one row per
frame with the status, X, +$01:+$02 and +$08 bytes of the Level 1 records the
native replay comparator checks."""
import csv
import sys

RECORDS = ['24b0', '24f0', '2530', '2570', '2580', '25e0', '26d0', '2700',
           '2730', '2760']


def main() -> None:
    source, target = sys.argv[1], sys.argv[2]
    with open(source) as handle, open(target, 'w') as out:
        out.write('frame,' + ','.join(
            f'{r}_st,{r}_x,{r}_b1b2,{r}_b8' for r in RECORDS) + '\n')
        current = {}
        frame = None

        def flush() -> None:
            out.write(str(frame) + ',' + ','.join(
                ','.join(current.get(r, ('00', '0000', '0000', '00')))
                for r in RECORDS) + '\n')

        for row in csv.DictReader(handle):
            f = int(row['frame'])
            if frame is None:
                frame = f
            if f != frame:
                flush()
                frame = f
                current = {}
            if row['record'] in RECORDS:
                current[row['record']] = (row['b00'], row['b06'] + row['b07'],
                                          row['b01'] + row['b02'], row['b08'])
        if frame is not None:
            flush()


if __name__ == '__main__':
    main()
