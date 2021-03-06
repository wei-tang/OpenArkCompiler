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


public class ThreadTest_13 {
    /**
     * Test for run(). Should do nothing.
    */

    public static void main(String[] args) throws Exception {
        Thread t = new Thread();  // Test run() for thread
        Thread.State tsBefore = t.getState();
        t.run();
        Thread.State tsAfter = t.getState();
        System.out.println("run() should do nothing --- " + tsBefore.equals(Thread.State.NEW));
        System.out.println("run() should do nothing --- " + tsBefore.equals(tsAfter));
        System.out.println("run() should do nothing --- " + tsBefore);
        Thread tt = new Thread(new ThreadRun("helloworld")); // Test run() for runnable object
        Thread.State ttsBefore = tt.getState();
        tt.run();
        Thread.State ttsAfter = tt.getState();
        System.out.println("run() should do nothing --- " + ttsBefore.equals(Thread.State.NEW));
        System.out.println("run() should do nothing --- " + ttsBefore.equals(ttsAfter));
    }
    private static class ThreadRun implements Runnable {
        private final String helloWorld;
        public ThreadRun(String str) {
            helloWorld = str;
        }
        public void run() {
            System.out.println(helloWorld);
        }
    }
}
