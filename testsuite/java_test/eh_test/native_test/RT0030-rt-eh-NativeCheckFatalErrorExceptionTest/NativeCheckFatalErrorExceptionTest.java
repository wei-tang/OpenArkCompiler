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
import java.nio.file.InvalidPathException;
public class NativeCheckFatalErrorExceptionTest extends Exception {
    static {
        System.loadLibrary("jniNativeCheckFatalErrorExceptionTest");
    }
    private static int processResult = 99;
    public static void main(String[] argv) {
        System.exit(run(argv, System.out));
    }
    private native void nativeNativeCheckFatalErrorExceptionTest() throws InvalidPathException;
    private void callback() throws NumberFormatException {
        throw new NumberFormatException("JavaMethodThrows");
    }
    /**
     * main test fun
     * @return status code
    */

    public static int run(String[] argv, PrintStream out) {
        int result = 2; /* STATUS_FAILED*/

        try {
            result = nativeCheckFatalErrorExceptionTest();
        } catch (Exception e) {
            processResult -= 10;
        }
        if (result == 1 && processResult == 95) {
            result = 0;
        }
        return result;
    }
    /**
     *  Asynchronous: Native(exception at callback from Java, then FatalError), catch by Java.
     * @return status code
    */

    public static int nativeCheckFatalErrorExceptionTest() {
        int result1 = 4; /* STATUS_FAILED*/

        NativeCheckFatalErrorExceptionTest cTest = new NativeCheckFatalErrorExceptionTest();
        try {
            cTest.nativeNativeCheckFatalErrorExceptionTest();
            processResult -= 10;
        } catch (NumberFormatException e1) {
            result1 = 1;
            System.out.println("======>" + e1.getMessage());
            processResult--;
        } catch (InvalidPathException e2) {
            result1 = 3;
            System.out.println("======>" + e2.getMessage());
            processResult -= 10;
        } catch (StringIndexOutOfBoundsException e3) {
            result1 = 3;
            processResult -= 10;
        }
        processResult -= 3;
        return result1;
    }
}
