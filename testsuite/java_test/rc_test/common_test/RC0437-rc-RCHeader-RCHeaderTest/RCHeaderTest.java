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


import sun.misc.Cleaner;
class RCRunner implements Runnable {
    public void run() {
        RCHeaderTest.iArray = null;
    }
}
class Resource {
    @Override
    public void finalize() {
        System.out.println("ExpectResult");
    }
}
public class RCHeaderTest {
    static int[] iArray;
    static Cleaner c = null;
    Resource res;
    public RCHeaderTest() {
        iArray = new int[4];
        res = new Resource();
    }
    static void foo() {
        RCHeaderTest rcht = new RCHeaderTest();
        c = Cleaner.create(rcht, new RCRunner());
        if (c == null) {
            System.out.println("Cleaner create failly");
        }
    }
    public static void main(String[] args) {
        foo();
        Runtime.getRuntime().runFinalization();
        System.out.println("ExpectResultEnd");    //ReleaseEnd
    }
}
