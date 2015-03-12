#!/usr/bin/env python

import argparse
import os
import sys
import string
import ng_utils

if __name__ == '__main__':
    parser = argparse.ArgumentParser(
            description = 'Check the soundness of the specified AA')
    parser.add_argument('bc', help = 'the bitcode of the program')
    parser.add_argument('logs', nargs='+', help = 'point-to logs (.pts)')
    parser.add_argument('aa',
                        help = 'the checked alias analysis')
    parser.add_argument('--disable-print-value',
                        help = 'disable printing values. only print value IDs',
                        action = 'store_true',
                        default = False)
    parser.add_argument('--output-ng',
                        help = 'output dynamic aliases',
                        action = 'store_true',
                        default = False)
    # Due to the behavior of LLVM's alias analysis chaining, the baseline AA must be an ImmutablePass.
    parser.add_argument('--baseline',
                        help = 'baseline AA which is assumed to be correct: no-aa, basicaa, tbaa',
                        metavar = 'baseline_aa',
                        default = 'no-aa',
                        choices = ['no-aa', 'basicaa', 'tbaa'])
    parser.add_argument('--flag',
                        help = 'Additional flags that are passed to opt',
                        default='')
    args = parser.parse_args()

    cmd = ng_utils.load_all_plugins('opt')
    # Load the baseline AA
    if args.baseline == args.aa:
        sys.stderr.write('\033[0;31m')
        print >> sys.stderr, 'Error: Baseline and the checked AA',
        print >> sys.stderr, 'must be different'
        sys.stderr.write('\033[m')
        sys.exit(1)
    # baseline need be put before aa
    cmd = ng_utils.load_aa(cmd, args.baseline)
    cmd = ' '.join((cmd, '-baseline-aa'))
    cmd = ' '.join((cmd, '-baseline-aa-name', args.baseline))

    # Load the checked AA
    cmd = ng_utils.load_aa(cmd, args.aa)

    cmd = ' '.join((cmd, '-check-aa'))
    for log in args.logs:
        cmd = ' '.join((cmd, '-log-file', log))
    if args.output_ng:
        cmd = ' '.join((cmd, '-output-ng', './'))
    if args.disable_print_value:
        cmd = ' '.join((cmd, '-print-value-in-report=false'))
    cmd = ' '.join((cmd, '-disable-output', '<', args.bc))
    cmd += ' ' + args.flag.replace('"', '')

    ng_utils.invoke(cmd)
