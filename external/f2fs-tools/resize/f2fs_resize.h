#ifndef _RESIZE_H_
#define _RESIZE_H_

#include <linux/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <mntent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <linux/hdreg.h>
#include <assert.h>

#include "../include/f2fs_fs.h"

#define ver_after(a, b) (typecheck(unsigned long long, a) &&            \
        typecheck(unsigned long long, b) &&                     \
        ((long long)((a) - (b)) > 0))

#define ENTRIES_IN_SUM	512
#define SUMMARY_SIZE    (7)
#define SUM_ENTRY_SIZE  (ENTRIES_IN_SUM * SUMMARY_SIZE)
#define SUM_FOOTER_SIZE (5)

struct list_head {
    struct list_head *next, *prev;
};

struct curseg_info {
        struct f2fs_summary_block *sum_blk;     /* cached summary block */
        unsigned char alloc_type;               /* current allocation type */
        unsigned int segno;                     /* current segment number */
        unsigned short next_blkoff;             /* next block offset to write */
        unsigned int zone;                      /* current zone number */
        unsigned int next_segno;                /* preallocated segment */
};

struct f2fs_sm_info {
        struct sit_info *sit_info;
        struct curseg_info *curseg_array;

        block_t seg0_blkaddr;
        block_t main_blkaddr;
        block_t ssa_blkaddr;

        unsigned int segment_count;
        unsigned int main_segments;
        unsigned int reserved_segments;
        unsigned int ovp_segments;
};

struct f2fs_sb_info {
    struct f2fs_fsck *fsck;

    struct f2fs_super_block *raw_super;
    struct f2fs_nm_info *nm_info;
    struct f2fs_sm_info *sm_info;
    struct f2fs_checkpoint *ckpt;
    int cur_cp;

    struct list_head orphan_inode_list;
    unsigned int n_orphans;

    /* basic file system units */
    unsigned int log_sectors_per_block;     /* log2 sectors per block */
    unsigned int log_blocksize;             /* log2 block size */
    unsigned int blocksize;                 /* block size */
    unsigned int root_ino_num;              /* root inode number*/
    unsigned int node_ino_num;              /* node inode number*/
    unsigned int meta_ino_num;              /* meta inode number*/
    unsigned int log_blocks_per_seg;        /* log2 blocks per segment */
    unsigned int blocks_per_seg;            /* blocks per segment */
    unsigned int segs_per_sec;              /* segments per section */
    unsigned int secs_per_zone;             /* sections per zone */

    unsigned int total_sections;            /* total section count */
    unsigned int total_node_count;          /* total node block count */
    unsigned int total_valid_node_count;    /* valid node block count */
    unsigned int total_valid_inode_count;   /* valid inode count */
    int active_logs;                        /* # of active logs */

    block_t user_block_count;               /* # of user blocks */
    block_t total_valid_block_count;        /* # of valid blocks */
    block_t alloc_valid_block_count;        /* # of allocated blocks */
    block_t last_valid_block_count;         /* for recovery */
    u32 s_next_generation;                  /* for NFS support */

    unsigned int cur_victim_sec;            /* current victim section num */
};

static inline struct f2fs_super_block *F2FS_RAW_SUPER(struct f2fs_sb_info *sbi)
{
    return (struct f2fs_super_block *)(sbi->raw_super);
}

static inline struct f2fs_checkpoint *F2FS_CKPT(struct f2fs_sb_info *sbi)
{
    return (struct f2fs_checkpoint *)(sbi->ckpt);
}

static inline bool is_set_ckpt_flags(struct f2fs_checkpoint *cp, unsigned int f)
{
	unsigned int ckpt_flags = le32_to_cpu(cp->ckpt_flags);
	return ckpt_flags & f;
}

static inline struct f2fs_sm_info *SM_I(struct f2fs_sb_info *sbi)
{
        return (struct f2fs_sm_info *)(sbi->sm_info);
}

static inline struct curseg_info *CURSEG_I(struct f2fs_sb_info *sbi, int type)
{
        return (struct curseg_info *)(SM_I(sbi)->curseg_array + type);
}

extern int validate_super_block(struct f2fs_sb_info *, int);
extern int get_valid_checkpoint(struct f2fs_sb_info *sbi);
extern int f2fs_adjust_super_block(struct f2fs_sb_info *sbi);
extern int f2fs_adjust_checkpoint(struct f2fs_sb_info *sbi);
#endif
