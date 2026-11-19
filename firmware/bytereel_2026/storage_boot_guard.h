#pragma once

enum BrStorageRecovery:uint8_t { BR_STORAGE_RECOVERY_MOUNTED,BR_STORAGE_RECOVERY_REQUIRE_STOP,BR_STORAGE_RECOVERY_REFUSE_DATA,BR_STORAGE_RECOVERY_FORMAT_BLANK };
static inline BrStorageRecovery br_storage_recovery(bool mounted,bool stopped,bool wholePartitionErased){
  if(mounted)return BR_STORAGE_RECOVERY_MOUNTED;
  if(!stopped)return BR_STORAGE_RECOVERY_REQUIRE_STOP;
  return wholePartitionErased?BR_STORAGE_RECOVERY_FORMAT_BLANK:BR_STORAGE_RECOVERY_REFUSE_DATA;
}
enum BrPartitionCheck:uint8_t { BR_PARTITION_ERASED,BR_PARTITION_HAS_DATA,BR_PARTITION_READ_FAILED };
static inline BrStorageRecovery br_storage_recovery_checked(bool mounted,bool stopped,BrPartitionCheck check){
  if(mounted)return BR_STORAGE_RECOVERY_MOUNTED;
  if(!stopped)return BR_STORAGE_RECOVERY_REQUIRE_STOP;
  return check==BR_PARTITION_ERASED?BR_STORAGE_RECOVERY_FORMAT_BLANK:BR_STORAGE_RECOVERY_REFUSE_DATA;
}
