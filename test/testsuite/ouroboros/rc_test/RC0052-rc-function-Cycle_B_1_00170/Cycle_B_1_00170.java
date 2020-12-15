/*
 *- @TestCaseID:maple/runtime/rc/function/Cycle_B_1_00170.java
 *- @TestCaseName:MyselfClassName
 *- @RequirementName:[运行时需求]支持自动内存管理
 *- @Title/Destination: Cycle_B_1_00170 in RC测试-Cycle-00.vsd
 *- @Condition: no
 * -#c1
 *- @Brief:functionTest
 * -#step1
 *- @Expect:ExpectResult\nExpectResult\n
 *- @Priority: High
 *- @Source: Cycle_B_1_00170.java
 *- @ExecuteClass: Cycle_B_1_00170
 *- @ExecuteArgs:
 *- @Remark:
 */
class Cycle_B_1_00170_A1 {
    Cycle_B_1_00170_A2 a2_0;
    int a;
    int sum;

    Cycle_B_1_00170_A1() {
        a2_0 = null;
        a = 1;
        sum = 0;
    }

    void add() {
        sum = a + a2_0.a;
    }
}


class Cycle_B_1_00170_A2 {
    Cycle_B_1_00170_A3 a3_0;
    int a;
    int sum;

    Cycle_B_1_00170_A2() {
        a3_0 = null;
        a = 2;
        sum = 0;
    }

    void add() {
        sum = a + a3_0.a;
    }
}


class Cycle_B_1_00170_A3 {
    Cycle_B_1_00170_A1 a1_0;
    int a;
    int sum;

    Cycle_B_1_00170_A3() {
        a1_0 = null;
        a = 3;
        sum = 0;
    }

    void add() {
        sum = a + a1_0.a;
    }
}

class Cycle_B_1_00170_A4 {
    Cycle_B_1_00170_A1 a1_0;
    int a;
    int sum;

    Cycle_B_1_00170_A4() {
        a1_0 = null;
        a = 4;
        sum = 0;
    }

    void add() {
        sum = a + a1_0.a;
    }
}


class Cycle_B_1_00170_A5 {
    Cycle_B_1_00170_A4 a4_0;
    int a;
    int sum;

    Cycle_B_1_00170_A5() {
        a4_0 = null;
        a = 5;
        sum = 0;
    }

    void add() {
        sum = a + a4_0.a;
    }
}


public class Cycle_B_1_00170 {

    public static void main(String[] args) {
        rc_testcase_main_wrapper();
	Runtime.getRuntime().gc();
	rc_testcase_main_wrapper();
        
    }

    private static void rc_testcase_main_wrapper() {
        Cycle_B_1_00170_A1 a1_0 = new Cycle_B_1_00170_A1();
        a1_0.a2_0 = new Cycle_B_1_00170_A2();
        a1_0.a2_0.a3_0 = new Cycle_B_1_00170_A3();
        Cycle_B_1_00170_A4 a4_0 = new Cycle_B_1_00170_A4();
        Cycle_B_1_00170_A5 a5_0 = new Cycle_B_1_00170_A5();
        a5_0.a4_0 = a4_0;
        a4_0.a1_0 = a1_0;
        a1_0.a2_0.a3_0.a1_0 = a1_0;
        a1_0.add();
        a1_0.a2_0.add();
        a1_0.a2_0.a3_0.add();
        a4_0.add();
        a5_0.add();
        int nsum = (a1_0.sum + a1_0.a2_0.sum + a1_0.a2_0.a3_0.sum + a4_0.sum + a5_0.sum);
        //System.out.println(nsum);
        if (nsum == 26)
            System.out.println("ExpectResult");
    }

}
// EXEC:%maple  %f %build_option -o %n.so
// EXEC:%run %n.so %n %run_option | compare %f
// ASSERT: scan-full ExpectResult\nExpectResult\n
