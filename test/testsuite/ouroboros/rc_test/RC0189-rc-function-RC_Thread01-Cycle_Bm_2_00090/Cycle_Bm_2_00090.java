/*
 *- @TestCaseID:maple/runtime/rc/function/RC_Thread01/Cycle_Bm_2_00090.java
 *- @TestCaseName:MyselfClassName
 *- @RequirementName:[运行时需求]支持自动内存管理
 *- @Title/Destination:Change Cycle_B_2_00090 in RC测试-Cycle-00.vsd to Multi thread testcase.
 *- @Condition: no
 * -#c1
 *- @Brief:functionTest
 * -#step1
 *- @Expect:ExpectResult\nExpectResult\n
 *- @Priority: High
 *- @Source: Cycle_Bm_2_00090.java
 *- @ExecuteClass: Cycle_Bm_2_00090
 *- @ExecuteArgs:
 *- @Remark:
 */
class ThreadRc_Cycle_Bm_2_00090 extends Thread {
    private boolean checkout;

    public void run() {
        Cycle_B_2_00090_A1 a1_0 = new Cycle_B_2_00090_A1();
        Cycle_B_2_00090_A3 a3_0 = new Cycle_B_2_00090_A3();
        a1_0.a2_0 = new Cycle_B_2_00090_A2();
        a1_0.a2_0.a1_0 = a1_0;
        a3_0.a2_0 = a1_0.a2_0;
        a3_0.a2_0.a3_0 = a3_0;
        a1_0.add();
        a1_0.a2_0.add();
        a3_0.add();
        int nsum = (a1_0.sum + a1_0.a2_0.sum + a3_0.sum);
        //System.out.println(nsum);

        if (nsum == 9)
            checkout = true;
        //System.out.println(checkout);
    }

    public boolean check() {
        return checkout;
    }

    class Cycle_B_2_00090_A1 {
        Cycle_B_2_00090_A2 a2_0;
        int a;
        int sum;

        Cycle_B_2_00090_A1() {
            a2_0 = null;
            a = 1;
            sum = 0;
        }

        void add() {
            sum = a + a2_0.a;
        }
    }

    class Cycle_B_2_00090_A2 {
        Cycle_B_2_00090_A1 a1_0;
        Cycle_B_2_00090_A3 a3_0;
        int a;
        int sum;

        Cycle_B_2_00090_A2() {
            a1_0 = null;
            a3_0 = null;
            a = 2;
            sum = 0;
        }

        void add() {
            sum = a + a1_0.a;
        }
    }

    class Cycle_B_2_00090_A3 {
        Cycle_B_2_00090_A2 a2_0;
        int a;
        int sum;

        Cycle_B_2_00090_A3() {
            a2_0 = null;
            a = 1;
            sum = 0;
        }

        void add() {
            sum = a + a2_0.a;
        }
    }
}


public class Cycle_Bm_2_00090 {

    public static void main(String[] args) {
        rc_testcase_main_wrapper();
	Runtime.getRuntime().gc();
	rc_testcase_main_wrapper();
        
    }

    private static void rc_testcase_main_wrapper() {
        ThreadRc_Cycle_Bm_2_00090 A1_Cycle_Bm_2_00090 = new ThreadRc_Cycle_Bm_2_00090();
        ThreadRc_Cycle_Bm_2_00090 A2_Cycle_Bm_2_00090 = new ThreadRc_Cycle_Bm_2_00090();
        ThreadRc_Cycle_Bm_2_00090 A3_Cycle_Bm_2_00090 = new ThreadRc_Cycle_Bm_2_00090();
        ThreadRc_Cycle_Bm_2_00090 A4_Cycle_Bm_2_00090 = new ThreadRc_Cycle_Bm_2_00090();
        ThreadRc_Cycle_Bm_2_00090 A5_Cycle_Bm_2_00090 = new ThreadRc_Cycle_Bm_2_00090();
        ThreadRc_Cycle_Bm_2_00090 A6_Cycle_Bm_2_00090 = new ThreadRc_Cycle_Bm_2_00090();

        A1_Cycle_Bm_2_00090.start();
        A2_Cycle_Bm_2_00090.start();
        A3_Cycle_Bm_2_00090.start();
        A4_Cycle_Bm_2_00090.start();
        A5_Cycle_Bm_2_00090.start();
        A6_Cycle_Bm_2_00090.start();

        try {
            A1_Cycle_Bm_2_00090.join();
            A2_Cycle_Bm_2_00090.join();
            A3_Cycle_Bm_2_00090.join();
            A4_Cycle_Bm_2_00090.join();
            A5_Cycle_Bm_2_00090.join();
            A6_Cycle_Bm_2_00090.join();

        } catch (InterruptedException e) {
        }
        if (A1_Cycle_Bm_2_00090.check() && A2_Cycle_Bm_2_00090.check() && A3_Cycle_Bm_2_00090.check() && A4_Cycle_Bm_2_00090.check() && A5_Cycle_Bm_2_00090.check() && A6_Cycle_Bm_2_00090.check())
            System.out.println("ExpectResult");
    }
}
// EXEC:%maple  %f %build_option -o %n.so
// EXEC:%run %n.so %n %run_option | compare %f
// ASSERT: scan-full ExpectResult\nExpectResult\n
