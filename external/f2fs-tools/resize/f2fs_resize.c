/**
 * main.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "f2fs_resize.h"

struct f2fs_super_block *sb;
static u_int32_t old_segment_count;

void print_raw_sb_info(struct f2fs_sb_info *sbi, struct f2fs_super_block * block)
{
	struct f2fs_super_block *sb = F2FS_RAW_SUPER(sbi);

	if(block)
		sb = block;
	if (!config.dbg_lv)
		return;

	printf("\n");
	printf("+--------------------------------------------------------+\n");
	printf("| Super block                                            |\n");
	printf("+--------------------------------------------------------+\n");

	DISP_u32(sb, magic);
	DISP_u32(sb, major_ver);
	DISP_u32(sb, minor_ver);
	DISP_u32(sb, log_sectorsize);
	DISP_u32(sb, log_sectors_per_block);

	DISP_u32(sb, log_blocksize);
	DISP_u32(sb, log_blocks_per_seg);
	DISP_u32(sb, segs_per_sec);
	DISP_u32(sb, secs_per_zone);
	DISP_u32(sb, checksum_offset);
	DISP_u64(sb, block_count);

	DISP_u32(sb, section_count);
	DISP_u32(sb, segment_count);
	DISP_u32(sb, segment_count_ckpt);
	DISP_u32(sb, segment_count_sit);
	DISP_u32(sb, segment_count_nat);

	DISP_u32(sb, segment_count_ssa);
	DISP_u32(sb, segment_count_main);
	DISP_u32(sb, segment0_blkaddr);
	DISP_u32(sb, cp_blkaddr);
	DISP_u32(sb, sit_blkaddr);
	DISP_u32(sb, nat_blkaddr);
	DISP_u32(sb, ssa_blkaddr);
	DISP_u32(sb, main_blkaddr);

	DISP_u32(sb, root_ino);
	DISP_u32(sb, node_ino);
	DISP_u32(sb, meta_ino);
	DISP_u32(sb, cp_payload);
	DISP("%s", sb, version);
	printf("\n");
}

void print_ckpt_info(struct f2fs_sb_info *sbi)
{
	struct f2fs_checkpoint *cp = F2FS_CKPT(sbi);

	if (!config.dbg_lv)
		return;

	printf("\n");
	printf("+--------------------------------------------------------+\n");
	printf("| Checkpoint                                             |\n");
	printf("+--------------------------------------------------------+\n");

	DISP_u64(cp, checkpoint_ver);
	DISP_u64(cp, user_block_count);
	DISP_u64(cp, valid_block_count);
	DISP_u32(cp, rsvd_segment_count);
	DISP_u32(cp, overprov_segment_count);
	DISP_u32(cp, free_segment_count);

	DISP_u32(cp, alloc_type[CURSEG_HOT_NODE]);
	DISP_u32(cp, alloc_type[CURSEG_WARM_NODE]);
	DISP_u32(cp, alloc_type[CURSEG_COLD_NODE]);
	DISP_u32(cp, cur_node_segno[0]);
	DISP_u32(cp, cur_node_segno[1]);
	DISP_u32(cp, cur_node_segno[2]);

	DISP_u32(cp, cur_node_blkoff[0]);
	DISP_u32(cp, cur_node_blkoff[1]);
	DISP_u32(cp, cur_node_blkoff[2]);


	DISP_u32(cp, alloc_type[CURSEG_HOT_DATA]);
	DISP_u32(cp, alloc_type[CURSEG_WARM_DATA]);
	DISP_u32(cp, alloc_type[CURSEG_COLD_DATA]);
	DISP_u32(cp, cur_data_segno[0]);
	DISP_u32(cp, cur_data_segno[1]);
	DISP_u32(cp, cur_data_segno[2]);

	DISP_u32(cp, cur_data_blkoff[0]);
	DISP_u32(cp, cur_data_blkoff[1]);
	DISP_u32(cp, cur_data_blkoff[2]);

	DISP_u32(cp, ckpt_flags);
	DISP_u32(cp, cp_pack_total_block_count);
	DISP_u32(cp, cp_pack_start_sum);
	DISP_u32(cp, valid_node_count);
	DISP_u32(cp, valid_inode_count);
	DISP_u32(cp, next_free_nid);
	DISP_u32(cp, sit_ver_bitmap_bytesize);
	DISP_u32(cp, nat_ver_bitmap_bytesize);
	DISP_u32(cp, checksum_offset);
	DISP_u64(cp, elapsed_time);

	DISP_u32(cp, sit_nat_version_bitmap[0]);
	printf("\n\n");
}

int sanity_check_raw_super(struct f2fs_super_block *raw_super)
{
	unsigned int blocksize;
	u32 crc = 0;

	if (F2FS_SUPER_MAGIC != le32_to_cpu(raw_super->magic)) {
		return -1;
	}

	crc = le32_to_cpu(raw_super->crc);
	if (!crc) {
		MSG(0, "crc is zero, this partition may no support crc");
	} else {
		if (f2fs_crc_valid(crc, raw_super, offsetof(struct f2fs_super_block, crc))) {
			MSG(0, "Invalid checksum: crc(0x%x)", crc);
			return -1;
		}
	}

	if (F2FS_BLKSIZE != PAGE_CACHE_SIZE) {
		return -1;
	}

	blocksize = 1 << le32_to_cpu(raw_super->log_blocksize);
	if (F2FS_BLKSIZE != blocksize) {
		return -1;
	}

	if (le32_to_cpu(raw_super->log_sectorsize) > F2FS_MAX_LOG_SECTOR_SIZE ||
		le32_to_cpu(raw_super->log_sectorsize) <  F2FS_MIN_LOG_SECTOR_SIZE) {
		return -1;
	}

	if (le32_to_cpu(raw_super->log_sectors_per_block) +
		le32_to_cpu(raw_super->log_sectorsize) !=
			F2FS_MAX_LOG_SECTOR_SIZE) {
		return -1;
	}

	return 0;
}

int validate_super_block(struct f2fs_sb_info *sbi, int block)
{
	u64 offset;

	sbi->raw_super = calloc(sizeof(struct f2fs_super_block), 1);
	if (sbi->raw_super == NULL) {
		ERR_MSG("\tError: Calloc Failed!\n");
		return -1;
	}

	if(block == 0)
		offset = F2FS_SUPER_OFFSET;
	else
		offset = F2FS_BLKSIZE + F2FS_SUPER_OFFSET;

	if (dev_read(sbi->raw_super, offset, sizeof(struct f2fs_super_block))) {
		free(sbi->raw_super);
		return -1;
	}
	MSG(-1, "Info: get validate_super_block is %d\n", block);

	print_raw_sb_info(sbi, NULL);
	if (!sanity_check_raw_super(sbi->raw_super))
		return 0;

	free(sbi->raw_super);
	MSG(-1, "\tCan't find a valid F2FS superblock at 0x%x\n", block);

	return -EINVAL;
}

void *validate_checkpoint(block_t cp_addr,
                unsigned long long *version)
{
	void *cp_page_1, *cp_page_2;
	struct f2fs_checkpoint *cp_block;
	unsigned long blk_size = F2FS_BLKSIZE;
	unsigned long long cur_version = 0, pre_version = 0;
	unsigned int crc = 0;
	size_t crc_offset;

	/* Read the 1st cp block in this CP pack */
	cp_page_1 = malloc(PAGE_SIZE);
	if (dev_read_block(cp_page_1, cp_addr) < 0)
		return NULL;

	cp_block = (struct f2fs_checkpoint *)cp_page_1;
	crc_offset = le32_to_cpu(cp_block->checksum_offset);
	if (crc_offset >= blk_size)
		goto invalid_cp1;

	crc = *(unsigned int *)((unsigned char *)cp_block + crc_offset);
	if (f2fs_crc_valid(crc, cp_block, crc_offset)) {
		goto invalid_cp1;
	}

	pre_version = le64_to_cpu(cp_block->checkpoint_ver);

	/* Read the 2nd cp block in this CP pack */
	cp_page_2 = malloc(PAGE_SIZE);
	cp_addr += le32_to_cpu(cp_block->cp_pack_total_block_count) - 1;

	if (dev_read_block(cp_page_2, cp_addr) < 0)
		goto invalid_cp2;

	cp_block = (struct f2fs_checkpoint *)cp_page_2;
	crc_offset = le32_to_cpu(cp_block->checksum_offset);
	if (crc_offset >= blk_size)
		goto invalid_cp2;

	crc = *(unsigned int *)((unsigned char *)cp_block + crc_offset);
	if (f2fs_crc_valid(crc, cp_block, crc_offset))
		goto invalid_cp2;

	cur_version = le64_to_cpu(cp_block->checkpoint_ver);

	MSG(-1, "Info: get validate checkpoint is %llu\n", cp_block->checkpoint_ver);
	if (cur_version == pre_version) {
		*version = cur_version;
		free(cp_page_2);
		return cp_page_1;
	}

