/*
 * Copyright (c) [2021] Huawei Technologies Co.,Ltd.All rights reserved.
 *
 * OpenArkCompiler is licensed under Mulan PSL v2.
 * You can use this software according to the terms and conditions of the Mulan PSL v2.
 *
 *     http://license.coscl.org.cn/MulanPSL2
 *
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR
 * FIT FOR A PARTICULAR PURPOSE.
 * See the Mulan PSL v2 for more details.
*/


import java.lang.Thread;
public class ThreadGroupExObjecttoString {
    static int res = 99;
    public static void main(String argv[]) {
        System.out.println(new ThreadGroupExObjecttoString().run());
    }
    /**
     * main test fun
     * @return status code
    */

    public int run() {
        int result = 2; /*STATUS_FAILED*/
        try {
            result = threadGroupExObjecttoString1();
        } catch (Exception e) {
            ThreadGroupExObjecttoString.res = ThreadGroupExObjecttoString.res - 20;
        }
        if (result == 4 && ThreadGroupExObjecttoString.res == 89) {
            result = 0;
        }
        return result;
    }
    private int threadGroupExObjecttoString1() {
        int result1 = 4; /*STATUS_FAILED*/
        // String toString()
        ThreadGroup gr1 = new ThreadGroup("Thread8023");
        String str1 = gr1.toString();
        if (str1.equals("java.lang.ThreadGroup[name=Thread8023,maxpri=10]")) {
            ThreadGroupExObjecttoString.res = ThreadGroupExObjecttoString.res - 10;
        }
        return result1;
    }
}