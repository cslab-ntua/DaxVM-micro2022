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


READ = 1
MMAP = 2
MMAP_POPULATE = 3
MMAP_DAXVM = 4
FLEX_MMAP = 5
FLEX_MMAP_POPULATE = 6
FLEX_MMAP_DAXVM = 7
names = ["read", "read", "mmap", "mmap_populate", "mmap_daxvm"]

random = [0]
repeat = [1]
runs = 10
cores = [1,2,4,8,16]

bins = [READ, MMAP, MMAP_DAXVM]
iosize = [1024*32]
aging = ["aged"]
prezeroing = ["off"]
dirty = ["on"]
random = [0]

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
    os.system("rm -r /mnt/nvmm1/micro/")
    os.system("mkdir -p /mnt/nvmm1/micro/")
    for _run in range(0, runs):
        exe = "taskset -c 0-%d ./read %d %d %d %d %d > result.txt" % (
            core-1, core, _bin, io, 1, _random)
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

        os.system("rm -r /mnt/nvmm1/micro/")
        os.system("mkdir -p /mnt/nvmm1/micro/")
        for _run in range(0, runs):
            exe = "taskset -c 0-%d ./read %d %d %d %d %d > result.txt" % (
                core-1, core, _bin, io, 1, _random)
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


def print_results(_random, _a):

    if _random:
        resultdir = "./cameraready-mt/random/%s" % _a
    else:
        resultdir = "./cameraready-mt/seq/%s" % _a

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
    for c in cores:
        out.write(("%d," % c))
        for pre in prezeroing:
            for _bin in bins:
                if _bin >= MMAP_DAXVM and pre == "off":
                    for _d in dirty:
                        out.write(("%3f," % (statistics.mean(results[_random][_a][r][io][_bin][c][pre][_d]))).rjust(20))
                else:
                    out.write(("%3f," % (statistics.mean(results[_random][_a][r][io][_bin][c][pre]))).rjust(20))
        out.write("\n")
    out.write("\n\n\n")
    out.close()

def print_results_stdev(_random, _a):

    if _random:
        resultdir = "./cameraready-mt/random/%s" % _a
    else:
        resultdir = "./cameraready-mt/seq/%s" % _a

    os.system("mkdir -p %s" % resultdir)
    out = open("./%s/throughput_stdev.txt" % resultdir, "w")
    for pre in prezeroing:
        for _bin in bins:
            if _bin >= MMAP_DAXVM and pre == "off":
                for _d in dirty:
                    out.write(("pre-%s-dirty-%s-%s," %
                               (pre, _d, names[_bin])).rjust(20))

            else:
                out.write(("pre-%s-%s," % (pre, names[_bin])).rjust(20))
    out.write("\n")
    for c in cores:
        out.write(("%d," % c))
        for pre in prezeroing:
            for _bin in bins:
                if _bin >= MMAP_DAXVM and pre == "off":
                    for _d in dirty:
                        out.write(("%3f," % (statistics.stdev(results[_random][_a][r][io][_bin][c][pre][_d]))).rjust(20))
                else:
                    out.write(("%3f," % (statistics.stdev(results[_random][_a][r][io][_bin][c][pre]))).rjust(20))
        out.write("\n")
    out.write("\n\n\n")
    out.close()

for _random in random:
    for _a in aging:
        if "nohuge" == _a:
            pmem = "pmem0"
            os.system("echo never >  /sys/kernel/mm/transparent_hugepage/enabled")
        else:
            pmem = "pmem0"
            os.system(
                "echo always >  /sys/kernel/mm/transparent_hugepage/enabled")

        os.system("mkdir -p /mnt/nvmm1/micro/")
        for r in repeat:
            for io in iosize:
                for _bin in bins:
                    os.system(
                        "echo 1 > /proc/fs/ext4/%s/daxvm_page_tables_on" % pmem)
                    for core in cores:
                        for pre in prezeroing:
                            if pre == "on":
                                os.system(
                                    "echo 1 > /proc/fs/ext4/%s/daxvm_zeroout_blocks" % pmem)
                            else:
                                os.system(
                                    "echo 0 > /proc/fs/ext4/%s/daxvm_zeroout_blocks" % pmem)
                            if _bin >= MMAP_DAXVM:
                                call_daxvm(core, _bin, io, r,
                                           _random, pre, _a, pmem)
                            else:
                                call_normal(core, _bin, io, r,
                                            _random, pre, _a)

        print_results(_random, _a)
        print_results_stdev(_random, _a)
