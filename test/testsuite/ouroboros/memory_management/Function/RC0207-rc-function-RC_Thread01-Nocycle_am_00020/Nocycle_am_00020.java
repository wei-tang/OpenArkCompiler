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
 * -@TestCaseID:maple/runtime/rc/function/RC_Thread01/Nocycle_am_00020.java
 *- @TestCaseName:MyselfClassName
 *- @RequirementName:[运行时需求]支持自动内存管理
 *- @Title/Destination:Change Nocycle_a_00020 in RC测试-No-Cycle-00.vsd to Multi thread testcase.
 *- @Brief:functionTest
 *- @Expect:ExpectResult\nExpectResult\n
 *- @Priority: High
 *- @Source: Nocycle_am_00020.java
 *- @ExecuteClass: Nocycle_am_00020
 *- @ExecuteArgs:
 */
class ThreadRc_00020 extends Thread {
    private boolean checkout;

    public void run() {
        Nocycle_a_00020_A1 a1_main = new Nocycle_a_00020_A1("a1_main");
        a1_main.b1_0 = new Nocycle_a_00020_B1("b1_0");
        a1_main.b2_0 = new Nocycle_a_00020_B2("b2_0");
        a1_main.add();
        a1_main.b1_0.add();
        a1_main.b2_0.add();
//         System.out.printf("RC-Testing_Result=%d\n",a1_main.sum+a1_main.b1_0.sum+a1_main.b2_0.sum);
        int result = a1_main.sum + a1_main.b1_0.sum + a1_main.b2_0.sum;
        //System.out.println("RC-Testing_Result_Thread1="+result);
        if (result == 1310)
            checkout = true;
        //System.out.println(checkout);

    }

    public boolean check() {
        return checkout;
    }

    class Nocycle_a_00020_A1 {
        Nocycle_a_00020_B1 b1_0;
        Nocycle_a_00020_B2 b2_0;
        int a;
        int sum;
        String strObjectName;

        Nocycle_a_00020_A1(String strObjectName) {
            b1_0 = null;
            b2_0 = null;
            a = 101;
            sum = 0;
            this.strObjectName = strObjectName;
//        System.out.println("RC-Testing_Construction_A1_"+strObjectName);
        }

        void add() {
            sum = a + b1_0.a + b2_0.a;
        }
    }

    class Nocycle_a_00020_B1 {
        int a;
        int sum;
        String strObjectName;

        Nocycle_a_00020_B1(String strObjectName) {
            a = 201;
            sum = 0;
            this.strObjectName = strObjectName;
//        System.out.println("RC-Testing_Construction_B1_"+strObjectName);
        }

        void add() {
            sum = a + a;
        }
    }

    class Nocycle_a_00020_B2 {
        int a;
        int sum;
        String strObjectName;

        Nocycle_a_00020_B2(String strObjectName) {
            a = 202;
            sum = 0;
            this.strObjectName = strObjectName;
//        System.out.println("RC-Testing_Construction_B2_"+strObjectName);
        }

        void add() {
            sum = a + a;
        }
    }
}


public class Nocycle_am_00020 {

    public static void main(String[] args) {
        rc_testcase_main_wrapper();
        Runtime.getRuntime().gc();
        rc_testcase_main_wrapper();

    }

    private static void rc_testcase_main_wrapper() {
        ThreadRc_00020 A1_00020 = new ThreadRc_00020();
        ThreadRc_00020 A2_00020 = new ThreadRc_00020();
        ThreadRc_00020 A3_00020 = new ThreadRc_00020();
        ThreadRc_00020 A4_00020 = new ThreadRc_00020();
        ThreadRc_00020 A5_00020 = new ThreadRc_00020();
        ThreadRc_00020 A6_00020 = new ThreadRc_00020();

        A1_00020.start();
        A2_00020.start();
        A3_00020.start();
        A4_00020.start();
        A5_00020.start();
        A6_00020.start();
        try {
            A1_00020.join();
            A2_00020.join();
            A3_00020.join();
            A4_00020.join();
            A5_00020.join();
            A6_00020.join();
        } catch (InterruptedException e) {
        }

        if (A1_00020.check() && A2_00020.check() && A3_00020.check() && A4_00020.check() && A5_00020.check() && A6_00020.check())
            System.out.println("ExpectResult");
    }
}
// EXEC:%maple  %f %build_option -o %n.so
// EXEC:%run %n.so %n %run_option | compare %f
// ASSERT: scan-full ExpectResult\nExpectResult\n