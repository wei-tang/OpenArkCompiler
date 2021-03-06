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


import java.lang.reflect.InvocationTargetException;
import java.util.ArrayList;
import java.io.PrintStream;
public class StringGenericTest {
    private static int processResult = 99;
    public static void main(String[] argv) throws IllegalAccessException, IllegalArgumentException,
            InvocationTargetException, NoSuchMethodException, SecurityException {
        System.out.println(run(argv, System.out));
    }
    public static int run(String[] argv, PrintStream out) {
        int result = 2; /* STATUS_Success*/

        try {
            StringGenericTest_1();
        } catch (Exception e) {
            processResult -= 10;
        }
        if (result == 2 && processResult == 99) {
            result = 0;
        }
        return result;
    }
    public static void StringGenericTest_1() throws IllegalAccessException, IllegalArgumentException,
            InvocationTargetException, NoSuchMethodException, SecurityException {
        ArrayList<String> arrayList1 = new ArrayList<String>();
        arrayList1.add("abc");
        ArrayList<Integer> arrayList2 = new ArrayList<Integer>();
        arrayList2.add(123);
        System.out.println(arrayList1.getClass() == arrayList2.getClass());
        String str = StringGenericTest.<String>add("1", "2");
        System.out.println(str);
    }
    public static <T> T add(T x, T y) {
        return y;
    }
}
