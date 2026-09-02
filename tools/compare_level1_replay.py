#!/usr/bin/env python3
"""Compare a native --replay-output CSV against the MAME capture it replayed.

Usage:
  compare_level1_replay.py capture-state.csv native-replay.csv [objects-compact.csv]

Every board frame in which the emulated game is in player state $01 is
compared field by field: course page/offset, airborne flag, rider row,
course index/state, $26D0, score, lives, $220A, and (with the compact objects
file) every hoop record, every fire-pot record and the $25E0 bag state.
"""
import csv
import sys


def main() -> None:
    mame = {int(r['frame']): r for r in csv.DictReader(open(sys.argv[1]))}
    native = list(csv.DictReader(open(sys.argv[2])))
    objects = {}
    if len(sys.argv) > 3:
        objects = {int(r['frame']): r for r in csv.DictReader(open(sys.argv[3]))}
    mismatches = {}
    firsts = {}
    compared = 0
    for r in native:
        frame = int(r['mame_frame'])
        m = mame.get(frame)
        if not m or m['player_state_2800'] != '01':
            continue
        compared += 1
        checks = {
            'progress': (m['scroll_acc_2203'],
                         '%02x%02x' % (int(r['page']), int(r['offset_byte']))),
            'airborne': (int(m['airborne_20b0'], 16), int(r['airborne'])),
            'rider_row': (int(m['rider_y_2644']), int(r['rider_row'])),
            'course': ((int(m['course_index_2208'], 16),
                        int(m['course_state_20bc'], 16)),
                       (int(r['course_index']), int(r['course_state']))),
            'score': (int(m['score_bcd_20a0']), int(r['score'])),
            'scene_playing': (3, int(r['scene'])),
        }
        status = int(m['object_26d0_status'], 16)
        checks['hoop_26d0'] = ((status != 0, int(m['object_26d0_x88'], 16) if status else 0),
                               (int(r['hoop0']) != 0, int(r['hoop0_x']) if int(r['hoop0']) else 0))
        if 'lives_2200' in m:
            checks['lives'] = (int(m['lives_2200'], 16), int(r['lives']))
        if 'extra_220a' in m:
            checks['extra_charlie'] = (int(m['extra_220a'], 16), int(r['extra_state']))
        o = objects.get(frame)
        if o:
            for rec, col in [('24b0', 'pot0'), ('24f0', 'pot1'), ('2530', 'pot2'),
                             ('26d0', 'hoop0'), ('2700', 'hoop1'), ('2730', 'hoop2'),
                             ('2760', 'hoop3')]:
                st = int(o[rec + '_st'], 16)
                x = int(o[rec + '_x'], 16)
                nst = int(r[col])
                nx = int(r[col + '_x'])
                if col.startswith('pot'):
                    ok = st == nst and (st == 0 or x == nx)
                    if st == 1:
                        ok = ok and int(o[rec + '_b1b2'], 16) == int(r[col + '_cd'])
                    checks[col] = ((st, x), (nst, nx)) if not ok else (0, 0)
                else:
                    ok = (st != 0) == (nst != 0) and (st == 0 or x == nx)
                    checks[col] = ((st, x), (nst, nx)) if not ok else (0, 0)
            checks['bag_25e0'] = (int(o['25e0_st'], 16), int(r['bag_state']))
        for key, (expected, actual) in checks.items():
            if expected != actual:
                mismatches[key] = mismatches.get(key, 0) + 1
                firsts.setdefault(key, (frame, expected, actual))
    print(f'compared {compared} frames in player state $01')
    if not mismatches:
        print('no mismatches')
        return
    for key, count in mismatches.items():
        print(f'{key}: {count} mismatches, first at frame {firsts[key][0]}: '
              f'mame={firsts[key][1]} native={firsts[key][2]}')
    sys.exit(1)


if __name__ == '__main__':
    main()
