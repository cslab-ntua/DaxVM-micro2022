#!/usr/bin/env python

import os
import sys
import statistics

import psutil


def get_fs_type(mypath):
    root_type = ""
    for part in psutil.disk_partitions():
        if part.mountpoint == '/':
            root_type = part.fstype
            continue

        if mypath.startswith(part.mountpoint):
            return part.fstype

    return root_type


WRITE = 0
MMAP = 2
MMAP_POPULATE = 3
MMAP_DAXVM = 4
names = ["write", "write", "mmap", "mmap_populate", "mmap_daxvm"]

random = [0]
repeat = [512]
runs = 10
cores = [1]

bins = [WRITE, MMAP, MMAP_DAXVM]
iosize = [4096, 4096*4, 4096*16, 4096*64, 4096*256, 4096*512*2, 4096*512*8]
aging = ["aged"]
prezeroing = ["on", "off"]
dirty = ["on", "off"]


results = {}
for _random in random:
    results[_random] = {}
    for _a in aging:
        results[_random][_a] = {}
        for r in repeat:
            results[_random][_a][r] = {}
            for io in iosize:
                results[_random][_a][r][io] = {}
                for _bin in bins:
                    results[_random][_a][r][io][_bin] = {}
                    for core in cores:
                        results[_random][_a][r][io][_bin][core] = {}
                        for pre in prezeroing:
                            if _bin >= MMAP_DAXVM and pre == "off":
                                results[_random][_a][r][io][_bin][core][pre] = {}
                                for _d in dirty:
                                    results[_random][_a][r][io][_bin][core][pre][_d] = [
                                    ]
                            else:
                                results[_random][_a][r][io][_bin][core][pre] = []


def call_normal(core, _bin, io, r, _random, pre, _a):
    _r = r
    for _run in range(0, runs):
        exe = "taskset -c 0-%d ./write %d %d %d %d %d > result.txt" % (
            core-1, core, _bin, io, _r, _random)
        print(exe)
        os.system(exe)

        _results = open("./result.txt", "r")
        for l in _results.readlines():
            if ":" in l:
                results[_random][_a][r][io][_bin][core][pre].append(
                    float(l.split(":")[-1]))
                break
        _results.close()


def call_daxvm(core, _bin, io, r, _random, pre, _a, pmem):
    _r = r
    if pre == "on":
        types = ["on"]
    else:
        types = dirty

    for _d in types:
        if _d == "on":
            os.system("echo 1 > /proc/fs/daxvm/daxvm_msync")
        else:
            os.system("echo 0 > /proc/fs/daxvm/daxvm_msync")

        for _run in range(0, runs):
            exe = "taskset -c 0-%d ./write %d %d %d %d %d > result.txt" % (
                core-1, core, _bin, io, _r, _random)
            print(exe)
            os.system(exe)

            _results = open("./result.txt", "r")
            for l in _results.readlines():
                if ":" in l:
                    if pre == "off":
                        results[_random][_a][r][io][_bin][core][pre][_d].append(
                            float(l.split(":")[-1]))
                        break
                    else:
                        results[_random][_a][r][io][_bin][core][pre].append(
                            float(l.split(":")[-1]))
                        break
            _results.close()


def print_results_single_core(_random, _a):

    if _random:
        resultdir = "./cameraready/random/%s" % _a
    else:
        resultdir = "./cameraready/seq/%s" % _a

    os.system("mkdir -p %s" % resultdir)
    out = open("./%s/throughput.txt" % resultdir, "w")
    for pre in prezeroing:
        for _bin in bins:
            if _bin >= MMAP_DAXVM and pre == "off":
                for _d in dirty:
                    out.write(("pre-%s-dirty-%s-%s," %
                               (pre, _d, names[_bin])).rjust(20))

            else:
                out.write(("pre-%s-%s," % (pre, names[_bin])).rjust(20))
    out.write("\n")
    for io in iosize:
        if io < 4096*256:
            out.write(("%dK," % (io/1024)).rjust(8))
        elif io < 4096*512*512:
            out.write(("%dM," % (io/1024/1024)).rjust(8))
        else:
            out.write(("%dG," % (io/1024/1024/1024)).rjust(8))

        for pre in prezeroing:
            for _bin in bins:
                if _bin >= MMAP_DAXVM and pre == "off":
                    for _d in dirty:
                        out.write(("%3f," % (statistics.median(
                            results[_random][_a][r][io][_bin][1][pre][_d]))).rjust(20))
                else:
                    out.write(("%3f," % (statistics.median(
                        results[_random][_a][r][io][_bin][1][pre]))).rjust(20))
        out.write("\n")
    out.write("\n\n\n")
    out.close()

    out = open("./%s/throughput_relative.txt" % resultdir, "w")
    for pre in prezeroing:
        for _bin in bins:
            if _bin >= MMAP_DAXVM and pre == "off":
                for _d in dirty:
                    out.write(("pre-%s-dirty-%s-%s," %
                               (pre, _d, names[_bin])).rjust(20))

            else:
                out.write(("pre-%s-%s," % (pre, names[_bin])).rjust(20))
    out.write("\n")
    for io in iosize:
        if io < 4096*256:
            out.write(("%dK," % (io/1024)).rjust(8))
        elif io < 4096*512*512:
            out.write(("%dM," % (io/1024/1024)).rjust(8))
        else:
            out.write(("%dG," % (io/1024/1024/1024)).rjust(8))
        for pre in prezeroing:
            for _bin in bins:
                if _bin >= MMAP_DAXVM and pre == "off":
                    for _d in dirty:
                        out.write(("%3f," % (statistics.median(results[_random][_a][r][io][_bin][1][pre][_d])/statistics.median(
                            results[_random][_a][r][io][WRITE][1]["on"]))).rjust(20))
                else:
                    out.write(("%3f," % (statistics.median(results[_random][_a][r][io][_bin][1][pre])/statistics.median(
                        results[_random][_a][r][io][WRITE][1]["on"]))).rjust(20))
        out.write("\n")
    out.write("\n\n\n")
    out.close()


pmem = "pmem0"
for _random in random:
    for _a in aging:
        if "nohuge" == _a:
            os.system("echo never >  /sys/kernel/mm/transparent_hugepage/enabled")
        else:
            os.system(
                "echo always >  /sys/kernel/mm/transparent_hugepage/enabled")

        for r in repeat:
            for io in iosize:
                for _bin in bins:
                    os.system("rm /mnt/nvmm1/log.txt")
                    if (_bin >= MMAP_DAXVM):
                        os.system(
                            "echo 1 > /proc/fs/ext4/%s/daxvm_page_tables_on" % pmem)
                    else:
                        os.system(
                            "echo 0 > /proc/fs/ext4/%s/daxvm_page_tables_on" % pmem)

                    for core in cores:
                        for pre in prezeroing:
                            if pre == "on":
                                os.system(
                                    "echo 1 > /proc/fs/ext4/%s/daxvm_zeroout_blocks" % pmem)
                            else:
                                os.system(
                                    "numactl -N 0 ../../Linux/tools/daxvm/prezero/zero > /dev/null &")
                                os.system(
                                    "echo 0 > /proc/fs/ext4/%s/daxvm_zeroout_blocks" % pmem)
                            if _bin >= MMAP_DAXVM:
                                call_daxvm(core, _bin, io, r,
                                           _random, pre, _a, pmem)
                            else:
                                call_normal(core, _bin, io, r,
                                            _random, pre, _a)
                            if pre == "off":
                                os.system("killall zero")

        print_results_single_core(_random, _a)
