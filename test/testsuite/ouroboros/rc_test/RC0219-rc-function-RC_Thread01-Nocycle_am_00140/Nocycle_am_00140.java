/*
 *- @TestCaseID:maple/runtime/rc/function/RC_Thread01/Nocycle_am_00140.java
 *- @TestCaseName:MyselfClassName
 *- @RequirementName:[运行时需求]支持自动内存管理
 *- @Title/Destination:Change Nocycle_a_00140 in RC测试-No-Cycle-00.vsd to Multi thread testcase.
 *- @Condition: no
 * -#c1
 *- @Brief:functionTest
 * -#step1
 *- @Expect:ExpectResult\nExpectResult\n
 *- @Priority: High
 *- @Source: Nocycle_am_00140.java
 *- @ExecuteClass: Nocycle_am_00140
 *- @ExecuteArgs:
 *- @Remark:
 */
class ThreadRc_00140 extends Thread {
    private boolean checkout;

    public void run() {
        Nocycle_a_00140_A1 a1_main = new Nocycle_a_00140_A1("a1_main");
        Nocycle_a_00140_A2 a2_main = new Nocycle_a_00140_A2("a2_main");
        a1_main.b1_0 = new Nocycle_a_00140_B1("b1_0");
        a1_main.b2_0 = new Nocycle_a_00140_B2("b2_0");
        a2_main.b1_0 = new Nocycle_a_00140_B1("b1_0");
        a1_main.add();
        a2_main.add();
        a1_main.b1_0.add();
        a1_main.b2_0.add();
        a2_main.b1_0.add();
//		 System.out.printf("RC-Testing_Result=%d\n",a1_main.sum+a2_main.sum+a1_main.b1_0.sum+a1_main.b2_0.sum);

        int result = a1_main.sum + a2_main.sum + a1_main.b1_0.sum + a1_main.b2_0.sum;
        //System.out.println("RC-Testing_Result="+result);

        if (result == 1613)
            checkout = true;
        //System.out.println(checkout);
    }

    public boolean check() {
        return checkout;
    }

    class Nocycle_a_00140_A1 {
        Nocycle_a_00140_B1 b1_0;
        Nocycle_a_00140_B2 b2_0;
        int a;
        int sum;
        String strObjectName;

        Nocycle_a_00140_A1(String strObjectName) {
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

    class Nocycle_a_00140_A2 {
        Nocycle_a_00140_B1 b1_0;
        int a;
        int sum;
        String strObjectName;

        Nocycle_a_00140_A2(String strObjectName) {
            b1_0 = null;
            a = 102;
            sum = 0;
            this.strObjectName = strObjectName;
//	    System.out.println("RC-Testing_Construction_A2_"+strObjectName);
        }

        void add() {
            sum = a + b1_0.a;
        }
    }

    class Nocycle_a_00140_B1 {
        int a;
        int sum;
        String strObjectName;

        Nocycle_a_00140_B1(String strObjectName) {
            a = 201;
            sum = 0;
            this.strObjectName = strObjectName;
//	    System.out.println("RC-Testing_Construction_B1_"+strObjectName);
        }

        void add() {
            sum = a + a;
        }
    }

    class Nocycle_a_00140_B2 {
        int a;
        int sum;
        String strObjectName;

        Nocycle_a_00140_B2(String strObjectName) {
            a = 202;
            sum = 0;
            this.strObjectName = strObjectName;
//	    System.out.println("RC-Testing_Construction_B2_"+strObjectName);
        }

        void add() {
            sum = a + a;
        }
    }
}


public class Nocycle_am_00140 {

    public static void main(String[] args) {
        rc_testcase_main_wrapper();
	Runtime.getRuntime().gc();
	rc_testcase_main_wrapper();
        
    }

    private static void rc_testcase_main_wrapper() {
        ThreadRc_00140 A1_00140 = new ThreadRc_00140();
        ThreadRc_00140 A2_00140 = new ThreadRc_00140();
        ThreadRc_00140 A3_00140 = new ThreadRc_00140();
        ThreadRc_00140 A4_00140 = new ThreadRc_00140();
        ThreadRc_00140 A5_00140 = new ThreadRc_00140();
        ThreadRc_00140 A6_00140 = new ThreadRc_00140();

        A1_00140.start();
        A2_00140.start();
        A3_00140.start();
        A4_00140.start();
        A5_00140.start();
        A6_00140.start();

        try {
            A1_00140.join();
            A2_00140.join();
            A3_00140.join();
            A4_00140.join();
            A5_00140.join();
            A6_00140.join();

        } catch (InterruptedException e) {
        }
        if (A1_00140.check() && A2_00140.check() && A3_00140.check() && A4_00140.check() && A5_00140.check() && A6_00140.check())
            System.out.println("ExpectResult");
    }
}
// EXEC:%maple  %f %build_option -o %n.so
// EXEC:%run %n.so %n %run_option | compare %f
// ASSERT: scan-full ExpectResult\nExpectResult\n
