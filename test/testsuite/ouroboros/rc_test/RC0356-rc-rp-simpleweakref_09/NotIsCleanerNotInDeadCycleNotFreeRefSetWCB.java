/*
 *- @TestCaseID: RefProCase/RefProcessor/src/NotIsCleanerNotInDeadCycleNotFreeRefSetWCB.java
 *- @TestCaseName:MyselfClassName
 *- @RequirementName:[运行时需求]支持自动内存管理
 *- @Title/Destination: RefProcessor basic testcase: 在可达的环中，设置非cleaner对象，且不释放对象，之后设置WCB
 *- @Condition: no
 *- @Brief:functionTest
 * -#step1：创建两个带有弱引用对象的class，弱引用申请后通过get方法判断非空，并相互声明对方类作为自己的Field。
 * -#step2：其中class A1会分别对软引用、弱引用、虚引用进行clear和enqueue，其中虚引用分为没有Referent finalized的clear和enqueue
 *          和有Referent finalized的clear和enqueue。
 * -#step3：创建第三个类InCycle，将上两个类连成一个可达的环。
 * -#step4：创建InCycle的实例，调用方法setCycle给第一个类对象关于第二个类的Field初始化对象，再将该对象关于第一个类Field赋值为第一
 *          个类，从而形成一个环。
 * -#step5：方法setCycle调用方法ModifyA1执行前两个类对象的add方法进行Field的运算，相当于对对象进行了引用，判断运算结果是否正确。
 * -#step6：确认两个类的软引用、弱引用、虚引用对象为NULL。
 * -#step7：调用Runtime.getRuntime().gc()进行资源回收，重复步骤3~6。
 * -#step8：确认class A1的软引用、弱引用、虚引用都应该为空。
 *- @Expect: ExpectResult\n
 *- @Priority: High
 *- @Source:NotIsCleanerNotInDeadCycleNotFreeRefSetWCB.java
 *- @ExecuteClass: NotIsCleanerNotInDeadCycleNotFreeRefSetWCB
 *- @ExecuteArgs:
 *- @Remark:
 *
 */

import java.io.PrintStream;
import java.lang.ref.*;

public class NotIsCleanerNotInDeadCycleNotFreeRefSetWCB {
    static int TEST_NUM = 1;
    static int judgeNum = 0;

    public static void main(String[] args) throws Exception {
        judgeNum = 0;
        for (int i = 0; i < TEST_NUM; i++) {
            notIsCleanerNotInDeadCycleNotFreeRefSetWCB();
        }
        if (judgeNum == 0) {
            System.out.println("ExpectResult");
        }
    }

    static boolean resultSetCycle(Cycle_BDec_00010_A1 a1) throws InterruptedException {
        InCycle cycleA = new InCycle();
        cycleA.setCycle(a1);
        Runtime.getRuntime().gc();
        boolean result = cycleA.setCycle(a1);
        return result;
    }

    static void notIsCleanerNotInDeadCycleNotFreeRefSetWCB() throws InterruptedException {
        Cycle_BDec_00010_A1 cycleA = new Cycle_BDec_00010_A1();
        boolean result = resultSetCycle(cycleA);
        cycleA.partner2 = null;
        if (result == true) {
            if (cycleA.clearSoftReference() == true) {
                if (cycleA.clearWeakReference() == true) {
                    if (cycleA.clearFinalizePhantomReference() == true) {
                        if (cycleA.clearNoFinalizePhantomReference() == true) {
                        } else {
                            judgeNum++;
                            System.out.println(Thread.currentThread().getStackTrace()[2].getClassName() +
                                    " ErrorResult in clearNoFinalizePhantomReference--------------" +
                                    "NotIsCleanerNotInDeadCycleNotFreeRefSetWCB");
                        }
                    } else {
                        judgeNum++;
                        System.out.println(Thread.currentThread().getStackTrace()[2].getClassName() +
                                " ErrorResult in clearFinalizePhantomReference------------------" +
                                "NotIsCleanerNotInDeadCycleNotFreeRefSetWCB");
                    }
                } else {
                    judgeNum++;
                    System.out.println(Thread.currentThread().getStackTrace()[2].getClassName() +
                            " ErrorResult in clearWeakReference---------------------" +
                            "NotIsCleanerNotInDeadCycleNotFreeRefSetWCB");
                }
            } else {
                judgeNum++;
                System.out.println(Thread.currentThread().getStackTrace()[2].getClassName() +
                        " ErrorResult in clearSoftReference-------------------" +
                        "NotIsCleanerNotInDeadCycleNotFreeRefSetWCB");
            }
        } else {
            judgeNum++;
            System.out.println(Thread.currentThread().getStackTrace()[2].getClassName() +
                    " ErrorResult in isCleanerNotInDeadCycleNotFreeRefSetWCB---------------------" +
                    "NotIsCleanerNotInDeadCycleNotFreeRefSetWCB");
        }
    }
}

