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


import java.lang.StringBuffer;
public class StringBufferExObjectnotifyIllegalMonitorStateException {
    static int res = 99;
    static private String[] stringArray = {
            "", "a", "b", "c", "ab", "ac", "abc", "aaaabbbccc"
    };
    static private StringBuffer sb = null;
    public static void main(String argv[]) {
        for (int i = 0; i < stringArray.length; i++) {
            sb = new StringBuffer(stringArray[i]);
        }
        System.out.println(new StringBufferExObjectnotifyIllegalMonitorStateException().run());
    }
    /**
     * main test fun
     *
     * @return status code
    */

    public int run() {
        int result = 2; /*STATUS_FAILED*/
        try {
            result = stringBufferExObjectnotifyIllegalMonitorStateException1();
        } catch (Exception e) {
            StringBufferExObjectnotifyIllegalMonitorStateException.res = StringBufferExObjectnotifyIllegalMonitorStateException.res - 20;
        }
        Thread t1 = new Thread(new StringBufferExObjectnotifyIllegalMonitorStateException11());
        t1.setUncaughtExceptionHandler(new Thread.UncaughtExceptionHandler() {
            @Override
            public void uncaughtException(Thread t, Throwable e) {
                System.out.println(t.getName() + " : " + e.getMessage());
            }
        });
        t1.start();
        try {
            Thread.sleep(1000);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
        if (result == 4 && StringBufferExObjectnotifyIllegalMonitorStateException.res == 58) {
            result = 0;
        }
        return result;
    }
    private int stringBufferExObjectnotifyIllegalMonitorStateException1() {
        int result1 = 4; /*STATUS_FAILED*/
        // IllegalMonitorStateException - if the current thread is not the owner of the object's monitor.
        // final void notify()
        try {
            sb.notify();
            StringBufferExObjectnotifyIllegalMonitorStateException.res = StringBufferExObjectnotifyIllegalMonitorStateException.res - 10;
        } catch (IllegalMonitorStateException e2) {
            StringBufferExObjectnotifyIllegalMonitorStateException.res = StringBufferExObjectnotifyIllegalMonitorStateException.res - 1;
        }
        return result1;
    }
    private class StringBufferExObjectnotifyIllegalMonitorStateException11 implements Runnable {
        // final void notify()
        /**
         * Thread run fun
        */

        public void run() {
            synchronized (sb) {
                try {
                    sb.notify();
                    StringBufferExObjectnotifyIllegalMonitorStateException.res = StringBufferExObjectnotifyIllegalMonitorStateException.res - 40;
                } catch (IllegalMonitorStateException e2) {
                    StringBufferExObjectnotifyIllegalMonitorStateException.res = StringBufferExObjectnotifyIllegalMonitorStateException.res - 30;
                }
            }
        }
    }
}