invalid_cp2:
	free(cp_page_2);
invalid_cp1:
	free(cp_page_1);
	return NULL;
}

int get_valid_checkpoint(struct f2fs_sb_info *sbi)
{
	struct f2fs_super_block *raw_sb = sbi->raw_super;
	void *cp1, *cp2, *cur_page;
	unsigned long blk_size = F2FS_BLKSIZE;
	unsigned long long cp1_version = 0, cp2_version = 0, version;
	unsigned long long cp_start_blk_no;
	unsigned int cp_blks = 1 + le32_to_cpu(F2FS_RAW_SUPER(sbi)->cp_payload);
	int ret;

	sbi->ckpt = malloc(cp_blks * blk_size);
	if (!sbi->ckpt)
		return -ENOMEM;

	/*
	 * Finding out valid cp block involves read both
	 * sets( cp pack1 and cp pack 2)
	 */
	cp_start_blk_no = le32_to_cpu(raw_sb->cp_blkaddr);
	cp1 = validate_checkpoint(cp_start_blk_no, &cp1_version);

	/* The second checkpoint pack should start at the next segment */
	cp_start_blk_no += 1 << le32_to_cpu(raw_sb->log_blocks_per_seg);
	cp2 = validate_checkpoint(cp_start_blk_no, &cp2_version);

	if (cp1 && cp2) {
		if (ver_after(cp2_version, cp1_version)) {
		cur_page = cp2;
		sbi->cur_cp = 2;
		version = cp2_version;
        } else {
		cur_page = cp1;
		sbi->cur_cp = 1;
		version = cp1_version;
	}
	} else if (cp1) {
		cur_page = cp1;
		sbi->cur_cp = 1;
		version = cp1_version;
	} else if (cp2) {
		cur_page = cp2;
		sbi->cur_cp = 2;
		version = cp2_version;
	} else {
		free(cp1);
		free(cp2);
		goto fail_no_cp;
	}

	MSG(-1, "Info: CKPT version = %llx\n", version);

	memcpy(sbi->ckpt, cur_page, blk_size);

	if (cp_blks > 1) {
		unsigned int i;
		unsigned long long cp_blk_no;

		cp_blk_no = le32_to_cpu(raw_sb->cp_blkaddr);
		if (cur_page == cp2)
			cp_blk_no += 1 <<
				le32_to_cpu(raw_sb->log_blocks_per_seg);
		/* copy sit bitmap */
		for (i = 1; i < cp_blks; i++) {
			unsigned char *ckpt = (unsigned char *)sbi->ckpt;
			ret = dev_read_block(cur_page, cp_blk_no + i);
			ASSERT(ret >= 0);
			memcpy(ckpt + i * blk_size, cur_page, blk_size);
		}
	}

	free(cp1);
	free(cp2);
	return 0;

fail_no_cp:
	free(sbi->ckpt);
	return -EINVAL;
}

