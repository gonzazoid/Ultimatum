// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/services/storage/dom_storage/dom_storage_database.h"

#include "components/services/storage/dom_storage/dom_storage_database_leveldb.h"

namespace storage {

DomStorageDatabase::KeyValuePair::KeyValuePair() = default;

DomStorageDatabase::KeyValuePair::~KeyValuePair() = default;

DomStorageDatabase::KeyValuePair::KeyValuePair(KeyValuePair&&) = default;

DomStorageDatabase::KeyValuePair::KeyValuePair(const KeyValuePair&) = default;

DomStorageDatabase::KeyValuePair::KeyValuePair(Key key, Value value)
    : key(std::move(key)), value(std::move(value)) {}

DomStorageDatabase::KeyValuePair& DomStorageDatabase::KeyValuePair::operator=(
    KeyValuePair&&) = default;

DomStorageDatabase::KeyValuePair& DomStorageDatabase::KeyValuePair::operator=(
    const KeyValuePair&) = default;

bool DomStorageDatabase::KeyValuePair::operator==(
    const KeyValuePair& rhs) const {
  return std::tie(key, value) == std::tie(rhs.key, rhs.value);
}

// static
void DomStorageDatabaseFactory::OpenDirectory(
    const base::FilePath& directory,
    const std::string& name,
    const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    OpenCallback callback) {
  DomStorageDatabaseLevelDB::OpenDirectory(directory, name, memory_dump_id,
                                           std::move(blocking_task_runner),
                                           std::move(callback));
}

// static
void DomStorageDatabaseFactory::OpenInMemory(
    const std::string& name,
    const std::optional<base::trace_event::MemoryAllocatorDumpGuid>&
        memory_dump_id,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    OpenCallback callback) {
  DomStorageDatabaseLevelDB::OpenInMemory(name, memory_dump_id,
                                          std::move(blocking_task_runner),
                                          std::move(callback));
}

// static
void DomStorageDatabaseFactory::Destroy(
    const base::FilePath& directory,
    const std::string& name,
    scoped_refptr<base::SequencedTaskRunner> blocking_task_runner,
    base::OnceCallback<void(DbStatus)> callback) {
  DomStorageDatabaseLevelDB::Destroy(
      directory, name, std::move(blocking_task_runner), std::move(callback));
}

DbStatus DomStorageDatabase::GetAllKeys(std::vector<std::vector<uint8_t>>* keys) const {
  // TODO REWRITE!!!
  // if (!db_)
  //   return Status::IOError(kInvalidDatabaseMessage);

  // leveldb::Iterator* it = db_->NewIterator(leveldb::ReadOptions());
  // for (it->SeekToFirst(); it->Valid(); it->Next()) {
    // TODO use begin() & end() of Slice in v.131
  //   std::vector<uint8_t> key = std::vector<uint8_t>(it->key().data(), it->key().data() + it->key().size());
  //   keys->push_back(std::move(key));
  // }
  return DbStatus::OK();
}

}  // namespace storage
