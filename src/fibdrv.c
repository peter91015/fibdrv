#include <linux/cdev.h>

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/kdev_t.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>

#include "xs.h"

MODULE_LICENSE("Dual MIT/GPL");
MODULE_AUTHOR("National Cheng Kung University, Taiwan");
MODULE_DESCRIPTION("Fibonacci engine driver");
MODULE_VERSION("0.1");

#define DEV_FIBONACCI_NAME "fibonacci"
#define MAX_SIZE 500
/* MAX_LENGTH is set to 92 because
 * ssize_t can't fit the number > 92
 */
#define MAX_LENGTH 92

static ktime_t kt;
static dev_t fib_dev = 0;
static struct cdev *fib_cdev;
static struct class *fib_class;
static DEFINE_MUTEX(fib_mutex);
#define XOR_SWAP(a, b, type) \
    do {                     \
        type *__c = (a);     \
        type *__d = (b);     \
        *__c ^= *__d;        \
        *__d ^= *__c;        \
        *__c ^= *__d;        \
    } while (0)

static void __swap(void *a, void *b, size_t size)
{
    if (a == b)
        return;

    switch (size) {
    case 1:
        XOR_SWAP(a, b, char);
        break;
    case 2:
        XOR_SWAP(a, b, short);
        break;
    case 4:
        XOR_SWAP(a, b, unsigned int);
        break;
    case 8:
        XOR_SWAP(a, b, unsigned long);
        break;
    default:
        /* Do nothing */
        break;
    }
}
static void reverse_str(char *str, size_t n)
{
    for (int i = 0; i < (n >> 1); i++)
        __swap(&str[i], &str[n - i - 1], sizeof(char));
}
static void string_number_add(xs *a, xs *b, xs *out)
{
    char *data_a, *data_b;
    size_t size_a, size_b;
    int i, carry = 0;
    int sum;

    /*
     * Make sure the string length of 'a' is always greater than
     * the one of 'b'.
     */
    if (xs_size(a) < xs_size(b))
        __swap((void *) &a, (void *) &b, sizeof(void *));

    data_a = xs_data(a);
    data_b = xs_data(b);

    size_a = xs_size(a);
    size_b = xs_size(b);

    reverse_str(data_a, size_a);
    reverse_str(data_b, size_b);

    char buf[MAX_SIZE];

    for (i = 0; i < size_b; i++) {
        sum = (data_a[i] - '0') + (data_b[i] - '0') + carry;
        buf[i] = '0' + sum % 10;
        carry = sum / 10;
    }

    for (i = size_b; i < size_a; i++) {
        sum = (data_a[i] - '0') + carry;
        buf[i] = '0' + sum % 10;
        carry = sum / 10;
    }

    if (carry)
        buf[i++] = '0' + carry;

    buf[i] = 0;

    reverse_str(buf, i);

    /* Restore the original string */
    reverse_str(data_a, size_a);
    reverse_str(data_b, size_b);

    if (out)
        *out = *xs_tmp(buf);
}
static void string_number_sub(xs *a, xs *b, xs *out)
{
    char *data_a, *data_b;
    size_t size_a, size_b;
    int i, carry = 0;
    int sum;

    /*
     * Make sure the string length of 'a' is always greater than
     * the one of 'b'.
     */
    if (xs_size(a) < xs_size(b))
        __swap((void *) &a, (void *) &b, sizeof(void *));

    data_a = xs_data(a);
    data_b = xs_data(b);

    size_a = xs_size(a);
    size_b = xs_size(b);

    reverse_str(data_a, size_a);
    reverse_str(data_b, size_b);

    char buf[MAX_SIZE];

    for (i = 0; i < size_b; i++) {
        // sum = (data_a[i] - '0') + (data_b[i] - '0') + carry;
        sum = (data_a[i] - '0') - (data_b[i] - '0') + carry;
        if (sum < 0) {
            buf[i] = (sum + 10) + '0';
            carry = -1;
        } else {
            buf[i] = sum + '0';
            carry = 0;
        }
    }

    for (i = size_b; i < size_a; i++) {
        sum = (data_a[i] - '0') + carry;
        if (sum < 0) {
            buf[i] = (sum + 10) + '0';
            carry = -1;
        } else {
            buf[i] = sum + '0';
            carry = 0;
        }
        // buf[i] = '0' + sum % 10;
        // carry = sum / 10;
    }
    if (buf[size_a - 1] == '0') {
        buf[size_a - 1] = 0;
        reverse_str(buf, size_a - 1);
    } else {
        buf[size_a] = 0;
        reverse_str(buf, size_a);
    }
    /*if (carry)
        buf[i++] = '0' + carry;*/



    /* Restore the original string */
    reverse_str(data_a, size_a);
    reverse_str(data_b, size_b);

    if (out)
        *out = *xs_tmp(buf);
}
static void string_scalar_multi(xs *a, char b, xs *out)
{
    /*b is a integer in 0~9*/
    if (b == '0') {
        *out = *xs_tmp("0");
        return;
    }
    if (b == '1') {
        *out = *xs_tmp(xs_data(a));
        return;
    } else {
        int b_int = b - '0', carry = 0, i;
        char *data_a = xs_data(a);
        size_t size_a = xs_size(a);
        // printk("a = %s\n", data_a);
        // reverse_str(data_a, size_a);
        char buf[MAX_SIZE];
        for (i = 0; i < size_a; i++) {
            int sum;
            sum = (data_a[i] - '0') * b_int + carry;
            buf[i] = '0' + sum % 10;
            carry = sum / 10;
        }
        if (carry)
            buf[i++] = '0' + carry;
        buf[i] = '\0';
        // reverse_str(buf, i);
        // printk("buf = %s\n", buf);
        // reverse_str(data_a, size_a);
        if (out)
            *out = *xs_tmp(buf);
    }
}
static void string_pow10(char *input, size_t pow, xs *out)
{
    char buf[MAX_SIZE];
    int size_input = strlen(input);
    int i, tmp = pow;
    for (i = 0; i < size_input; i++)
        buf[i] = input[i];
    while (tmp-- > 0)
        buf[i++] = '0';
    buf[i] = '\0';
    if (out)
        *out = *xs_tmp(buf);
    /*char *zeros = malloc(pow+1);
    for(int i=0;i<pow;i++)
        zeros[i] = '0';
    zeros[pow] = '\0';
    xs *tmp = xs_tmp(zeros), *xs_input = xs_tmp(input);
    out = xs_tmp("");
    xs_concat(out, xs_input, tmp);
    free(zeros);
    xs_free(xs_input);*/
}
static void string_number_multi(xs *a, xs *b, xs *out)
{
    char *data_a, *data_b;
    size_t size_a, size_b;
    int i;
    // int sum;

    /*
     * Make sure the string length of 'a' is always greater than
     * the one of 'b'.
     */
    if (xs_size(a) < xs_size(b))
        __swap((void *) &a, (void *) &b, sizeof(void *));

    data_a = xs_data(a);
    data_b = xs_data(b);
    printk("a = %s\n", data_a);
    printk("b = %s\n", data_b);
    size_a = xs_size(a);
    size_b = xs_size(b);

    reverse_str(data_a, size_a);
    reverse_str(data_b, size_b);
    if (out)
        *out = *xs_tmp("0");
    for (i = 0; i < size_b; i++) {
        // printk("i = %d\n",i);
        xs tmp;
        tmp = *xs_tmp("0");
        // printk("(init))mp = %s\n", xs_data(&tmp));
        // printk("b = %s", &data_b[i]);
        string_scalar_multi(a, data_b[i], &tmp);
        // printk("(scalar)tmp = %s\n", xs_data(&tmp));
        reverse_str(xs_data(&tmp), xs_size(&tmp));
        string_pow10(xs_data(&tmp), i, &tmp);

        string_number_add(out, &tmp, out);
        // printk("tmp = %s\n", xs_data(&tmp));
    }
    reverse_str(data_a, size_a);
    reverse_str(data_b, size_b);
    // reverse_str(xs_data(out), xs_size(out));
}
static void fast_doubling_rec(xs *out, long long k)
{
    if (k == 0) {
        *out = *xs_tmp("0");
        return;
    }
    if (k == 1 || k == 2) {
        *out = *xs_tmp("1");
        return;
    }
    if (k & 1) {
        long long tmp = (k - 1) >> 1;
        xs a, b, t1, t2;
        a = *xs_tmp("0");
        b = *xs_tmp("0");
        t1 = *xs_tmp("0");
        t2 = *xs_tmp("0");
        fast_doubling_rec(&a, tmp);
        fast_doubling_rec(&b, tmp + 1);
        xs tmp_a = *xs_tmp(xs_data(&a));
        string_number_multi(&a, &tmp_a, &t1);
        xs tmp_b = *xs_tmp(xs_data(&b));
        string_number_multi(&b, &tmp_b, &t2);
        string_number_add(&t1, &t2, out);

    } else {
        long long tmp = k >> 1;
        xs a, b, t1, t2;
        a = *xs_tmp("0");
        b = *xs_tmp("0");
        t1 = *xs_tmp("0");
        t2 = *xs_tmp("0");
        fast_doubling_rec(&a, tmp);
        fast_doubling_rec(&b, tmp + 1);
        reverse_str(xs_data(&b), xs_size(&b));
        string_scalar_multi(&b, '2', &t2);
        reverse_str(xs_data(&t2), xs_size(&t2));
        string_number_sub(&t2, &a, &t1);
        string_number_multi(&a, &t1, out);
    }
}

