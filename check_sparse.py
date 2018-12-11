#!/usr/bin/env python3

import json
import os
import platform
import subprocess
import sys


def get_cflags(compile_commands):
    for cfile in compile_commands:
        cflags = cfile['command']
        cflags = cflags.split(" ")
        out = []
        for f in cflags:
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
        cflags += ['-Wsparse-all', '-Wsparse-error']
        # WORKAROUND(Jflesch):
        # TODO(Jflesch): Why do I need to do that ?
        cflags += ['-D_Float32=float']
        cflags += ['-D_Float32x=float']
        cflags += ['-D_Float64=double']
        cflags += ['-D_Float64x=double']
        cflags += ['-D_Float128=double']
        cflags += ['-D_Float128x=double']
        print("Running sparse on {}".format(filepath))
        cmd = ['sparse', filepath] + cflags
        r = subprocess.run(cmd)
        if r.returncode != 0:
            print("Command was: {}".format(cmd))
            sys.exit(r.returncode)
