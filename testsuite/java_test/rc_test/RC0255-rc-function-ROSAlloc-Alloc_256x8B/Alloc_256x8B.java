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
public class Alloc_256x8B {
    private  final static int PAGE_SIZE=4*1024;
    private final static int OBJ_HEADSIZE = 8;
    private final static int MAX_256_8B=256*8;
    private static ArrayList<byte[]> store;
    private static int alloc_test(int slot_type){
        int sum=0;
        store=new ArrayList<byte[]>();
        byte[] temp;
        for(int i=1024+1-OBJ_HEADSIZE;i<=slot_type-OBJ_HEADSIZE;i=i+256)
        {
            for(int j=0;j<(PAGE_SIZE*5120/(i+OBJ_HEADSIZE)+5);j++)
            {
                temp = new byte[i];
                store.add(temp);
            }
            sum +=store.size();
            store=new ArrayList<byte[]>();
        }
        return sum;
    }
    public static void main(String[] args) {
        store = new ArrayList<byte[]>();
        int result = alloc_test(MAX_256_8B);
        //System.out.println(result);
        if ( result == 62191)
            System.out.println("ExpectResult");
        else
            System.out.println("Error");
    }
}
