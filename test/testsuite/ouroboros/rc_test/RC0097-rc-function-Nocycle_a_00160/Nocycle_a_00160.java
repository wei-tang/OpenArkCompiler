/*
 *- @TestCaseID:maple/runtime/rc/function/Nocycle_a_00160.java
 *- @TestCaseName:MyselfClassName
 *- @RequirementName:[运行时需求]支持自动内存管理
 *- @Title/Destination: Nocycle_a_00160 in RC测试-No-Cycle-00.vsd.
 *- @Condition: no
 * -#c1
 *- @Brief:functionTest
 * -#step1
 *- @Expect:ExpectResult\nExpectResult\n
 *- @Priority: High
 *- @Source: Nocycle_a_00160.java
 *- @ExecuteClass: Nocycle_a_00160
 *- @ExecuteArgs:
 *- @Remark:
 */
class Nocycle_a_00160_A1 {
    Nocycle_a_00160_B1 b1_0;
    Nocycle_a_00160_B2 b2_0;
    int a;
    int sum;
    String strObjectName;

    Nocycle_a_00160_A1(String strObjectName) {
        b1_0 = null;
        b2_0 = null;
        a = 101;
        sum = 0;
        this.strObjectName = strObjectName;
//	    System.out.println("RC-Testing_Construction_A1_"+strObjectName);
    }

    void add() {
        sum = a + b1_0.a + b2_0.a;
    }
}

class Nocycle_a_00160_A2 {
    Nocycle_a_00160_B1 b1_0;
    Nocycle_a_00160_B2 b2_0;
    int a;
    int sum;
    String strObjectName;

    Nocycle_a_00160_A2(String strObjectName) {
        b1_0 = null;
        b2_0 = null;
        a = 102;
        sum = 0;
        this.strObjectName = strObjectName;
//	    System.out.println("RC-Testing_Construction_A2_"+strObjectName);
    }

    void add() {
        sum = a + b1_0.a + b2_0.a;
    }
}


class Nocycle_a_00160_A3 {
    Nocycle_a_00160_B1 b1_0;
    Nocycle_a_00160_B2 b2_0;
    int a;
    int sum;
    String strObjectName;

    Nocycle_a_00160_A3(String strObjectName) {
        b1_0 = null;
        b2_0 = null;
        a = 103;
        sum = 0;
        this.strObjectName = strObjectName;
//	    System.out.println("RC-Testing_Construction_A3_"+strObjectName);
    }

    void add() {
        sum = a + b1_0.a + b2_0.a;
    }
}

class Nocycle_a_00160_A4 {
    Nocycle_a_00160_B1 b1_0;
    Nocycle_a_00160_B2 b2_0;
    int a;
    int sum;
    String strObjectName;

    Nocycle_a_00160_A4(String strObjectName) {
        b1_0 = null;
        b2_0 = null;
        a = 104;
        sum = 0;
        this.strObjectName = strObjectName;
//	    System.out.println("RC-Testing_Construction_A4_"+strObjectName);
    }

    void add() {
        sum = a + b1_0.a + b2_0.a;
    }
}


class Nocycle_a_00160_B1 {
    int a;
    int sum;
    String strObjectName;

    Nocycle_a_00160_B1(String strObjectName) {
        a = 201;
        sum = 0;
        this.strObjectName = strObjectName;
//	    System.out.println("RC-Testing_Construction_B1_"+strObjectName);
    }

    void add() {
        sum = a + a;
    }
}

class Nocycle_a_00160_B2 {
    int a;
    int sum;
    String strObjectName;

    Nocycle_a_00160_B2(String strObjectName) {
        a = 202;
        sum = 0;
        this.strObjectName = strObjectName;
//	    System.out.println("RC-Testing_Construction_B2_"+strObjectName);
    }

    void add() {
        sum = a + a;
    }
}

public class Nocycle_a_00160 {

    public static void main(String[] args) {
        rc_testcase_main_wrapper();
	Runtime.getRuntime().gc();
	rc_testcase_main_wrapper();
        
    }

    private static void rc_testcase_main_wrapper() {
        Nocycle_a_00160_A1 a1_main = new Nocycle_a_00160_A1("a1_main");
        Nocycle_a_00160_A2 a2_main = new Nocycle_a_00160_A2("a2_main");
        Nocycle_a_00160_A3 a3_main = new Nocycle_a_00160_A3("a3_main");
        Nocycle_a_00160_A4 a4_main = new Nocycle_a_00160_A4("a4_main");
        a1_main.b1_0 = new Nocycle_a_00160_B1("b1_0");
        a1_main.b2_0 = new Nocycle_a_00160_B2("b2_0");
        a2_main.b1_0 = new Nocycle_a_00160_B1("b1_0");
        a2_main.b2_0 = new Nocycle_a_00160_B2("b2_0");
        a3_main.b1_0 = new Nocycle_a_00160_B1("b1_0");
        a3_main.b2_0 = new Nocycle_a_00160_B2("b2_0");
        a4_main.b1_0 = new Nocycle_a_00160_B1("b1_0");
        a4_main.b2_0 = new Nocycle_a_00160_B2("b2_0");
        a1_main.add();
        a2_main.add();
        a3_main.add();
        a4_main.add();
        a1_main.b1_0.add();
        a1_main.b2_0.add();
        a2_main.b1_0.add();
        a2_main.b2_0.add();
        a3_main.b1_0.add();
        a3_main.b2_0.add();
        a4_main.b1_0.add();
        a4_main.b2_0.add();

        int result = a1_main.sum + a2_main.sum + a3_main.sum + a4_main.sum + a1_main.b1_0.sum + a1_main.b2_0.sum;

        if (result == 2828)
            System.out.println("ExpectResult");
    }

}

// EXEC:%maple  %f %build_option -o %n.so
// EXEC:%run %n.so %n %run_option | compare %f
// ASSERT: scan-full ExpectResult\nExpectResult\n
