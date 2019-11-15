/*
 *  * Copyright (c) 2016-2017 Wuklab, Purdue University. All rights reserved.
 *   *
 *    * This program is free software; you can redistribute it and/or modify
 *     * it under the terms of the GNU General Public License as published by
 *      * the Free Software Foundation; either version 2 of the License, or
 *       * (at your option) any later version.
 *        */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/mutex.h>

#define MAX_FILENAME_LENGTH 100
static DEFINE_SPINLOCK(lock);

struct mc_msg_queue{
        char* msg_data;
        unsigned int msg_size;
        struct list_head list;
};


unsigned int append(char* msg_data, unsigned int msg_size, struct list_head* name_nid_dict, struct spinlock_t *lock){
        struct mc_msg_queue* tmp;

        tmp = kmalloc(sizeof(struct mc_msg_queue), GFP_KERNEL);
        if(!tmp){
                printk("allocated wrong\n");
                return 0;
        }


	tmp->msg_data = kmalloc(sizeof(char)*(msg_size+1), GFP_KERNEL);
        strcpy(tmp->msg_data, msg_data);
        tmp->msg_size = msg_size;

	unsigned long flags;
	spin_lock_irqsave(lock, flags);
        list_add_tail(&tmp->list, name_nid_dict);
	spin_unlock_irqrestore(lock, flags);

        return 1;
        /* return error
 *         */
}


/* the msg data passed in should copy a new memory here, msg_data should point to a continous memory
 * */
unsigned int pop(char* msg_data, int* msg_size, struct list_head* name_nid_dict, struct spinlock_t *lock){
        struct list_head* next = name_nid_dict->next;

        struct mc_msg_queue* item = list_entry(next, struct mc_msg_queue, list);

	unsigned long flags;
        spin_lock_irqsave(lock, flags);
        strcpy(msg_data, item->msg_data);
        *msg_size = item->msg_size;
        list_del(&item->list);
	spin_unlock_irqrestore(lock, flags);

        kfree(item->msg_data);
        kfree(item);
        name_nid_dict = next;

        return 1;
}

/*
 *  * print name_nid_dict
 *   */


void print(struct list_head* name_nid_dict){
        struct mc_msg_queue *pos = NULL;
        list_for_each_entry(pos, name_nid_dict, list){
                printk(pos->msg_data);
                printk(" ");
                printk("message data: %d\n", pos->msg_size);
        }
}

void free_all(struct list_head* name_nid_dict, struct spinlock_t *lock){
	unsigned long flags;
	spin_lock_irqsave(lock, flags);
        struct list_head* cur=name_nid_dict->next;
        while(cur != name_nid_dict){
                struct mc_msg_queue* item = list_entry(cur, struct mc_msg_queue, list);
                printk("message freed:%s \n", item->msg_data);
                struct list_head* tmp = cur->next;
                list_del(cur);

                cur = tmp;
                kfree(item->msg_data);
                kfree(item);
        }
	spin_unlock_irqrestore(lock, flags);
}


LIST_HEAD(yi_list);

static int mq_test_module_init(void)
{
        printk("loaded yi\n");
	spin_lock_init(&lock);
        append("NVSL", 4, &yi_list, &lock);
        append("STABLE", 6, &yi_list, &lock);
        append("WUKLAB", 6, &yi_list,&lock);
        char* msg = kmalloc(sizeof(char)*(MAX_FILENAME_LENGTH+1), GFP_KERNEL);
        int size;
        pop(msg,&size, &yi_list, &lock);
        printk("%s: %d\n", msg, size);
        kfree(msg);

        print(&yi_list);
        return 0;
}


static void mq_test_module_exit(void)
{
        printk("offload! \n");
        free_all(&yi_list, &lock);
        printk("free done\n");

}

module_init(mq_test_module_init);
module_exit(mq_test_module_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Wuklab@Purdue");
