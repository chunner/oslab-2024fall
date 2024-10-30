#include <os/mm.h>

static ptr_t kernMemCurr = FREEMEM_KERNEL;
static ptr_t userMemCurr = FREEMEM_USER;


ptr_t allocKernelPage(int numPage)
{
    // align PAGE_SIZE
    ptr_t ret = ROUND(kernMemCurr, PAGE_SIZE);
    kernMemCurr = ret + numPage * PAGE_SIZE;
    return ret;
}

ptr_t allocUserPage(int numPage)
{
    // align PAGE_SIZE
    ptr_t ret = ROUND(userMemCurr, PAGE_SIZE);
    userMemCurr = ret + numPage * PAGE_SIZE;
    return ret;
}
ptr_t allocKernelSP() {
    if (recycle_kernel_sp_num >= 1) {
        return recycle_kernel_sp[--recycle_kernel_sp_num];
    } else {
        return allocKernelPage(KernelStackPage) + KernelStackPage * PAGE_SIZE;
    }
}
ptr_t allockUserSP() {
    if (recycle_user_sp_num >= 1) {
        return recycle_user_sp[--recycle_user_sp_num];
    } else {
        return allocUserPage(UserStackPage) + UserStackPage * PAGE_SIZE;
    }
}