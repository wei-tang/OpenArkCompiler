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

CO0 = {
    "compile": [
        Clang2mpl(
            infile="${APP}.c"
        ),
        Maple(
            maple="${OUT_ROOT}/${MAPLE_BUILD_TYPE}/bin/maple",
            run=["mplcg"],
            option={
                "mplcg": "--quiet"
            },
            global_option="",
            infile="${APP}.mpl"
        ),
        CLinker(
            infile="${APP}.s",
            front_option="",
            outfile="${APP}.out",
            back_option="-lm"
        )
    ],
    "run": [
        QemuRun(
            qemu_libc=[
                "${MAPLE_ROOT}/tools/gcc-linaro-7.5.0/aarch64-linux-gnu/libc"
            ],
            infile="${APP}.out",
            redirection="output.log"
        ),
        CheckFileEqual(
            file1="output.log",
            file2="expected.txt"         
        )
    ]
}
