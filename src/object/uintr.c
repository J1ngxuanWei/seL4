/*
 * Copyright 2025
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include <stdint.h>
#include <object/uintr.h>
#include <api/syscall.h>
#include <arch/machine.h>
#include <arch/model/smp.h>

exception_t handle_SysUintrRegisterHandler(void)
{
    uint64_t handler = getSyscallArg(0, NULL);
    uint32_t flags = getSyscallArg(1, NULL);

    if (flags & ~UINTR_HANDLER_FLAG_WAITING_ANY)
        return EXCEPTION_SYSCALL_ERROR;

    if (!handler)
        return EXCEPTION_SYSCALL_ERROR;

    tcb_t* cur = NODE_STATE(ksCurThread);
    if (is_uintr_receiver(cur))
        return EXCEPTION_SYSCALL_ERROR;
    
    if (!cur->upid_is_alloced) {
		alloc_upid(cur);
        cur->upid_is_alloced = 1;
	}

    // Here need to disable preemption
    // fpregs_lock();
    struct uintr_upid_ctx *upid_ctx = &cur->upid_ctx;
    upid_ctx->refs += 1;
    struct uintr_upid *upid = &upid_ctx->upid;
    upid->nc.nv = UINTR_NOTIFICATION_VECTOR;

#ifdef ENABLE_SMP_SUPPORT
#ifdef CONFIG_USE_LOGICAL_IDS
    upid->nc.ndst = (uint32_t)getCurrentLOGID();
#else
    upid->nc.ndst = (uint32_t)getCurrentCPUID();
#endif
#else
#ifdef CONFIG_USE_LOGICAL_IDS
    upid->nc.ndst = (uint32_t)apic_get_logical_id();
#else
    upid->nc.ndst = 0;
#endif
#endif

    x86_wrmsr(MSR_IA32_UINTR_HANDLER, handler);
    x86_wrmsr(MSR_IA32_UINTR_PD, (uint64_t)upid);
    x86_wrmsr(MSR_IA32_UINTR_STACKADJUST, 128);
    uint64_t misc_msr = x86_rdmsr(MSR_IA32_UINTR_MISC);
    misc_msr |= (uint64_t)UINTR_NOTIFICATION_VECTOR << 32;
    x86_wrmsr(MSR_IA32_UINTR_MISC, misc_msr);

	cur->upid_activated = true;

    // Here we enable preemption
	// fpregs_unlock();

    return EXCEPTION_NONE;
}

exception_t handle_SysUintrUnRegisterHandler(void)
{
    return EXCEPTION_SYSCALL_ERROR;
}

exception_t handle_SysUintrVectorFd(void)
{
    return EXCEPTION_SYSCALL_ERROR;
}

exception_t handle_SysUintrRegisterSender(void)
{
    return EXCEPTION_SYSCALL_ERROR;
}

exception_t handle_SysUintrUnRegisterSender(void)
{
    return EXCEPTION_SYSCALL_ERROR;
}

exception_t handle_SysUintrWait(void)
{
    return EXCEPTION_SYSCALL_ERROR;
}

exception_t handle_SysUintrRegisterSelf(void)
{
    return EXCEPTION_SYSCALL_ERROR;
}

exception_t handle_SysUintrAltStack(void)
{
    return EXCEPTION_SYSCALL_ERROR;
}

exception_t handle_SysUintrIpiFd(void)
{
    return EXCEPTION_SYSCALL_ERROR;
}
