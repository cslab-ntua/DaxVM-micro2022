#include <linux/time.h>
#include <linux/fs.h>
#include <linux/seq_file.h>
#include <linux/slab.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>

#include "daxvm.h"

int daxvm_msync_support = 1;
static const char proc_dirname[] = "fs/daxvm";
static struct proc_dir_entry *daxvm_proc_root;

ssize_t daxvm_msync_write(struct file *filp, const char __user *buf, size_t len, loff_t *ppos)
{
  char *_buf;
	int retval = len;

	_buf = kmalloc(len, GFP_KERNEL);
	if (_buf == NULL)  {
		retval = -ENOMEM;
		goto out;
	}
	if (copy_from_user(_buf, buf, len)) {
		retval = -EFAULT;
		goto out;
	}
	_buf[len] = 0;
	sscanf(_buf, "%i", &daxvm_msync_support);

out:
  return retval;
}

static int daxvm_msync_show(struct seq_file *seq, void *v)
{
	seq_printf(seq, "%i\n", daxvm_msync_support);
	return 0;
}

static int daxvm_msync_open(struct inode *inode, struct file *file)
{
	return single_open(file, daxvm_msync_show, PDE_DATA(inode));
}

static const struct file_operations daxvm_msync_fops = {
	.owner		= THIS_MODULE,
	.open		= daxvm_msync_open,
	.read		= seq_read,
	.write		= daxvm_msync_write,
	.llseek		= seq_lseek,
	.release	= single_release,
};

static int __init proc_daxvm_init(void)
{
	daxvm_proc_root = proc_mkdir(proc_dirname, NULL);
	proc_create_data("daxvm_msync", 0664, daxvm_proc_root, &daxvm_msync_fops, NULL);
	return 0;
}
fs_initcall(proc_daxvm_init);