static long long fib_dp(long long k, char __user *buf)
{
    xs f[3];
    int i, n;
    f[0] = *xs_tmp("0");
    f[1] = *xs_tmp("1");
    f[2] = *xs_tmp("1");
    int tmp;
    for (i = 2; i <= k; i++) {
        tmp = i % 3;
        string_number_add(&f[(tmp + 1) % 3], &f[(tmp + 2) % 3], &f[tmp]);
    }
    tmp = k % 3;
    n = xs_size(&f[tmp]);
    if (copy_to_user(buf, xs_data(&f[tmp]), n))
        return -EFAULT;

    for (i = 0; i <= 2; i++)
        xs_free(&f[i]);

    return n;
}
static long long fib_fast_doubling(long long k, char __user *buf)
{
    xs output;
    output = *xs_tmp("0");
    long long n;
    fast_doubling_rec(&output, k);
    n = xs_size(&output);
    printk("test test \n");
    if (copy_to_user(buf, xs_data(&output), n))
        return -EFAULT;
    // xs_free(&output);
    return n;
}
static long long fib_sequence(long long k, char __user *buf, int size)
{
    /* FIXME: C99 variable-length array (VLA) is not allowed in Linux kernel. */
    switch (size) {
    case 1:
        return fib_fast_doubling(k, buf);
    default:
        return fib_dp(k, buf);
    }
}

