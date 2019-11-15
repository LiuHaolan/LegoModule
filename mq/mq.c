/*
 * Copyright (c) 2016-2017 Wuklab, Purdue University. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/spinlock.h>

#define MAX_FILENAME_LENGTH 100

/* list api
 * this list api
 */

struct mc_msg_queue{
	char* msg_data;
	unsigned int msg_size;
	struct list_head list;
};

/* we also need a lock to protect name_mq_map */
static DEFINE_SPINLOCK(map_lock);

struct name_mq_map{
	char mq_name[MAX_FILENAME_LENGTH];
	unsigned int max_size;
	struct list_head* mq;
	struct list_head list;
	spinlock_t mq_lock;
};

unsigned int append(char* msg_data, unsigned int msg_size, struct list_head* name_nid_dict){
	struct mc_msg_queue* tmp;
	
	tmp = kmalloc(sizeof(struct mc_msg_queue), GFP_KERNEL);
	if(!tmp){
		printk("allocated wrong\n");
		return 0;
	}

	tmp->msg_data = kmalloc(sizeof(char)*(msg_size+1), GFP_KERNEL);
	strcpy(tmp->msg_data, msg_data);
	tmp->msg_size = msg_size;

	list_add_tail(&tmp->list, name_nid_dict);	

	return 1;
	/* return error
	*/
}



/* the msg data passed in should copy a new memory here, msg_data should point to a continous memory
*/
unsigned int pop(char* msg_data, int* msg_size, struct list_head* name_nid_dict){
	struct list_head* next = name_nid_dict->next;
	
	struct mc_msg_queue* item = list_entry(next, struct mc_msg_queue, list);
		
	strcpy(msg_data, item->msg_data);
	*msg_size = item->msg_size;

	list_del(&item->list);

	kfree(item->msg_data);	
	kfree(item);
	name_nid_dict = next;

	return 1;
}

/*
 * print name_nid_dict
 */


void print(struct list_head* name_nid_dict){
	struct mc_msg_queue *pos = NULL;
	list_for_each_entry(pos, name_nid_dict, list){
		printk(pos->msg_data);
		printk(" ");
		printk("message data: %d\n", pos->msg_size);
	}
}


void free_all(struct list_head* name_nid_dict){
//	unsigned long flags;
//	struct name_mq_map* lock_item = list_entry(name_nid_dict, struct name_mq_map, list);
	
//	spin_lock_irqsave(&lock_item->mq_lock,flags);

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
	
//	spin_unlock_irqrestore(&lock_item->mq_lock,flags);
}

LIST_HEAD(addr_map);

unsigned int mc_mq_open(char* mq_name, unsigned int max_size)
{

/*
 * what if we already got a message queue with that name in the name map?	
 */
	unsigned long flags;
	spin_lock_irqsave(&map_lock, flags);

	struct name_mq_map *pos, *target = NULL;
	list_for_each_entry(pos, &addr_map, list){
		
		if(strcmp(mq_name, pos->mq_name)==0){
			target = pos;		
		}
	}
	if(target != NULL){
		spin_unlock_irqrestore(&map_lock, flags);
		return 0;
	}
	/* return 0 means message queue already exist	
	*/
	spin_unlock_irqrestore(&map_lock, flags);

	printk("begin open %s direc\n", mq_name);

	struct name_mq_map* tmp;
	
	tmp = kmalloc(sizeof(struct name_mq_map), GFP_KERNEL);
	if(!tmp){
//		spin_unlock_irqrestore(&map_lock, flags);

		printk("allocated wrong\n");
		return 0;
	}

	strcpy(tmp->mq_name, mq_name);
	tmp->max_size = max_size;
	tmp->mq = kmalloc(sizeof(struct list_head), GFP_KERNEL);
	
	INIT_LIST_HEAD(tmp->mq);	
/* init the lock */
	spin_lock_init(&tmp->mq_lock);

	spin_lock_irqsave(&map_lock, flags);

	list_add_tail(&tmp->list, &addr_map);	

	spin_unlock_irqrestore(&map_lock, flags);

	return 1;
}

unsigned int mc_mq_send(char *mq_name, char* msg_data, unsigned int msg_size){
	
/* find out where is our mq head pointer */
	struct name_mq_map *pos, *target = NULL;
	list_for_each_entry(pos, &addr_map, list){
		if(strcmp(mq_name, pos->mq_name)==0){
			target = pos;		
		}
	}
	if(target != NULL){
		/* spin lock acquire here */
		unsigned long flags;
		
		spin_lock_irqsave(&target->mq_lock,flags);
		append(msg_data, msg_size, target->mq);
		spin_unlock_irqrestore(&target->mq_lock,flags);
		return 1;	
	}
	return 0;
}