int f2fs_adjust_super_block(struct f2fs_sb_info *sbi)
{
	struct hd_geometry geom;

	u_int32_t segment_count;
	u_int32_t sector_size;
	u_int32_t segment_size_bytes;
	u_int32_t zone_size_bytes;
	u_int64_t zone_align_start_offset, extent_sector;
	u_int8_t *zero_buff;
	u_int32_t crc = 0;
	u_int32_t old_crc = 0;
	int index, fd = config.fd;

	sb = sbi->raw_super;
	/* get partition start sector */
	if (ioctl(fd, HDIO_GETGEO, &geom) < 0)
		config.start_sector = 0;
	else
		config.start_sector = geom.start;

	sector_size = 1 << get_sb(log_sectorsize);
	segment_size_bytes = (1 << get_sb(log_blocksize)) * (1 << get_sb(log_blocks_per_seg));
	zone_size_bytes = segment_size_bytes;

	zone_align_start_offset =
		(config.start_sector * sector_size +
		2 * F2FS_BLKSIZE + zone_size_bytes - 1) /
		zone_size_bytes * zone_size_bytes -
		config.start_sector * sector_size;

	//we can obtain extent_sector from SSA#
	extent_sector = sb->segment_count_ssa * 1024 * 1024 *2;
	MSG(-1, "Info: the extent partition size is %"PRIx64"\n", extent_sector);
	MSG(-1, "Info: the partition size is %"PRIx64"\n", config.total_sectors);
	if (config.total_sectors > extent_sector)
		config.total_sectors = extent_sector;
	//leave 16KB for encrypting fs
	config.total_sectors -= 32;

	MSG(-1, "Info: the actually partition size is %"PRIx64"\n", config.total_sectors);

	/* total # of user blocks */
	set_sb(block_count, config.total_sectors >> get_sb(log_sectors_per_block));

	/* total # of segments */
	segment_count = (config.total_sectors * sector_size -
						zone_align_start_offset) / segment_size_bytes;

	old_segment_count = get_sb(segment_count_main);

	/* set new # of segment, section, segment_main */
	set_sb(segment_count, segment_count);
	set_sb(segment_count_main, segment_count - get_sb(segment_count_ckpt) -
				get_sb(segment_count_sit) - get_sb(segment_count_nat) -
				get_sb(segment_count_ssa));
	set_sb(section_count, get_sb(segment_count_main) / get_sb(segs_per_sec));
	/* modify this for seg:sec != 1:1 */
	set_sb(segment_count_main, get_sb(section_count) * config.segs_per_sec);

	/* calculate new reserved and overpro for checkpoint */
	if (config.overprovision == 0)
		config.overprovision = get_best_overprovision(sb);

	config.reserved_segments =
		(2 * (100 / config.overprovision + 1) + 6)
				* config.segs_per_sec;

	/* recalculate checksum for superblock, should use sb instead of sbi->raw_super*/
	old_crc = get_sb(crc);
	crc = f2fs_cal_crc32(F2FS_SUPER_MAGIC, (unsigned char*)(sb), offsetof(struct f2fs_super_block, crc));
	set_sb(crc, crc);
	MSG(-1, "Info: update crc successfully (0x%x --> 0x%x)\n", old_crc, crc);
	/* write adjusted sb back */
	zero_buff = calloc(F2FS_BLKSIZE, 1);
	if (zero_buff == NULL) {
		ERR_MSG("\tError: Calloc Failed!\n");
		return -1;
	}
	memcpy(zero_buff + F2FS_SUPER_OFFSET, sb,
			sizeof(struct f2fs_super_block));

	for (index = 0; index < 2; index++) {
		if (dev_write(zero_buff, index * F2FS_BLKSIZE, F2FS_BLKSIZE)) {
			MSG(1, "\tError: While while writing supe_blk \
				on disk!!! index : %d\n", index);
		free(zero_buff);
		return -1;
		}
	}

	MSG(-1, "Info: finish f2fs_adjust_super_block\n");
	free(zero_buff);
	return 0;
}