static int fib_open(struct inode *inode, struct file *file)
{
    if (!mutex_trylock(&fib_mutex)) {
        printk(KERN_ALERT "fibdrv is in use");
        return -EBUSY;
    }
    return 0;
}

static int fib_release(struct inode *inode, struct file *file)
{
    mutex_unlock(&fib_mutex);
    return 0;
}

/* calculate the fibonacci number at given offset */
static ssize_t fib_read(struct file *file,
                        char *buf,
                        size_t size,
                        loff_t *offset)
{
    ktime_t start = ktime_get();
    ssize_t fib_res = (ssize_t) fib_sequence(*offset, buf, size);
    kt = ktime_sub(ktime_get(), start);
    return fib_res;
}

/* write operation is skipped */
static ssize_t fib_write(struct file *file,
                         const char *buf,
                         size_t size,
                         loff_t *offset)
{
    return kt;
}

static loff_t fib_device_lseek(struct file *file, loff_t offset, int orig)
{
    loff_t new_pos = 0;
    switch (orig) {
    case 0: /* SEEK_SET: */
        new_pos = offset;
        break;
    case 1: /* SEEK_CUR: */
        new_pos = file->f_pos + offset;
        break;
    case 2: /* SEEK_END: */
        new_pos = MAX_LENGTH - offset;
        break;
    }

    /*if (new_pos > MAX_LENGTH)
        new_pos = MAX_LENGTH; */ // max case
    if (new_pos < 0)
        new_pos = 0;        // min case
    file->f_pos = new_pos;  // This is what we'll use now
    return new_pos;
}

const struct file_operations fib_fops = {
    .owner = THIS_MODULE,
    .read = fib_read,
    .write = fib_write,
    .open = fib_open,
    .release = fib_release,
    .llseek = fib_device_lseek,
};

static int __init init_fib_dev(void)
{
    int rc = 0;

    mutex_init(&fib_mutex);

    // Let's register the device
    // This will dynamically allocate the major number
    rc = alloc_chrdev_region(&fib_dev, 0, 1, DEV_FIBONACCI_NAME);

    if (rc < 0) {
        printk(KERN_ALERT
               "Failed to register the fibonacci char device. rc = %i",
               rc);
        return rc;
    }

    fib_cdev = cdev_alloc();
    if (fib_cdev == NULL) {
        printk(KERN_ALERT "Failed to alloc cdev");
        rc = -1;
        goto failed_cdev;
    }
    fib_cdev->ops = &fib_fops;
    rc = cdev_add(fib_cdev, fib_dev, 1);

    if (rc < 0) {
        printk(KERN_ALERT "Failed to add cdev");
        rc = -2;
        goto failed_cdev;
    }

    fib_class = class_create(THIS_MODULE, DEV_FIBONACCI_NAME);

    if (!fib_class) {
        printk(KERN_ALERT "Failed to create device class");
        rc = -3;
        goto failed_class_create;
    }

    if (!device_create(fib_class, NULL, fib_dev, NULL, DEV_FIBONACCI_NAME)) {
        printk(KERN_ALERT "Failed to create device");
        rc = -4;
        goto failed_device_create;
    }
    return rc;
failed_device_create:
    class_destroy(fib_class);
failed_class_create:
    cdev_del(fib_cdev);
failed_cdev:
    unregister_chrdev_region(fib_dev, 1);
    return rc;
}

static void __exit exit_fib_dev(void)
{
    mutex_destroy(&fib_mutex);
    device_destroy(fib_class, fib_dev);
    class_destroy(fib_class);
    cdev_del(fib_cdev);
    unregister_chrdev_region(fib_dev, 1);
}

module_init(init_fib_dev);
module_exit(exit_fib_dev);