unsigned int mc_mq_receive(char *mq_name, char* msg_data, unsigned int* msg_size){


	/* find out where is our mq head pointer */
	struct name_mq_map *pos, *target = NULL;
	list_for_each_entry(pos, &addr_map, list){
		if(strcmp(mq_name, pos->mq_name)==0){
			target = pos;		
		}
	}
	if(target ==NULL){
		return 0;
	}

	/* spin lock acquire here */
	unsigned long flags;
	
	spin_lock_irqsave(&target->mq_lock,flags);
	pop(msg_data, msg_size, target->mq);
	spin_unlock_irqrestore(&target->mq_lock,flags);
	return 1;
}

unsigned int mc_mq_close(char* mq_name){
	unsigned long flags;
	spin_lock_irqsave(&map_lock, flags);

	struct name_mq_map *pos, *target = NULL;
	list_for_each_entry(pos, &addr_map, list){
		
		if(strcmp(mq_name, pos->mq_name)==0){
			target = pos;		
		}
	}

	if(target ==NULL){
		spin_unlock_irqrestore(&map_lock, flags);	
		return 0;	
	}
	printk("close: ");		
	printk(target->mq_name);
	printk(" ");
	printk("max size data: %d\n", target->max_size);
	list_del(&target->list);

	spin_unlock_irqrestore(&map_lock, flags);
/* possible leak
 * what if message queue still gots some thing, then we free then
 */
	free_all(target->mq);
	kfree(target->mq);
	kfree(target);

	return 1;
}

unsigned int mc_mq_free(void){
	struct list_head* cur=addr_map.next;
	while(cur != &addr_map){		
		struct name_mq_map* item = list_entry(cur, struct name_mq_map, list);
		printk("%s: %d\n", item->mq_name, item->max_size);
		struct list_head* tmp = cur->next;
		list_del(cur);

		cur = tmp;
		
		/* test
		 * print all the mq message 
		 */
		print(item->mq);

		free_all(item->mq);
		kfree(item->mq);
		kfree(item);
	}
	return 1;
}

struct open_argu {
	char name[10];
	int max_size;
};
 
static int test_w(void* data)
{
	struct open_argu* argu = (struct open_argu*) data;
	char* name = argu->name;
	int max_size = argu->max_size;

	mc_mq_send("steven", name, strlen(name));
	mc_mq_send("jishen", name, strlen(name));
	
	while(!kthread_should_stop()){
		flush_signals(current);
		set_current_state(TASK_INTERRUPTIBLE);
		if(!kthread_should_stop())schedule();
		set_current_state(TASK_RUNNING);
	}
	kfree(argu);
	return 0;	
}
static struct task_struct *thread_list[4];
int thread_init(long n){
	printk(KERN_INFO "kthread init\n");

	struct open_argu* argu[4];
	int j=0;	
	for(j=0;j<4;j++){
		argu[j] = kmalloc(sizeof(struct open_argu), GFP_KERNEL);
	}	
	strcpy((argu[0])->name,"haolan1");
	strcpy((argu[1])->name,"haolan2");
	strcpy((argu[2])->name,"haolan3");
	strcpy((argu[3])->name,"haolan4");
	
	long a = 0;	
	for(j=0;j<4;j++){
		argu[j]->max_size = j+10;
		thread_list[j] = kthread_run(test_w, (void*)(argu[j]), "haolan%d",j);
	}

	return 0;
}

void thread_destroy(void){
	int ret;
	int a=0;
	for(a=0;a<4;a++){
		
		ret = kthread_stop(thread_list[a]);
		if(!ret)
			printk(KERN_INFO "thread stopped %d\n", a);
		
	}
}

static int mq_test_module_init(void)
{
	printk("loaded yi\n");
/*	
	append("NVSL", 4, &yi_list);
	append("STABLE", 6, &yi_list);
	append("WUKLAB", 6, &yi_list);
	char* msg = kmalloc(sizeof(char)*(MAX_FILENAME_LENGTH+1), GFP_KERNEL);	
	int size;	
	pop(msg,&size, &yi_list);
	printk("%s: %d\n", msg, size);
	kfree(msg);

	print(&yi_list);
*/
	spin_lock_init(&map_lock);

	mc_mq_open("max",90);
	mc_mq_open("steven",80);
	mc_mq_close("max");
	mc_mq_open("jishen",90);
	

/*
	char* msg = kmalloc(sizeof(char)*(MAX_FILENAME_LENGTH+1), GFP_KERNEL);	
	int size;	
	if(mc_mq_receive("steven",msg,&size))
		printk("%s: %d\n", msg, size);
	if(mc_mq_receive("steven",msg,&size))
		printk("%s: %d\n", msg, size);
	kfree(msg);
*/
	
	thread_init(3);

	return 0;
}


static void mq_test_module_exit(void)
{
	printk("offload! \n");
	thread_destroy();
	printk("thread k destroyed\n");
	mc_mq_free();

	printk("free done\n");

}

module_init(mq_test_module_init);
module_exit(mq_test_module_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Wuklab@Purdue");
