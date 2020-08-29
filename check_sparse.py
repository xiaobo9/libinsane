#!/usr/bin/env python3

import json
import os
import subprocess
import sys


def get_cflags(compile_commands):
    for cfile in compile_commands:
        cflags = cfile['command']
        cflags = cflags.split(" ")
        out = []
        for f in cflags:
            if f == "":
                continue
            if f[0] == "'" and f[-1] == "'":
                f = f[1:-1]
            if f[:2] == "-D" or f[:2] == "-I":
                out.append(f)
        yield (out, cfile['file'])


if __name__ == "__main__":
    with open(sys.argv[1], 'r') as fd:
        compile_commands = json.load(fd)
    os.chdir(os.path.dirname(sys.argv[1]))
    for (cflags, filepath) in get_cflags(compile_commands):
        if "libinsane-gobject" in filepath:
            # gobject macros raise many warnings --> do not check
            continue
        cflags += [
            '-O2', '-Wsparse-all', '-Wsparse-error', '-Wno-non-pointer-null'
        ]
        # WORKAROUND(Jflesch):
        # TODO(Jflesch): Why do I need to do that ?
        cflags += ['-D_Float32=float']
        cflags += ['-D_Float32x=float']
        cflags += ['-D_Float64=double']
        cflags += ['-D_Float64x=double']
        cflags += ['-D_Float128=double']
        cflags += ['-D_Float128x=double']
        cflags += ['-D__ARM_PCS_VFP']
        print("Running sparse on {}".format(filepath))
        cmd = ['sparse', filepath] + cflags
        r = subprocess.run(cmd)
        if r.returncode != 0:
            print("Command was: {}".format(cmd))
            sys.exit(r.returncode)
