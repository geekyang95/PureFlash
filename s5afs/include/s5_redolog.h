#ifndef s5_redolog_h__
#define s5_redolog_h__
#include <stdint.h>
#include <thread>

#include "afs_flash_store.h"

class S5RedoLog
{
	enum class ItemType : uint32_t {
		ALLOCATE_OBJ = 1,
		TRIM_OBJ = 2,
		FREE_OBJ = 3
	};

	class Item{
		int64_t phase;
		ItemType type;
		union {
			struct {
				struct lmt_key bkey;
				struct lmt_entry bentry;
				int free_list_head;
			} allocation;
			struct {
				struct lmt_key bkey;
				struct lmt_entry bentry;
				int trim_list_tail;
			}trim;
			struct {
				int obj_id;
				int trim_list_head;
				int free_list_tail;
			}free;
		};
	};

public:
	int64_t phase;
	size_t size;
	struct S5FlashStore* store;
	dev_handle_t fd;
	off_t start_offset;
	off_t current_offset;
	void* entry_buff;
	std::thread auto_save_thread;

	int init(struct S5FlashStore* ssd);
	int load();
	int replay();
	int discard();
	int log_allocation(const struct block_key* key, const struct block_entry* entry, int free_list_head);
	int log_free(int block_id, int trim_list_head, int free_list_tail);
	int log_trim(const struct block_key* key, const struct block_entry* entry, int trim_list_tail);
	int redo_allocation(Item* e);
	int redo_trim(Item* e);
	int redo_free(Item* e);
	int stop();
private:
	int write_entry();
};

#endif // s5_redolog_h__
