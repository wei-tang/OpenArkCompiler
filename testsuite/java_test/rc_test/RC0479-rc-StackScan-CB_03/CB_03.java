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


import java.util.HashMap;
class CB_03_A1 {
    static HashMap test1;
    static
    int a;
    CB_03_A2 a2_0;
    CB_03_A3 a3_0;
    int sum;
    String strObjectName;
    CB_03_A1(String strObjectName) {
        a2_0 = null;
        a3_0 = null;
        a = 101;
        sum = 0;
        this.strObjectName = strObjectName;
    }
    void add() {
        sum = a + a2_0.a + a3_0.a;
    }
    @Override
    public void finalize() throws Throwable {
        super.finalize();
        CB_03.check = this;
    }
}
class CB_03_A2 {
    CB_03_A1 a1_0;
    CB_03_A3 a3_0;
    volatile HashMap test2;
    int a;
    int sum;
    String strObjectName;
    CB_03_A2(String strObjectName) {
        a1_0 = null;
        a3_0 = null;
        a = 102;
        sum = 0;
        this.strObjectName = strObjectName;
    }
    void add() {
        sum = a + CB_03_A1.a + a3_0.a;
    }
    @Override
    public void finalize() throws Throwable {
        super.finalize();
    }
}
class CB_03_A3 {
    CB_03_A1 a1_0;
    CB_03_A2 a2_0;
    int a;
    int sum;
    String strObjectName;
    CB_03_A3(String strObjectName) {
        a1_0 = null;
        a2_0 = null;
        a = 103;
        sum = 0;
        this.strObjectName = strObjectName;
    }
    void add() {
        sum = a + CB_03_A1.a + a2_0.a;
    }
}
public class CB_03 {
    public volatile static CB_03_A1 check = null;
    private static CB_03_A1 a1_main = null;
    private CB_03() {
        a1_main = new CB_03_A1("a1_main");
        a1_main.a2_0 = new CB_03_A2("a2_0");
        a1_main.a2_0.a1_0 = a1_main;
        a1_main.a2_0.a3_0 = new CB_03_A3("a3_0");
        a1_main.a3_0 = a1_main.a2_0.a3_0;
        a1_main.a3_0.a1_0 = a1_main;
        a1_main.a3_0.a2_0 = a1_main.a2_0;
        a1_main.add();
        a1_main.a2_0.add();
        a1_main.a2_0.a3_0.add();
    }
    private static void test_CB_03(int times) {
        CB_03_A1.test1 = new HashMap();
        a1_main.a2_0.test2 = new HashMap();
        for (int i = 0; i < times; i++) {
            CB_03_A1.test1.put(i, new StringBuffer("Maple test").append(i));
            a1_main.a2_0.test2.put(i, new StringBuffer("Figo test").append(i));
        }
    }
    private static void rc_testcase_main_wrapper() {
        CB_03 cb01 = new CB_03();
        test_CB_03(100000);
        check = a1_main;
        try {
            int result = CB_03.check.sum + CB_03.check.a2_0.sum + CB_03.check.a2_0.a3_0.sum + CB_03_A1.test1.size() + check.a2_0.test2.size();
            if (result == 200918)
                System.out.println("ExpectResult");
        } catch (NullPointerException n) {
            System.out.println("ErrorResult");
        }
    }
    public static void main(String[] args) {
        rc_testcase_main_wrapper();
    }
}
