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


import java.io.PrintStream;
public class StringConsBytesTest {
    private static int processResult = 99;
    public static void main(String[] argv) {
        System.out.println(run(argv, System.out));
    }
    public static int run(String[] argv, PrintStream out) {
        int result = 2; /* STATUS_Success*/

        try {
            StringConsBytesTest_1();
        } catch (Exception e) {
            processResult -= 10;
        }
        if (result == 2 && processResult == 99) {
            result = 0;
        }
        return result;
    }
    public static void StringConsBytesTest_1() {
        byte[] str1_1 = new byte[]{(byte) 0x61, (byte) 0x62, (byte) 0x63, (byte) 0x31, (byte) 0x32, (byte) 0x33};
        String str1 = new String(str1_1);
        byte[] str1_2 = new byte[]{97, 98, 99, 49, 50, 51};
        String str1_3 = new String(str1_2);
        System.out.println(str1);
        System.out.println(str1_3);
    }
}
