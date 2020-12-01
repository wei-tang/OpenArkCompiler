/*
 * Copyright (c) [2020] Huawei Technologies Co.,Ltd.All rights reserved.
 *
 * OpenArkCompiler is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *
 *     http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR
 * FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
 * -@TestCaseID: Maple_ArrayBoundary_ABCOreturnIndex.java
 * -@TestCaseName: Array is get by function return, Index is get by function return
 * -@TestCaseType: Function Test
 * -@RequirementName: Array Bounds Check优化
 * -@Brief:
 * -#step1: new Array[5], Array is get by function return, Index is get by function return
 * -#step2: if visit Array
 * -#step3: catch Exception
 * -@Expect: 0\n
 * -@Priority: High
 * -@Source: ABCOreturnIndex.java
 * -@ExecuteClass: ABCOreturnIndex
 * -@ExecuteArgs:
 */

import java.io.PrintStream;

public class ABCOreturnIndex {
    static int RES_PROCESS = 99;

    public static void main(String[] argv) {
        System.out.println(run(argv, System.out)); //
    }

    public static int run(String argv[], PrintStream out) {
        int result = 4 /*STATUS_FAILED*/;
        try {
            result = test1();
        } catch (Exception e) {
            RES_PROCESS -= 10;
        }

        if (result == 1 && RES_PROCESS == 99) {
            result = 0;
        }
        return result;
    }

    public static int func() {
        int getRand = 0;
        for (int i = 0; i < 10; i++) {
            for (int j = 0; j < 10; j++) {
                for (int k = 0; k < 10; k++) {
                    getRand = getRand + k;
                    if (getRand > 200) {
                        break;
                    }
                }
            }
        }
        return getRand;
    }

    public static int test1() {
        int res = 3 /*STATUS_FAILED*/;

        int[] arr = new int[100];
        int[] arrCopy = arr;
        for (int i = 0; i < arr.length; i++) {
            arrCopy[i] = i;
        }

        int y = func();
        try {
            if (y >= 0) {
                int c = arr[y];
            }
        } catch (ArrayIndexOutOfBoundsException e) {
            res = 1;
        }

        return res;
    }
}

// EXEC:%maple  %f %build_option -o %n.so
// EXEC:%run %n.so %n %run_option | compare %f
// ASSERT: scan-full 0\n