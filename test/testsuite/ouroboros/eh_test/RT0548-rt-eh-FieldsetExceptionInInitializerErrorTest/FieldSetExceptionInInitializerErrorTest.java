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
 * -@TestCaseID: FieldSetExceptionInInitializerErrorTest.java
 * -@TestCaseName: Exception in reflect filed: public void set(Object obj, Object value).
 * -@TestCaseType: Function Test
 * -@RequirementName: [运行时需求]支持Java异常处理
 * -@Brief:
 * -#step1: Create a TestSet class, and create a public static selfShort method and static area has expression error.
 * -#step2: Create a Field instance f by calling the getDeclareFiled method on the TestSet object.
 * -#step3: Test method set(Object obj, Object value).
 * -#step4：Check ExceptionInInitializerError is thrown correctly.
 * -@Expect: 0\n
 * -@Priority: High
 * -@Source: FieldSetExceptionInInitializerErrorTest.java
 * -@ExecuteClass: FieldSetExceptionInInitializerErrorTest
 * -@ExecuteArgs:
 */

import java.io.PrintStream;
import java.lang.reflect.Field;

public class FieldSetExceptionInInitializerErrorTest {
    private static int processResult = 99;

    public static void main(String[] argv) {
        System.out.println(run(argv, System.out));
    }

    /**
     * main test fun
     *
     * @return status code
     */
    public static int run(String[] argv, PrintStream out) {
        int result = 2; /* STATUS_FAILED */
        try {
            result = fieldSetExceptionInInitializerError();
        } catch (Exception e) {
            processResult -= 20;
        }

        if (result == 4 && processResult == 98) {
            result = 0;
        }
        return result;
    }

    /**
     * Exception in reflect filed: public void set(Object obj, Object value).
     *
     * @return status code
     * @throws ClassNotFoundException
     * @throws NoSuchFieldException
     * @throws SecurityException
     * @throws IllegalArgumentException
     * @throws IllegalAccessException
     */
    public static int fieldSetExceptionInInitializerError()
            throws ClassNotFoundException, NoSuchFieldException, SecurityException, IllegalArgumentException,
            IllegalAccessException {
        int result1 = 4; /* STATUS_FAILED */

        Field field = TestSet.class.getDeclaredField("field6");
        try {
            field.set(new TestSet(), 123);
            processResult -= 10;
        } catch (ExceptionInInitializerError e1) {
            processResult--;
        }
        return result1;
    }
}

class TestSet {
    /**
     * a int field for test
     */
    public static int field6 = selfShort();
    /**
     * a int[] field for test
     */
    public static int[] field2 = {1, 2, 3, 4};

    /**
     * set value and return
     *
     * @return value
     */
    public static int selfShort() {
        int self1 = field2[2];
        int value = 11;
        return value;
    }
}

// EXEC:%maple  %f %build_option -o %n.so
// EXEC:%run %n.so %n %run_option | compare %f
// ASSERT: scan-full 0\n