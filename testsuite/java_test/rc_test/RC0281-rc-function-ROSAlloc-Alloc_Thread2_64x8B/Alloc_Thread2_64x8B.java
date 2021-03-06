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


import java.util.ArrayList;
class Alloc_Thread2_64x8B_01 extends Thread {
    private static final int PAGE_SIZE = 4 * 1024;
    private static final int OBJ_HEADSIZE = 8;
    private static final int MAX_64_8B = 64 * 8;
    private static boolean checkout = false;
    public void run() {
        ArrayList<byte[]> store = new ArrayList<byte[]>();
        byte[] temp;
        for (int i = 480 - OBJ_HEADSIZE; i <= MAX_64_8B - OBJ_HEADSIZE; i++) {
            for (int j = 0; j < (PAGE_SIZE * 128 / (i + OBJ_HEADSIZE) + 10); j++) {
                temp = new byte[i];
                store.add(temp);
            }
        }
        int checkSize = store.size();
        if (checkSize == 35212) {
            checkout = true;
        }
    }
    public boolean check() {
        return checkout;
    }
}
public class Alloc_Thread2_64x8B {
    public static void main(String[] args) {
        Runtime.getRuntime().gc();
        Alloc_Thread2_64x8B_01 test1 = new Alloc_Thread2_64x8B_01();
        test1.start();
        Alloc_Thread2_64x8B_01 test2 = new Alloc_Thread2_64x8B_01();
        test2.start();
        try {
            test1.join();
            test2.join();
        } catch (InterruptedException e) {
            // do nothing
        }
        if (test1.check() && test2.check()) {
            System.out.println("ExpectResult");
        } else {
            System.out.println("Error");
        }
    }
}