class Cycle_BDec_00010_A1 {
    Reference a1Ref;
    static ReferenceQueue rq = new ReferenceQueue();
    Cycle_BDec_00010_A2 partner2;
    int num;
    int sum;
    static int value;
    static StringBuffer obj = new StringBuffer("weak");

    Cycle_BDec_00010_A1() throws InterruptedException {
        obj = new StringBuffer("weak");
        a1Ref = new WeakReference<Object>(obj, rq);
        if (a1Ref.get() == null) {
            System.out.println("error in a1Ref.get");
        }
        obj = null;
        partner2 = null;
        num = 1;
        sum = 0;
        value = 100;
    }

    void add() {
        sum = num + partner2.num;
    }

    /**
     * 设置软引用, clear and enqueue
     */
    static Reference srp, wrp, prp;
    static ReferenceQueue srq = new ReferenceQueue();
    static ReferenceQueue wrq = new ReferenceQueue();
    static ReferenceQueue prq = new ReferenceQueue();
    static StringBuffer obj1 = new StringBuffer("soft");
    static StringBuffer obj2 = new StringBuffer("weak");
    static StringBuffer obj3 = new StringBuffer("obj");

    static void setSoftReference() {
        obj1 = new StringBuffer("soft");
        srp = new WeakReference<Object>(obj1, srq);
        if (srp.get() == null) {
            value++;
        }
        obj1 = null;
    }

    static boolean clearSoftReference() throws InterruptedException {
        int value = 100;
        Reference ref;
        setSoftReference();
        new Thread(new ThreadRef()).start();
        Thread.sleep(2000);
        // 释放，应该为空
        if (srp.get() != null) {
            value++;
        }
        while ((ref = srq.poll()) != null) {
            if (!ref.getClass().toString().equals("class java.lang.ref.WeakReference")) {
                value++;
            }
        }
        if (value == 100) {
            return true;
        } else {
            return false;
        }
    }

    /**
     * 设置弱引用, clear and enqueue
     */
    static void setWeakReference() {
        obj2 = new StringBuffer("weak");
        wrp = new WeakReference<Object>(obj2, wrq);
        if (wrp.get() == null) {
            value++;
        }
        obj2 = null;
    }

    static boolean clearWeakReference() throws InterruptedException {
        value = 100;
        Reference reference;
        setWeakReference();
        new Thread(new ThreadRef()).start();
        Thread.sleep(2000);
        // 释放，应该为空
        if (wrp.get() != null) {
            value++;
        }
        while ((reference = wrq.poll()) != null) {
            if (!reference.getClass().toString().equals("class java.lang.ref.WeakReference")) {
                value++;
            }
        }
        if (value == 100) {
            return true;
        } else {
            return false;
        }
    }

    /**
     * 设置虚引用, 没有Referent finalized, clear and enqueue.
     */
    static void setPhantomReference() {
        obj3 = new StringBuffer("obj");
        prp = new PhantomReference<Object>(obj3, prq);
        if (prp.get() != null) {
            value++;
        }
        obj3 = null;
    }

    static boolean clearNoFinalizePhantomReference() throws InterruptedException {
        value = 100;
        Reference reference;
        setPhantomReference();
        Thread.sleep(2000);
        while ((reference = prq.poll()) != null) {
            if (!reference.getClass().toString().equals("class java.lang.ref.PhantomReference")) {
                value++;
            }
        }
        if (value == 100) {
            return true;
        } else {
            return false;
        }
    }

