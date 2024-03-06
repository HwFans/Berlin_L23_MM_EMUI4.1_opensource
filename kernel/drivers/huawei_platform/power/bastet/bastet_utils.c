/* bastet_utils.c
 *
 * Provide Bastet utilities.
 *
 * Copyright (C) 2014 Huawei Device Co.,Ltd.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/sched.h>
#include <linux/fdtable.h>
#include <linux/file.h>
#include <linux/inetdevice.h>
#include <linux/of.h>
#include <linux/wakelock.h>
#include <linux/fb.h>
#include <linux/mm.h>
#include <linux/timer.h>
#include <uapi/linux/if.h>
#include <linux/atomic.h>
#include <net/tcp.h>
#include <huawei_platform/power/bastet/bastet_utils.h>

#define BASTET_WAKE_LOCK				"bastet_wl"
#define BASTET_DEFAULT_NET_DEV			"rmnet0"

/* minimum uid number */
#define MIN_UID				0
/* maximum uid number */
#define MAX_UID				65535

#define SCREEN_ON			1
#define SCREEN_OFF			0

#define CHANNEL_OCCUPIED_TIMEOUT			(5 * HZ)

static void channel_occupied_timeout(unsigned long data);

static struct wake_lock wl_bastet;
static bool bastet_cfg_en;
static DEFINE_TIMER(channel_timer, channel_occupied_timeout, 0, 0);

bool bastet_dev_en;
char cur_netdev_name[IFNAMSIZ] = BASTET_DEFAULT_NET_DEV;
atomic_t proxy = ATOMIC_INIT(0);
atomic_t buffer = ATOMIC_INIT(0);
atomic_t channel = ATOMIC_INIT(0);
atomic_t cp_reset = ATOMIC_INIT(0);
/* set 1 for adjusting to non-wifi-proxy situation */
atomic_t channel_en = ATOMIC_INIT(1);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 10)
uid_t hrt_uid = -1;
#else
int hrt_uid = -1;
#endif

inline bool is_bastet_enabled(void)
{
	return bastet_cfg_en && bastet_dev_en;
}

inline bool is_proxy_available(void)
{
	return atomic_read(&proxy) != 0;
}

inline bool is_buffer_available(void)
{
	return atomic_read(&buffer) != 0;
}

inline bool is_channel_available(void)
{
	return atomic_read(&channel) == 0;
}

inline bool is_cp_reset(void)
{
	return atomic_read(&cp_reset) != 0;
}

/* check priority channel enable or disable */
inline bool is_channel_enable(void)
{
	return atomic_read(&channel_en) != 0;
}

inline bool is_sock_foreground(struct sock *sk)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4, 1, 10)
	return hrt_uid == sock_i_uid(sk).val;
#else
	return hrt_uid == sock_i_uid(sk);
#endif

}

/**
 * Function: is_wifi_proxy
 * Description: check whether socket is wifi network type
 * Input: struct sock *sk, struct sock pointer
 * Output:
 * Return: true, wifi network type
 *         false, not wifi network type
 * Date: 2015.01.19
 * Author: Pengyu ID: 00188486
 */
inline bool is_wifi_proxy(struct sock *sk)
{
	return sk && sk->bastet && sk->bastet->is_wifi;
}

void set_channel_occupied(void)
{
	atomic_inc(&channel);
	mod_timer(&channel_timer, jiffies + CHANNEL_OCCUPIED_TIMEOUT);
}

void clear_channel_occupied(void)
{
	atomic_dec(&channel);
	if (atomic_read(&channel) == 0 && timer_pending(&channel_timer))
		del_timer(&channel_timer);
}

static void channel_occupied_timeout(unsigned long data)
{
	BASTET_LOGI("set channel available");
	atomic_set(&channel, 0);
}

static void bastet_modem_reset_notify(void)
{
	if (!is_bastet_enabled())
		return;

	post_indicate_packet(BST_IND_MODEM_RESET, NULL, 0);
	atomic_set(&cp_reset, 1);
}

#if defined CONFIG_MSM_SUBSYSTEM_RESTART
#include <soc/qcom/subsystem_notif.h>

static void *subsys_h;

static int bastet_mss_reset_notifier_cb(struct notifier_block *this,
		unsigned long code, void *data)
{
	if (SUBSYS_AFTER_SHUTDOWN == code) {
		BASTET_LOGI("SUBSYS_AFTER_SHUTDOWN");
		bastet_modem_reset_notify();
	}

	return 0;
}

static struct notifier_block mss_reset_notifier = {
	.notifier_call = bastet_mss_reset_notifier_cb,
};

static void reg_mss_reset_notify(void)
{
	BASTET_LOGI("register msm mss reset notification");
	subsys_h = subsys_notif_register_notifier("modem", &mss_reset_notifier);
	if (IS_ERR(subsys_h))
		BASTET_LOGE("failed to register for ssr rc: %d\n",
			(int)PTR_ERR(subsys_h));
}

static void unreg_mss_reset_notify(void)
{
	if (subsys_h != NULL)
		subsys_notif_unregister_notifier(subsys_h,
			&mss_reset_notifier);
}

#elif defined CONFIG_BALONG_MODEM_RESET
#include <linux/hisi/reset.h>

#if defined(CONFIG_HISI_BALONG_MODEM_HI3XXX) || defined(CONFIG_HISI_BALONG_MODEM_HI6XXX)
extern int ccorereset_regcbfunc(const char *pname,
	pdrv_reset_cbfun pcbfun, int userdata, int priolevel);
#elif defined CONFIG_HISI_BALONG_MODEM_HI3