static int set_sit_entry(unsigned int segno, int type)
{
	int index,blk_addr;
	struct f2fs_sit_block *sit_blk;
	struct f2fs_sit_entry *sit_entry;
	blk_addr = segno / SIT_ENTRY_PER_BLOCK;
	index = segno % SIT_ENTRY_PER_BLOCK;

	blk_addr += get_sb(sit_blkaddr);
	sit_blk = calloc(F2FS_BLKSIZE, 1);
	if (sit_blk == NULL) {
		ERR_MSG("\tError: Calloc Failed!\n");
		return -1;
	}

	if (dev_read_block(sit_blk, blk_addr) < 0) {
		MSG(1, "\tError: While reading sit block from disk!!!\n");
		free(sit_blk);
		return -1;
	}

	sit_entry = &sit_blk->entries[index];
	memset(sit_entry, 0, sizeof(struct f2fs_sit_entry));
	sit_entry -> vblocks = type << 10;
	if(dev_write_block(sit_blk, blk_addr) < 0) {
		MSG(1, "Info:\t Error to write sit block\n");
		free(sit_blk);
		return -1;
	}

	free(sit_blk);
	return 0;
}

int f2fs_adjust_current_segment(struct f2fs_sb_info *sbi)
{
	__le32 summary_start;
	__le32 total_count;
	int data_summary_count;
	int i,j,offset,sum;
	void * summary_page;
	struct curseg_info *seg_i;
	__u64 blk_addr;
	struct f2fs_sm_info *sm_info;
	struct curseg_info *array;
	struct f2fs_checkpoint *cp = sbi->ckpt;
	struct f2fs_summary_block * sum_blk2;
	int ret = 0;

	sum = 0;
	total_count = cp->cp_pack_total_block_count;

	if(!is_set_ckpt_flags(cp, CP_UMOUNT_FLAG))
	{
		MSG(-1, "\twe just resize umount!\n");
		return -1;
	}

	if(is_set_ckpt_flags(cp, CP_COMPACT_SUM_FLAG)) {
		if(total_count == 6)
			data_summary_count = 1;
		else
			/*now we don't meet with*/
			data_summary_count = total_count - 5;
	} else {
		data_summary_count = 3;
	}

	summary_page = (char *)calloc(PAGE_SIZE * (data_summary_count + 3), 1);
	if (summary_page == NULL) {
		ERR_MSG("\tError: Calloc Failed!\n");
		MSG(-1, "Info: fail f2fs_adjust_current_segment!!!\n");
		return -1;
	}
	summary_start = cp->cp_pack_start_sum + get_sb(cp_blkaddr);

	/*read cp from pack #2*/
	if(sbi->cur_cp % 2 == 0)
		summary_start += (1 << get_sb(log_blocks_per_seg));

	for(i = 0; i < (data_summary_count + 3); i++) {
		if (dev_read_block(summary_page + i * F2FS_BLKSIZE, summary_start) < 0) {
			MSG(1, "\tError: While reading summary page from disk!!!\n");
			ret = -1;
			goto free_summary_page;
		}
		summary_start ++;
	}

	summary_start -= (data_summary_count + 3);

	sm_info = calloc(sizeof(struct f2fs_sm_info), 1);
	if (sm_info == NULL) {
		ERR_MSG("\tError: Calloc Failed!\n");
		ret = -1;
		goto free_summary_page;
	}
	sbi->sm_info = sm_info;
	array = calloc(sizeof(*array), NR_CURSEG_TYPE);
	if (array == NULL) {
		ERR_MSG("\tError: Calloc Failed!\n");
		ret = -1;
		goto free_sm_info;
	}
	SM_I(sbi)->curseg_array = array;
	array[0].sum_blk = calloc(1, PAGE_CACHE_SIZE);
	if (array[0].sum_blk == NULL) {
		ERR_MSG("\tError: Calloc Failed!\n");
		ret = -1;
		goto free_array;
	}
	seg_i = CURSEG_I(sbi, CURSEG_HOT_DATA);
	void * zero_buf = calloc(PAGE_CACHE_SIZE, 1);
	if (zero_buf == NULL) {
		ERR_MSG("\tError: Calloc Failed!\n");
		ret = -1;
		goto free_array_sum_blk;
	}

	if(is_set_ckpt_flags(cp, CP_COMPACT_SUM_FLAG)) {
		/*hot data summary entries*/
		offset = 2 * SUM_JOURNAL_SIZE;
		unsigned short blk_off;
		void *src = summary_page;

		blk_off = le16_to_cpu(cp->cur_data_blkoff[CURSEG_HOT_DATA]);
		if(seg_i->alloc_type == SSR)
			blk_off = (1 << get_sb(log_blocks_per_seg));

		for (j = 0; j < blk_off; j++) {
			struct f2fs_summary s;
			memcpy(&s, (src + offset), SUMMARY_SIZE);
			memset((summary_page + offset), 0, SUMMARY_SIZE);
			seg_i->sum_blk->entries[j] = s;
			offset += SUMMARY_SIZE;
			if(offset >= PAGE_SIZE - SUM_FOOTER_SIZE)//footer
			{
				offset = 0;
				src += PAGE_SIZE;
			}
		}

		seg_i->sum_blk->footer.entry_type = SUM_TYPE_DATA;
		blk_addr = get_sb(ssa_blkaddr) + cp->cur_data_segno[0];
		if (dev_write_block(seg_i->sum_blk, blk_addr) < 0) {
			MSG(1, "\tError: While writing the old hot data summary to disk!!!\n");
			ret = -1;
			goto free_zero_buf;
		}

	} else {
		memcpy(seg_i->sum_blk, summary_page, SUM_ENTRY_SIZE);
		blk_addr = get_sb(ssa_blkaddr) + cp->cur_data_segno[0];
		if (dev_write_block(seg_i->sum_blk, blk_addr) < 0) {
			MSG(1, "\tError: While writing the old hot data summary to disk!!!\n");
			ret = -1;
			goto free_zero_buf;
		}

		memset(summary_page, 0, SUM_ENTRY_SIZE);
	}

	/*hot node summary*/
	offset = PAGE_SIZE * data_summary_count;
	blk_addr = get_sb(ssa_blkaddr) + cp->cur_node_segno[0];
	sum_blk2 = (struct f2fs_summary_block *)(summary_page + offset);
	sum_blk2->footer.entry_type = SUM_TYPE_NODE;
	if (dev_write_block(sum_blk2, blk_addr) < 0) {
		MSG(1, "\tError: While writing the old hot node summary to disk!!!\n");
		ret = -1;
		goto free_zero_buf;
	}

	/*warm node summary*/
	offset += PAGE_SIZE;
	blk_addr = get_sb(ssa_blkaddr) + cp->cur_node_segno[1];
	sum_blk2 = (struct f2fs_summary_block *)(summary_page + offset);
	sum_blk2->footer.entry_type = SUM_TYPE_NODE;
	if (dev_write_block(sum_blk2, blk_addr) < 0) {
		MSG(1, "\tError: While writing the old warm node summary to disk!!!\n");
		ret = -1;
		goto free_zero_buf;
	}

	/*cold node summary*/
	offset += PAGE_SIZE;
	sum_blk2 = (struct f2fs_summary_block *)(summary_page + offset);
	sum_blk2->footer.entry_type = SUM_TYPE_NODE;
	blk_addr = get_sb(ssa_blkaddr) + cp->cur_node_segno[2];
	if (dev_write_block(sum_blk2, blk_addr) < 0) {
		MSG(1, "\tError: While writing the old cold node summary to disk!!!\n");
		ret = -1;
		goto free_zero_buf;
	}

	/*new current summary*/
	for (i=0; i<data_summary_count; i++)
	{
		if (dev_write_block(summary_page + i*PAGE_SIZE, summary_start) < 0) {
			MSG(1, "Error: While writing the new data summary to disk!!!\n");
			ret = -1;
			goto free_zero_buf;
		}
		summary_start ++;
	}
	summary_start -= data_summary_count;

	struct f2fs_summary_block * sum_blk = (struct f2fs_summary_block *)zero_buf;
	sum_blk->footer.entry_type = SUM_TYPE_NODE;
	if (dev_write_block(sum_blk, summary_start + data_summary_count) < 0) {
		MSG(1, "Error: While writing the new hot node summary to disk!!!\n");
		ret = -1;
		goto free_zero_buf;
	}
	sum_blk->footer.entry_type = SUM_TYPE_NODE;
	if (dev_write_block(sum_blk, summary_start + data_summary_count + 1) < 0) {
		MSG(1, "Error: While writing the new warm node summary to disk!!!\n");
		ret = -1;
		goto free_zero_buf;
	}
	sum_blk->footer.entry_type = SUM_TYPE_NODE;
	if (dev_write_block(sum_blk, summary_start + data_summary_count + 2) < 0 ) {
		MSG(1, "Error: While writing the new cold node summary to disk!!!\n");
		ret = -1;
		goto free_zero_buf;
	}

	/*# of free segments changed*/
	if(cp->cur_data_blkoff[0] != 0)
			sum ++;
	for(i = 0; i < 3; i++) {
		if(cp->cur_node_blkoff[i] != 0)
			sum ++;
	}

	/* adjust new curseg info */
	cp->cur_data_segno[0] = get_sb(segment_count_main) -4;
	if (set_sit_entry(cp->cur_data_segno[0], 0) < 0) {
		MSG(1, "Error: While setting hot data sit entry!!!\n");
		ret = -1;
		goto free_zero_buf;
	}

	cp->cur_node_segno[0] = get_sb(segment_count_main) -1;
	cp->cur_node_blkoff[0] = 0;
	if (set_sit_entry(cp->cur_node_segno[0], 3) < 0) {
		MSG(1, "Error: While setting hot node sit entry!!!\n");
		ret = -1;
		goto free_zero_buf;
	}

	cp->cur_node_segno[1] = get_sb(segment_count_main) -2;
	cp->cur_node_blkoff[1] = 0;
	if (set_sit_entry(cp->cur_node_segno[1], 4) < 0) {
		MSG(1, "Error: While setting warm node sit entry!!!\n");
		ret = -1;
		goto free_zero_buf;
	}

	cp->cur_node_segno[2] = get_sb(segment_count_main) -3;
	cp->cur_node_blkoff[2] = 0;
	if (set_sit_entry(cp->cur_node_segno[2], 5) < 0) {
		MSG(1, "Error: While setting cold node sit entry!!!\n");
		ret = -1;
		goto free_zero_buf;
	}

	/*free segment have changed*/
	cp->free_segment_count -= sum;

	MSG(-1, "Info: finish f2fs_adjust_current_segment!!!\n");

free_zero_buf:
	free(zero_buf);
free_array_sum_blk:
	free(array[0].sum_blk);
free_array:
	free(array);
free_sm_info:
	free(sm_info);
free_summary_page:
	free(summary_page);
	if (ret == -1)
		MSG(-1, "Info: fail f2fs_adjust_current_segment!!!\n");
	return ret;
}

