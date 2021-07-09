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

import os
import stat
import sys

from api.shell import Shell
from test_cfg import TestCFG
from env_var import EnvVar
from mode import *
from shell_executor import ShellExecutor
from basic_tools.file import get_sub_files

class Case(object):

    def __init__(self, case_name, modes):
        self.case_name = case_name
        self.modes = modes
        self.case_timeout = 900
        self.case_cpu = 0.7
        self.case_memory = 0
        self.test_cfg = TestCFG(os.path.join(EnvVar.TEST_SUITE_ROOT, self.case_name, "test.cfg"))
        self.test_cfg_content = self.test_cfg.test_cfg
        self.shell_command_suite = {}
        self.case_path = os.path.join(EnvVar.TEST_SUITE_ROOT, self.case_name)

    def parse_config_file(self):
        for mode in self.modes:
            self.shell_command_suite[mode] = []
            global_variables = {"CASE": self.case_name, "MODE": mode}
            if EnvVar.MAPLE_BUILD_TYPE is not None:
                global_variables["MAPLE_BUILD_TYPE"] = EnvVar.MAPLE_BUILD_TYPE
            if mode in self.test_cfg_content.keys():
                mode_cfg_content = self.test_cfg_content[mode]
            else:
                mode_cfg_content = self.test_cfg_content["default"]
            for line in mode_cfg_content:
                if line[0] == "global":
                    global_variables.update(line[1])
                elif line[0] == "shell":
                    self.shell_command_suite[mode].append(Shell(line[1]).get_command(global_variables))
                else:
                    local_variables = line[1]
                    variables = global_variables.copy()
                    variables.update(local_variables)
                    phase = line[0]
                    if phase in globals()[mode].keys():
                        for command_object in globals()[mode][phase]:
                            self.shell_command_suite[mode].append(command_object.get_command(variables))
                    else:
                        print(line[0] + " not found !")
                        os._exit(1)

    def generate_test_script(self):
        for mode in self.modes:
            mode_file_path = os.path.join(EnvVar.TEST_SUITE_ROOT, self.case_name, "." + mode + "_test.sh")
            with open(mode_file_path, 'w') as f:
                f.write("#!/bin/bash\nset -e\nset -x\n")
                f.write("\n".join(self.shell_command_suite[mode]) + "\n")
            os.chmod(mode_file_path, stat.S_IRWXU)

    def rm_tmp_files(self, detail=False):
        all_cur_files = [file for file in os.listdir(self.case_path) if not file.endswith('_tmp@')]
        if '.raw_file_list.txt' not in all_cur_files:
            return
        with open(os.path.join(self.case_path,'.raw_file_list.txt'), 'r') as f:
            raw_cur_files, raw_sub_files = f.read().split('\n-----\n')
        tmp_cur_files = list(set(all_cur_files) - set(raw_cur_files.split('\n')))
        if tmp_cur_files:
            os.system('rm -rf %s'%(' '.join([os.path.join(self.case_path,f) for f in tmp_cur_files])))
        all_sub_files = [file for file in get_sub_files(self.case_path) if '_tmp@' not in file]
        tmp_sub_files = list(set(all_sub_files) - set(raw_sub_files.split('\n')))
        if tmp_sub_files:
            os.system('rm -rf %s'%(' '.join([os.path.join(self.case_path,f) for f in tmp_sub_files])))
        if detail and (tmp_cur_files or tmp_sub_files):
            print("\033[1;32m [[ CMD : rm -rf %s ]]\033[0m"%('  '.join(tmp_cur_files + tmp_sub_files)))

    def rm_tmp_folders(self, detail=False):
        del_file_list = [os.path.join(self.case_path,f) for f in os.listdir(self.case_path) if f.endswith("_tmp@")]
        if detail and del_file_list != []:
            os.system('rm -rf ' + " ".join(del_file_list))
            print("\033[1;32m [[ CMD : rm -rf %s ]]\033[0m"%('  '.join(del_file_list)))

    def clean(self, detail=False):
        self.rm_tmp_files(detail)
        self.rm_tmp_folders(detail)

    def save_tmp(self, detail=False):
        all_cur_files = [file for file in os.listdir(self.case_path) if not file.endswith('_tmp@')]
        if '.raw_file_list.txt' not in all_cur_files:
            return
        tmp_folder = [int(file.split('_tmp@')[0]) for file in os.listdir(self.case_path) if file.endswith('_tmp@')]
        cur_max_folder = 0
        if tmp_folder != []:
            cur_max_folder = max(tmp_folder)
        os.mkdir(str(cur_max_folder + 1) + '_tmp@')
        for file in all_cur_files:
            os.system('cp -r %s %d_tmp@'%(file, cur_max_folder + 1))
        self.clean()

    def save_raw_files(self):
        all_cur_files = os.listdir(self.case_path)
        all_sub_files = [file for file in get_sub_files(self.case_path) if '_tmp@' not in file]
        if '.raw_file_list.txt' in all_cur_files:
            return
        with open(os.path.join(self.case_path,'.raw_file_list.txt'), 'w') as f:
            f.writelines('\n'.join(all_cur_files))
            f.writelines('\n-----\n')
            f.writelines('\n'.join(all_sub_files))

    def get_case_package(self):
        case_list = []
        for mode in self.modes:
            case_list.append({
                "name": self.case_name,
                "memory": self.case_memory,
                "cpu": self.case_cpu,
                "option": mode,
                "cmd": "cd ${WORKSPACE}/out/host/test/" + self.case_name + ";export OUT_ROOT=${WORKSPACE}/out;bash ." + mode + "_test.sh",
                "timeout": self.case_timeout
            })
        return case_list

    def run(self, detail = False):
        result_table = {}
        for mode in self.modes:
            self.rm_tmp_files(detail)
            self.save_raw_files()
            result, result_in_color = "PASSED", "\033[1;32mPASSED\033[0m"
            log_file = open(os.path.join(EnvVar.TEST_SUITE_ROOT, self.case_name, mode + "_run.log"), "w+")
            for command in self.shell_command_suite[mode]:
                log_file.write("[[ CMD : " + command + " ]]\n")
                if detail:
                    print("\033[1;32m[[ CMD : " + command + " ]]\033[0m")
                exe = ShellExecutor(command=command, workdir=os.path.join(EnvVar.TEST_SUITE_ROOT, self.case_name), timeout=5000)
                exe.shell_execute()
                if exe.com_out is not None and len(exe.com_out) != 0:
                    if detail:
                        print(exe.com_out)
                    log_file.write(exe.com_out + "\n")
                if exe.com_err is not None and len(exe.com_err) != 0:
                    if detail:
                        print(exe.com_err)
                    log_file.write(exe.com_err + "\n")
                if exe.return_code == 124:
                    if detail:
                        print("ERROR : TIMEOUT !")
                    log_file.write("ERROR : TIMEOUT !\n")
                    result, result_in_color = "TIMEOUT", "\033[1;33mTIMEOUT\033[0m"
                    break
                elif exe.return_code != 0:
                    if detail:
                        print("ERROR : FAILED !")
                    log_file.write("ERROR : FAILED !\n")
                    result, result_in_color = "FAILED", "\033[1;31mFAILED\033[0m"
                    break
            print(self.case_name + " " + mode + " " + result_in_color)
            sys.stdout.flush()
            result_table[mode] = result
        return {self.case_name: result_table}
