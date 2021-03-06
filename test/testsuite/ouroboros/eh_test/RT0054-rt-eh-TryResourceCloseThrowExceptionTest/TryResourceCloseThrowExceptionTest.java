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
 * -@TestCaseID: TryResourceCloseThrowExceptionTest.java
 * -@TestCaseName: Test try(){}-catch(e){}-close(exception)-finally.
 * -@TestCaseType: Function Test
 * -@RequirementName: [运行时需求]支持Java异常处理
 * -@Brief:
 * -#step1: Create self class by implements AutoCloseable.
 * -#step2: Create resource instance by new self class.
 * -#step3: Test start() method and close() method.
 * -#step4: Check StringIndexOutOfBoundsException is thrown and finally is walked.
 * -@Expect: 0\n
 * -@Priority: High
 * -@Source: TryResourceCloseThrowExceptionTest.java
 * -@ExecuteClass: TryResourceCloseThrowExceptionTest
 * -@ExecuteArgs:
 */

import java.io.PrintStream;

public class TryResourceCloseThrowExceptionTest {
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
            result = tryResourceCloseThrowExceptionTest1();
        } catch (Exception e) {
            processResult -= 10;
        }

        if (result == 1 && processResult == 94) {
            result = 0;
        }
        return result;
    }

    public static class MyResource implements AutoCloseable {
        /**
         * start fun
         *
         * @throws NumberFormatException
         */
        public void start() throws NumberFormatException {
            processResult--;
        }

        @Override
        public void close() throws StringIndexOutOfBoundsException {
            processResult--;
            throw new StringIndexOutOfBoundsException("end:exception!!!");
        }
    }

    /**
     * Test try(){}-catch(e){}-close(exception)-finally.
     *
     * @return status code
     */
    public static int tryResourceCloseThrowExceptionTest1() {
        int result1 = 4; /* STATUS_FAILED */
        try (MyResource conn = new MyResource()) {
            conn.start();
        } catch (NumberFormatException e) {
            processResult -= 10;
            e.printStackTrace();
            result1 = 3;
        } catch (StringIndexOutOfBoundsException e) {
            processResult--;
            result1 = 1;
        } finally {
            processResult--;
        }
        processResult--;
        return result1;
    }
}

// EXEC:%maple  %f %build_option -o %n.so
// EXEC:%run %n.so %n %run_option | compare %f
// ASSERT: scan-full 0\n