int f2fs_adjust_checkpoint(struct f2fs_sb_info *sbi)
{
	struct f2fs_checkpoint *cp;
	u_int32_t written_segment;
	u_int32_t crc = 0;
	u_int64_t cp_seg_blk_offset = 0;

	cp = sbi->ckpt;

	written_segment = old_segment_count - cp->free_segment_count;
	cp->rsvd_segment_count = config.reserved_segments;
	cp->overprov_segment_count = (get_sb(segment_count) -
				config.reserved_segments) * config.overprovision / 100;
	cp->overprov_segment_count += config.reserved_segments;

	cp->free_segment_count = get_sb(segment_count_main) - written_segment;

	cp->user_block_count = (get_sb(segment_count_main) - cp->overprov_segment_count) *
				(1<<get_sb(log_blocks_per_seg));

	/* print pre-cp info */
	print_ckpt_info(sbi);

	/* adjust current segment */
	if(f2fs_adjust_current_segment(sbi) < 0) {
		MSG(-1, "\t Error to adjust current segment\n");
		goto error;
	}

	/*print adj-cp info*/
	print_ckpt_info(sbi);

	/*write back cp*/
	crc = f2fs_cal_crc32(F2FS_SUPER_MAGIC, cp, CHECKSUM_OFFSET);
	*((__le32 *)((unsigned char *)cp + CHECKSUM_OFFSET)) =
			cpu_to_le32(crc);

	/* write adjusted cp back to device */
	if(sbi->cur_cp == 2)
		goto cp2;

	cp_seg_blk_offset = get_sb(segment0_blkaddr);
	cp_seg_blk_offset *= 1 << get_sb(log_blocksize);

	if (dev_write(cp, cp_seg_blk_offset, F2FS_BLKSIZE )) {
		MSG(1, "\tError: While writing the cp to disk!!!\n");
		goto error;
	}

	cp_seg_blk_offset += (cp->cp_pack_total_block_count - 1)
					* 1 << get_sb(log_blocksize);

	if (dev_write(cp, cp_seg_blk_offset, F2FS_BLKSIZE )) {
		MSG(1, "\tError: While writing the cp to disk!!!\n");
		goto error;
	}

	MSG(-1, "Info: finish f2fs_adjust_checkpoint\n");
	return 0;
cp2:
	crc = f2fs_cal_crc32(F2FS_SUPER_MAGIC, cp, CHECKSUM_OFFSET);
	*((__le32 *)((unsigned char *)cp + CHECKSUM_OFFSET)) =cpu_to_le32(crc);

	cp_seg_blk_offset = get_sb(segment0_blkaddr) +
					(1 << get_sb(log_blocks_per_seg));
	cp_seg_blk_offset *= 1 << get_sb(log_blocksize);

	if (dev_write(cp, cp_seg_blk_offset, F2FS_BLKSIZE)) {
		MSG(1, "\tError: While writing the cp to disk!!!\n");
		goto error;
	}

	cp_seg_blk_offset += (cp->cp_pack_total_block_count - 1)
					* 1 << get_sb(log_blocksize);

	if (dev_write(cp, cp_seg_blk_offset, F2FS_BLKSIZE)) {
		MSG(1, "\tError: While writing the cp to disk!!!\n");
		goto error;
	}
	MSG(0, "Info: finish f2fs_adjust_checkpoint\n");

	return 0;
error:
	return -1;
}
