/*
 * - @TestCaseID: MethodGetTypeParameters
 *- @RequirementName: Java Reflection
 *- @RequirementID: SR000B7N9H
 *- @TestCaseName:MethodGetTypeParameters.java
 * - @Title/Destination: Method.getTypeParameters() returns an array of TypeVariable objects.
 * - @Condition: no
 * - @Brief:no:
 *  -#step1: 定义含注解的内部类MyTargetTest15。
 *  -#step2：通过调用getMethod()从内部类MyTargetTest15中获取MyTargetTest_1。
 *  -#step3：调用getTypeParameters()获取TypeVariable对象的数组。
 *  -#step4：确认返回的数组长度是0。
 * - @Expect: 0\n
 * - @Priority: High
 * - @Remark:
 * - @Source: MethodGetTypeParameters.java
 * - @ExecuteClass: MethodGetTypeParameters
 * - @ExecuteArgs:
 */

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.reflect.Method;
import java.lang.reflect.TypeVariable;

public class MethodGetTypeParameters {
    @Retention(RetentionPolicy.RUNTIME)
    public @interface MyTarget {
        public String name();
        public String value();
    }

    public static void main(String[] args) {
        int result = 2;
        try {
            result = MethodGetTypeParameters1();
        } catch (Exception e) {
            e.printStackTrace();
            result = 3;
        }
        System.out.println(result);
    }

    public static int MethodGetTypeParameters1() {
        Method m;
        try {
            m = MyTargetTest15.class.getMethod("MyTargetTest_1");
            TypeVariable<Method>[] Target = m.getTypeParameters();
            if (Target.length == 0) {
                return 0;
            }
        } catch (NoSuchMethodException | SecurityException e) {
            e.printStackTrace();
        }
        return 2;
    }

    class MyTargetTest15 {
        @MyTarget(name = "newName", value = "newValue")
        public String home;

        @MyTarget(name = "name", value = "value")
        public void MyTargetTest_1() {
            System.out.println("This is Example:hello world");
        }

        public void newMethod(@MyTarget(name = "name1", value = "value1") String home) {
            System.out.println("my home at:" + home);
        }

        @MyTarget(name = "cons", value = "constructor")
        public MyTargetTest15(String home) {
            this.home = home;
        }
    }
}
// EXEC:%maple  %f %build_option -o %n.so
// EXEC:%run %n.so %n %run_option | compare %f
// ASSERT: scan-full 0\n
