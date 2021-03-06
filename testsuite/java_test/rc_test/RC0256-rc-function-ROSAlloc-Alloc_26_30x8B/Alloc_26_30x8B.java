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
public class Alloc_26_30x8B {
    private  final static int PAGE_SIZE=4*1024;
    private final static int OBJ_HEADSIZE = 8;
    private final static int MAX_26_8B=26*8;
    private final static int MAX_27_8B=27*8;
    private final static int MAX_28_8B=28*8;
    private final static int MAX_29_8B=29*8;
    private final static int MAX_30_8B=30*8;
    private static ArrayList<byte[]> store;
    private static int alloc_test(int slot_type){
        store=new ArrayList<byte[]>();
        byte[] temp;
        int i;
        if(slot_type==24){
            i=1;}
        else if(slot_type==1024){
            i=64*8+1-OBJ_HEADSIZE;
        }else{
            i=slot_type-2*8+1;
        }
        for(;i<=slot_type-OBJ_HEADSIZE;i++)
        {
            for(int j=0;j<(PAGE_SIZE*3/(i+OBJ_HEADSIZE)+10);j++)
            {
                temp = new byte[i];
                store.add(temp);
            }
        }
        int check_size=store.size();
        store=new ArrayList<byte[]>();
        return check_size;
    }
    public static void main(String[] args) {
        store = new ArrayList<byte[]>();
        int countSize26 = alloc_test(MAX_26_8B);
        int countSize27 = alloc_test(MAX_27_8B);
        int countSize28 = alloc_test(MAX_28_8B);
        int countSize29 = alloc_test(MAX_29_8B);
        int countSize30 = alloc_test(MAX_30_8B);
        //System.out.println(countSize26);
        //System.out.println(countSize27);
        //System.out.println(countSize28);
        //System.out.println(countSize29);
        //System.out.println(countSize30);
        if (countSize26 == 557 && countSize27 == 538 && countSize28 == 522 && countSize29 == 506 && countSize30 == 492)
            System.out.println("ExpectResult");
        else
            System.out.println("Error");
    }
}
