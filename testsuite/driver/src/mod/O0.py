#
# Copyright (c) [2021] Huawei Technologies Co.,Ltd.All rights reserved.
#
# OpenArkCompiler is licensed under Mulan PSL v2.
# You can use this software according to the terms and conditions of the Mulan PSL v2.
#
#     http://license.coscl.org.cn/MulanPSL2
#
# THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER
# EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR
# FIT FOR A PARTICULAR PURPOSE.
# See the Mulan PSL v2 for more details.
#

from api import *

O0 = {
    "clean": [
        Shell(
            "rm -rf *.mpl *.dex *.class *.mplt *.s *.so *.o *.log *.jar *.VtableImpl.primordials.txt"
        )
    ],
    "compile": [
        Java2dex(
            jar_file=[
                "${OUT_ROOT}/aarch64-clang-release/ops/third_party/JAVA_LIBRARIES/core-oj_intermediates/classes.jar",
                "${OUT_ROOT}/aarch64-clang-release/ops/third_party/JAVA_LIBRARIES/core-libart_intermediates/classes.jar"
            ],
            outfile="${APP}.dex",
            infile=["${APP}.java","${EXTRA_JAVA_FILE}"]
        ),
        Dex2mpl(
            dex2mpl="${OUT_ROOT}/aarch64-clang-release/bin/dex2mpl",
            mplt="${OUT_ROOT}/aarch64-clang-release/libjava-core/libcore-all.mplt",
            litprofile="${MAPLE_ROOT}/src/mrt/codetricks/profile.pv/meta.list",
            infile="${APP}.dex"
        ),
        Maple(
            maple="${OUT_ROOT}/aarch64-clang-release/bin/maple",
            run=["me", "mpl2mpl", "mplcg"],
            option={
                "me": "--quiet",
                "mpl2mpl": "--quiet --regnativefunc --maplelinker --emitVtableImpl",
                "mplcg": "--quiet --no-pie --fpic --verbose-asm --maplelinker"
            },
            global_option="",
            infile="${APP}.mpl"
        ),
        Linker(
            lib="host-x86_64-O2",
        )
    ],
    "run": [
        Mplsh(
            qemu="${QEMU_PATH}/qemu-aarch64",
            qemu_libc="/usr/aarch64-linux-gnu",
            qemu_ld_lib=[
                "${OUT_ROOT}/aarch64-clang-release/ops/third_party",
                "${OUT_ROOT}/aarch64-clang-release/ops/host-x86_64-O2",
                "./"
            ],
            mplsh="${OUT_ROOT}/aarch64-clang-release/ops/mplsh",
            garbage_collection_kind="RC",
            xbootclasspath="libcore-all.so",
            infile="${APP}.so",
            redirection="output.log"
        ),
        CheckFileEqual(
            file1="output.log",
            file2="expected.txt"
        )
    ]
}