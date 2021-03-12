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


class Cycle_a_00190_A1 {
    Cycle_a_00190_A2 a2_0;
    int a;
    int sum;
    String strObjectName;
    Cycle_a_00190_A1(String strObjectName) {
        a2_0 = null;
        a = 101;
        sum = 0;
        this.strObjectName = strObjectName;
//	    System.out.println("RC-Testing_Construction_A1_"+strObjectName);
    }
    void add() {
        sum = a + a2_0.a;
    }
}
class Cycle_a_00190_A2 {
    Cycle_a_00190_A3 a3_0;
    int a;
    int sum;
    String strObjectName;
    Cycle_a_00190_A2(String strObjectName) {
        a3_0 = null;
        a = 102;
        sum = 0;
        this.strObjectName = strObjectName;
//	    System.out.println("RC-Testing_Construction_A2_"+strObjectName);
    }
    void add() {
        sum = a + a3_0.a;
    }
}
class Cycle_a_00190_A3 {
    Cycle_a_00190_A4 a4_0;
    int a;
    int sum;
    String strObjectName;
    Cycle_a_00190_A3(String strObjectName) {
        a4_0 = null;
        a = 103;
        sum = 0;
        this.strObjectName = strObjectName;
//	    System.out.println("RC-Testing_Construction_A3_"+strObjectName);
    }
    void add() {
        sum = a + a4_0.a;
    }
}
class Cycle_a_00190_A4 {
    Cycle_a_00190_A5 a5_0;
    int a;
    int sum;
    String strObjectName;
    Cycle_a_00190_A4(String strObjectName) {
        a5_0 = null;
        a = 104;
        sum = 0;
        this.strObjectName = strObjectName;
//	    System.out.println("RC-Testing_Construction_A4_"+strObjectName);
    }
    void add() {
        sum = a + a5_0.a;
    }
}
class Cycle_a_00190_A5 {
    Cycle_a_00190_A6 a6_0;
    int a;
    int sum;
    String strObjectName;
    Cycle_a_00190_A5(String strObjectName) {
        a6_0 = null;
        a = 105;
        sum = 0;
        this.strObjectName = strObjectName;
//	    System.out.println("RC-Testing_Construction_A5_"+strObjectName);
    }
    void add() {
        sum = a + a6_0.a;
    }
}
class Cycle_a_00190_A6 {
    Cycle_a_00190_A1 a1_0;
    Cycle_a_00190_A10 a10_0;
    int a;
    int sum;
    String strObjectName;
    Cycle_a_00190_A6(String strObjectName) {
        a1_0 = null;
        a10_0 = null;
        a = 106;
        sum = 0;
        this.strObjectName = strObjectName;
//	    System.out.println("RC-Testing_Construction_A6_"+strObjectName);
    }
    void add() {
        sum = a + a1_0.a + a10_0.a;
    }
}
class Cycle_a_00190_A7 {
    Cycle_a_00190_A5 a5_0;
    int a;
    int sum;
    String strObjectName;
    Cycle_a_00190_A7(String strObjectName) {
        a5_0 = null;
        a = 107;
        sum = 0;
        this.strObjectName = strObjectName;
//	    System.out.println("RC-Testing_Construction_A6_"+strObjectName);
    }
    void add() {
        sum = a + a5_0.a;
    }
}
class Cycle_a_00190_A8 {
    Cycle_a_00190_A9 a9_0;
    int a;
    int sum;
    String strObjectName;
    Cycle_a_00190_A8(String strObjectName) {
        a9_0 = null;
        a = 108;
        sum = 0;
        this.strObjectName = strObjectName;
//	    System.out.println("RC-Testing_Construction_A8_"+strObjectName);
    }
    void add() {
        sum = a + a9_0.a;
    }
}
class Cycle_a_00190_A9 {
    Cycle_a_00190_A7 a7_0;
    int a;
    int sum;
    String strObjectName;
    Cycle_a_00190_A9(String strObjectName) {
        a7_0 = null;
        a = 109;
        sum = 0;
        this.strObjectName = strObjectName;
//	    System.out.println("RC-Testing_Construction_A7_"+strObjectName);
    }
    void add() {
        sum = a + a7_0.a;
    }
}
class Cycle_a_00190_A10 {
    Cycle_a_00190_A8 a8_0;
    int a;
    int sum;
    String strObjectName;
    Cycle_a_00190_A10(String strObjectName) {
        a8_0 = null;
        a = 110;
        sum = 0;
        this.strObjectName = strObjectName;
//	    System.out.println("RC-Testing_Construction_A10_"+strObjectName);
    }
    void add() {
        sum = a + a8_0.a;
    }
}
public class Cycle_a_00190 {
    public static void main(String[] args) {
        rc_testcase_main_wrapper();
	Runtime.getRuntime().gc();
	rc_testcase_main_wrapper();
    }
    private static void rc_testcase_main_wrapper() {
        Cycle_a_00190_A1 a1_main = new Cycle_a_00190_A1("a1_main");
        Cycle_a_00190_A10 a10_main = new Cycle_a_00190_A10("a10_main");
        a1_main.a2_0 = new Cycle_a_00190_A2("a2_0");
        a1_main.a2_0.a3_0 = new Cycle_a_00190_A3("a3_0");
        a1_main.a2_0.a3_0.a4_0 = new Cycle_a_00190_A4("a4_0");
        a1_main.a2_0.a3_0.a4_0.a5_0 = new Cycle_a_00190_A5("a5_0");
        a1_main.a2_0.a3_0.a4_0.a5_0.a6_0 = new Cycle_a_00190_A6("a6_0");
        a1_main.a2_0.a3_0.a4_0.a5_0.a6_0.a1_0 = a1_main;
        a1_main.a2_0.a3_0.a4_0.a5_0.a6_0.a10_0 = a10_main;
        a10_main.a8_0 = new Cycle_a_00190_A8("a8_0");
        a10_main.a8_0.a9_0 = new Cycle_a_00190_A9("a9_0");
        a10_main.a8_0.a9_0.a7_0 = new Cycle_a_00190_A7("a7_0");
        a10_main.a8_0.a9_0.a7_0.a5_0 = a1_main.a2_0.a3_0.a4_0.a5_0;
        a1_main.add();
        a10_main.add();
        a1_main.a2_0.add();
        a1_main.a2_0.a3_0.add();
        a1_main.a2_0.a3_0.a4_0.add();
        a1_main.a2_0.a3_0.a4_0.a5_0.add();
        a1_main.a2_0.a3_0.a4_0.a5_0.a6_0.add();
        a10_main.a8_0.add();
        a10_main.a8_0.a9_0.add();
        a10_main.a8_0.a9_0.a7_0.add();
        int result = a1_main.sum + a10_main.sum + a1_main.a2_0.sum + a1_main.a2_0.a3_0.sum + a1_main.a2_0.a3_0.a4_0.sum + a1_main.a2_0.a3_0.a4_0.a5_0.sum + a1_main.a2_0.a3_0.a4_0.a5_0.a6_0.sum + a10_main.a8_0.sum + a10_main.a8_0.a9_0.sum + a10_main.a8_0.a9_0.a7_0.sum;
        if (result == 2215)
            System.out.println("ExpectResult");
    }
}
