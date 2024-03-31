/**
 * @file ramdrv.c
 * @author yduchesne
 * @brief Basic block driver implementation.
 *
 * Largely based on https://blog.pankajraghav.com/2022/11/30/BLKRAM.html.
 *
 * Objectives:
 *
 * - To serve as a block driver template.
 * - To serve as learning tool (documentation/comments were added to
 *   enhance explanations).
 *
 */

// Logging 
// see: https://www.kernel.org/doc/html/next/core-api/printk-basics.html
#define pr_fmt(fmt) "%s.%s[%d] " fmt, KBUILD_MODNAME, __func__, __LINE__

#include "asm/page.h"
#include "linux/blk_types.h"
#include "linux/sysfb.h"
#include <linux/module.h>
#include <linux/blkdev.h>
#include <linux/blk-mq.h>
#include <linux/idr.h>

// Units
#define KERNEL_SECTOR_SIZE 512
#define KB_PER_MB 1024
#define B_PER_MB (KB_PER_MB * 1024)

uint32_t capacity_mb = 40;
uint32_t max_segments = 32;
uint32_t max_segment_size = 65536;
uint32_t lbs = PAGE_SIZE;
uint32_t pbs = PAGE_SIZE;

/**
 * @brief Struct used to preserve the driver's in-memory state.
 *
 */
struct blk_ram_dev_t
{
	/**
	 * @brief Storage capacity in number of 512-byte sectors.
	 *
	 */
	sector_t capacity_num_sectors;

	/**
	 * @brief The driver's data (kept in memory).
	 *
	 */
	u8 *data;
	struct blk_mq_tag_set tag_set;

	/**
	 * @brief Corresponds to "our" RAM disk device.
	 *
	 * struct gendisk is the kernel's representation of an individual disk
	 * devices. The following fields of gendisk must be initialized:
	 *
	 * - major: the major number of this device: either a static major assigned
	 *          to this driver or one that is obtained dynamically from
	 *          register_blkdev().
	 * - first_minor: the first minor number to use.
	 * - minors: the number of minor numbers to allocate. A drive must use at
	 *           least one minor number.
	 * 	 - If a drive is partitionable, there has to be one minor number for each
	 *     possible partition.
	 *   - A common value for minors is 16, which allows for the full disk device
	 *     and 15 partitions.
	 *   - Some disk drivers use 64 minor numbers for each device.
	 * - disk_name: the disk's name, used in places like /proc/partitions and in
	 *              creating a sysfs directory for the device.
	 *
	 */
	struct gendisk *disk;
};

static int major;
static DEFINE_IDA(blk_ram_indexes);
static struct blk_ram_dev_t *blk_ram_dev = NULL;

static blk_status_t blk_ram_queue_rq(struct blk_mq_hw_ctx *hctx,
									 const struct blk_mq_queue_data *bd)
{
	struct request *rq = bd->rq;
	blk_status_t err = BLK_STS_OK;
	struct bio_vec bv;
	struct req_iterator iter;
	struct blk_ram_dev_t *blkram = hctx->queue->queuedata;

	// Shifting the sector number to obtain the block offset in number of byte
	// (SECTOR_SHIFT is set to 9: sectors are traditionally 512 bytes in size
	// and shifting left by 9 bits is equivalent to multiplying by 512).
	// The following formula can be referred to:
	//   block_offset = sector_num << SECTOR_SHIFT.
	loff_t pos = blk_rq_pos(rq) << SECTOR_SHIFT;
	// Similarly: number of sectors to number of bytes
	loff_t capacity_bytes = blkram->capacity_num_sectors << SECTOR_SHIFT;

	blk_mq_start_request(rq);

	rq_for_each_segment(bv, rq, iter)
	{
		unsigned int len = bv.bv_len;
		void *buf = page_address(bv.bv_page) + bv.bv_offset;

		// Ensure requested length is within device's capacity.
		if (pos + len > capacity_bytes)
		{
			err = BLK_STS_IOERR;
			break;
		}

		switch (req_op(rq))
		{
		case REQ_OP_READ:
			memcpy(buf, blkram->data + pos, len);
			break;
		case REQ_OP_WRITE:
			memcpy(blkram->data + pos, buf, len);
			break;
		default:
			err = BLK_STS_IOERR;
			goto end_request;
		}
		pos += len;
	}

end_request:
	blk_mq_end_request(rq, err);
	return BLK_STS_OK;
}

static const struct blk_mq_ops blk_ram_mq_ops = {
	.queue_rq = blk_ram_queue_rq,
};

static const struct block_device_operations blk_ram_rq_ops = {
	.owner = THIS_MODULE,
};

// ============================================================================
// Lifecycle

/**
 * @brief Performs registrations and other init tasks.
 *
 * Mainly, registration consists of:
 *
 * 1. Registering the driver.
 * 2. Registering the disk.
 *
 * @return int status code.
 */