    static int finalSet() {
        RC_Finalize testClass1 = new RC_Finalize();
        System.runFinalization();
        return testClass1.value;
    }

    /**
     * 设置虚引用, 经过Referent finalized，clear and enqueue
     */
    static boolean clearFinalizePhantomReference() throws InterruptedException {
        value = 100;
        int valueFinal = finalSet();
        Runtime.getRuntime().gc();
        valueFinal = finalSet();
        Reference reference;
        setPhantomReference();
        Thread.sleep(5000);
        if (valueFinal == 200) {
            value = value + 100;
        }
        while ((reference = prq.poll()) != null) {
            if (!reference.getClass().toString().equals("class java.lang.ref.PhantomReference")) {
                value++;
            }
        }
        if (value == 200) {
            return true;
        } else {
            return false;
        }
    }
}

class Cycle_BDec_00010_A2 {
    Reference a2Ref;
    static ReferenceQueue rq = new ReferenceQueue();
    Cycle_BDec_00010_A1 partner1;
    int num;
    int sum;
    static int value;
    static StringBuffer obj = new StringBuffer("weak");

    Cycle_BDec_00010_A2() throws InterruptedException {
        obj = new StringBuffer("weak");
        a2Ref = new WeakReference<Object>(obj, rq);
        if (a2Ref.get() == null) {
            assert false;
        }
        obj = null;

        partner1 = null;
        num = 2;
        sum = 0;
        value = 100;
    }

    void add() {
        sum = num + partner1.num;
    }
}

class RC_Finalize {
    static int value = 0;

    static void rc_Finalize() throws InterruptedException {
        RC_Finalize a = new RC_Finalize();
        Thread.sleep(2000);
        System.runFinalization();
    }

    protected void finalize() throws Throwable {
        super.finalize();
        value = 200;
    }

    public static int run(String argv[], PrintStream out) {
        return 0;
    }
}

class ThreadRef implements Runnable {
    public void run() {
        for (int i = 0; i < 60; i++) {
            WeakReference wr = new WeakReference(new Object());
            try {
                Thread.sleep(100);
            } catch (Exception e) {
                e.printStackTrace();
            }
        }
    }
}

class InCycle {
    /**
     * 确认环是正确的
     *
     * @param refInstance 传入的是带有Referent的类实例
     * @return true:正确；false：错误
     */
    public static boolean ModifyA1(Cycle_BDec_00010_A1 refInstance) {
        refInstance.add();
        refInstance.partner2.add();
        int nSum = refInstance.sum + refInstance.partner2.sum;
        if (nSum == 6) {
            return true;
        } else {
            return false;
        }
    }

    /**
     * 设置一个带Referent的环
     *
     * @param refInstance 传入的是带有Referent的类实例
     * @return true:正确；false：错误
     */
    public static boolean setCycle(Cycle_BDec_00010_A1 refInstance) throws InterruptedException {
        refInstance.partner2 = new Cycle_BDec_00010_A2();
        refInstance.partner2.partner1 = refInstance;
        refInstance.obj = null;
        refInstance.partner2.obj = null;
        Thread.sleep(2000);
        boolean ret;
        ret = ModifyA1(refInstance);

        if (ret == true) {
            if (refInstance.a1Ref.get() == null) {
                if (refInstance.partner2.a2Ref.get() == null) {
                    if (refInstance != null && refInstance.partner2 != null) {
                        return true;
                    } else {
                        System.out.println(Thread.currentThread().getStackTrace()[2].getClassName()
                                + " a1_0 == null || a1_0.a2_0 == null");
                        return false;
                    }
                } else {
                    System.out.println(Thread.currentThread().getStackTrace()[2].getClassName()
                            + " a2Ref.get() == null");
                    return false;
                }
            } else {
                System.out.println(Thread.currentThread().getStackTrace()[2].getClassName()
                        + " a1_0.a1Ref.get() == null");
                return false;
            }
        } else {
            System.out.println(Thread.currentThread().getStackTrace()[2].getClassName() + " ret != true");
            return false;
        }
    }
}
// EXEC:%maple  %f %build_option -o %n.so
// EXEC:%run %n.so %n %run_option | compare %f