static int __init blk_ram_init(void)
{
	pr_notice("-> Initializing...");
	int ret = 0;
	int minor;
	struct gendisk *disk;
	uint64_t capacity_bytes = capacity_mb * B_PER_MB; //capacity_mb >> 20;
	pr_notice("capacity_mb=0x%x (%u)", capacity_mb, capacity_mb);
	pr_notice("capacity_bytes=0x%llx (%llu)", capacity_bytes, capacity_bytes);

	pr_notice("Calling register_blkdev");
	ret = register_blkdev(0, "blkram");
	if (ret < 0)
		return ret;

	// Returned value is major no.
	major = ret;
	pr_notice("Got major no: %d", major);

	// Allocating memory for driver state
	blk_ram_dev = kzalloc(sizeof(struct blk_ram_dev_t), GFP_KERNEL);

	if (blk_ram_dev == NULL)
	{
		pr_err("memory allocation failed for blk_ram_dev");
		ret = -ENOMEM;
		goto unregister_blkdev;
	}

	// Capacity in number of sectors
	blk_ram_dev->capacity_num_sectors = capacity_bytes >> SECTOR_SHIFT;
	blk_ram_dev->data = kvmalloc(capacity_bytes, GFP_KERNEL);
	pr_notice("blk_ram_dev->capacity_num_sectors: %llu", blk_ram_dev->capacity_num_sectors);

	if (blk_ram_dev->data == NULL)
	{
		pr_err("memory allocation failed for the RAM disk");
		ret = -ENOMEM;
		goto data_err;
	}

	// Initializing tag set
	memset(&blk_ram_dev->tag_set, 0, sizeof(blk_ram_dev->tag_set));
	blk_ram_dev->tag_set.ops = &blk_ram_mq_ops;
	blk_ram_dev->tag_set.queue_depth = 128;
	blk_ram_dev->tag_set.numa_node = NUMA_NO_NODE;
	blk_ram_dev->tag_set.flags = BLK_MQ_F_SHOULD_MERGE;
	blk_ram_dev->tag_set.cmd_size = 0;
	blk_ram_dev->tag_set.driver_data = blk_ram_dev;
	blk_ram_dev->tag_set.nr_hw_queues = 1;

	ret = blk_mq_alloc_tag_set(&blk_ram_dev->tag_set);
	if (ret)
		goto data_err;

	// Allocating struct gendisk instance
	disk = blk_ram_dev->disk =
		blk_mq_alloc_disk(&blk_ram_dev->tag_set, blk_ram_dev);

	blk_queue_logical_block_size(disk->queue, lbs);
	blk_queue_physical_block_size(disk->queue, pbs);
	blk_queue_max_segments(disk->queue, max_segments);
	blk_queue_max_segment_size(disk->queue, max_segment_size);

	if (IS_ERR(disk))
	{
		ret = PTR_ERR(disk);
		pr_err("Error allocating a disk");
		goto tagset_err;
	}

	// This is not necessary as we don't support partitions, and creating
	// more RAM backed devices with the existing module
	minor = ret = ida_alloc(&blk_ram_indexes, GFP_KERNEL);
	if (ret < 0)
		goto cleanup_disk;

	disk->major = major;
	disk->first_minor = minor;
	disk->minors = 1;
	snprintf(disk->disk_name, DISK_NAME_LEN, "blkram");
	disk->fops = &blk_ram_rq_ops;
	disk->flags = GENHD_FL_NO_PART;
	set_capacity(disk, blk_ram_dev->capacity_num_sectors);

	pr_notice("Disk attributes:");
	pr_notice("- major: %d", disk->major);
	pr_notice("- first_minor: %d", disk->first_minor);
	pr_notice("- minors: %d", disk->minors);

	ret = add_disk(disk);
	if (ret < 0)
		goto cleanup_disk;

	pr_notice("<- Initialization completed\n");
	return 0;

cleanup_disk:
	put_disk(blk_ram_dev->disk);
tagset_err:
	kfree(blk_ram_dev->data);
data_err:
	kfree(blk_ram_dev);
unregister_blkdev:
	unregister_blkdev(major, "blkram");

	pr_notice("<- Initialization failed\n");
	return ret;
}

static void __exit blk_ram_exit(void)
{
	pr_notice("-> Exiting module...\n");
	if (blk_ram_dev->disk)
	{
		del_gendisk(blk_ram_dev->disk);
		put_disk(blk_ram_dev->disk);
	}
	unregister_blkdev(major, "blkram");
	kfree(blk_ram_dev);

	pr_notice("<- Exited module\n");
}

module_init(blk_ram_init);
module_exit(blk_ram_exit);

MODULE_AUTHOR("yduchesne");
MODULE_LICENSE("GPL");
