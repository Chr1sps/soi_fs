#include <fstream>
#include <bitset>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <iostream>

#include "fs.hpp"
#include "exceptions.hpp"


uint64_t FileSystem::get_current_time()
{
    return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::system_clock::now()
                    .time_since_epoch())
            .count();
}

int FileSystem::find_unused_inode()
{
    this->drive->seekg(this->inodes_offset);
    Inode inode;
    for (int i = 0; i < this->superblock.max_file_count; ++i)
    {
        this->drive->read(reinterpret_cast<char *>(&inode), sizeof(Inode));
        if (!(inode.flags & INODE_USED_MASK))
        {
            return i;
        }
    }
    throw NoEmptyInodesException();
}

void FileSystem::write_superblock()
{
    this->drive->write(reinterpret_cast<char *>(&superblock),
                       sizeof(superblock));
}

void
FileSystem::insert_block_data(DataBlock &block, char *data, int size, int pos)
{
    char *data_pointer = data;
    int end_pos = pos + size;
    for (int i = pos; i < end_pos && i < BLOCK_SIZE; ++i, ++data_pointer)
    {
        block.data[i] = *data_pointer;
    }
}

void FileSystem::write_block(uint64_t index, char *data, int size, int pos)
{
    DataBlock block = this->read_block(index);
    this->insert_block_data(block, data, size, pos);
    this->write_block(index, block);
}

void FileSystem::write_block(uint64_t index, DataBlock &block)
{
    this->drive->seekp(blocks_offset + index * sizeof(DataBlock));
    this->drive->write(reinterpret_cast<char *>(&block), sizeof(DataBlock));
}

FileSystem::DataBlock FileSystem::read_block(int index)
{
    DataBlock result;
    this->drive->seekg(blocks_offset + index * sizeof(DataBlock));
    this->drive->read(reinterpret_cast<char *>(&result), sizeof(DataBlock));
    return result;
}

FileSystem::Inode FileSystem::read_inode(int index)
{
    Inode result;
    this->drive->seekg(inodes_offset + index * sizeof(Inode));
    this->drive->read(reinterpret_cast<char *>(&result), sizeof(Inode));
    return result;
}

int FileSystem::get_file_data_block_count(uint64_t size)
{
    return (size + this->superblock.block_size - 1) /
           this->superblock.block_size;
}

int FileSystem::get_file_real_block_count(const Inode &inode)
{
    return get_file_real_block_count(inode.size);
}

int FileSystem::get_file_real_block_count(uint64_t size)
{
    int data_block_count = get_file_data_block_count(size);
    int result = data_block_count;
    if (data_block_count > INODE_PRIMARY_TABLE_SIZE)
    {
        ++result;
    }
    if (data_block_count >
        INODE_PRIMARY_TABLE_SIZE + INODE_BLOCK_POINTER_TABLE_SIZE)
    {
        result += 1 + (data_block_count - INODE_PRIMARY_TABLE_SIZE - 1) /
                      this->superblock.block_size;
    }
    return result;
}

uint32_t FileSystem::find_unused_block()
{
    for (uint32_t i = 0; i < this->superblock.block_count; ++i)
    {
        if (!read_bitmap(i))
        {
            return i;
        }
    }
    throw MemoryException();
}

[[nodiscard]]
uint32_t FileSystem::allocate_block()
{
    uint32_t new_block_index = find_unused_block();
    write_bitmap(new_block_index, true);
    ++this->superblock.occupied_count;
    --this->superblock.free_count;
    write_superblock();
    return new_block_index;
}

void FileSystem::release_block(uint32_t index)
{
    DataBlock empty = {{0}};
    write_bitmap(index, false);
    write_block(index, empty);
    --this->superblock.occupied_count;
    ++this->superblock.free_count;
    write_superblock();
}

int FileSystem::get_file_data_block_count(const Inode &inode)
{
    return get_file_data_block_count(inode.size);
}

void FileSystem::write_table_block_pointer(const int &table_block_index,
                                           const int &pointer_index,
                                           uint32_t pointer_value)
{
    this->write_block(table_block_index,
                      reinterpret_cast<char *>(&pointer_value),
                      sizeof(pointer_value),
                      pointer_index * sizeof(pointer_value));
}

void FileSystem::read_from_block(DataBlock &block, char *dest, int size,
                                 int pos)
{
    char *dest_p = dest;
    for (int i = 0; i < size; ++i, ++dest_p)
    {
        (*dest_p) = block.data[i + pos];
    }
}

void FileSystem::read_from_block(int index, char *dest, int size, int pos)
{
    DataBlock block = read_block(index);
    read_from_block(block, dest, size, pos); // 4, 0
}

uint64_t FileSystem::read_table_block_pointer(const int &table_block_index,
                                              const int &pointer_index)
{
    DataBlock table_block = read_block(table_block_index);
    uint32_t pointer;
    read_from_block(table_block,
                    reinterpret_cast<char *>(&pointer),
                    sizeof(uint32_t),
                    pointer_index *
                    sizeof(uint32_t));
    return pointer;
}

void FileSystem::resize_file(int index, uint64_t new_size)
{
    Inode inode = read_inode(index);
    int old_real_block_count = get_file_real_block_count(inode);
    int new_real_block_count = get_file_real_block_count(new_size);
    int old_data_block_count = get_file_data_block_count(inode);
    int new_data_block_count = get_file_data_block_count(new_size);
    if (new_real_block_count > MAX_INODE_BLOCK_COUNT)
    {
        throw FileSizeTooBigException();
    }

    // extending the file
    if (new_real_block_count > old_real_block_count)
    {
        for (int i = old_data_block_count; i < new_data_block_count; ++i)
        {
            uint32_t new_block_index = allocate_block();
            // allocating primaty table
            if (i < INODE_PRIMARY_TABLE_SIZE)
            {
                inode.data_pointers[i] = new_block_index;
            }
                // allocating secondary blocks
            else if (i <
                     INODE_PRIMARY_TABLE_SIZE + INODE_BLOCK_POINTER_TABLE_SIZE)
            {
                // allocating secondary table block
                if (i == INODE_PRIMARY_TABLE_SIZE)
                {
                    inode.secondary_data_table_block = allocate_block();

                }
                int secondary_index = i - INODE_PRIMARY_TABLE_SIZE;
                // writing to the secondary table block
                this->write_table_block_pointer(
                        inode.secondary_data_table_block,
                        secondary_index,
                        new_block_index);

            }
                // allocating ternary blocks
            else if (i < MAX_INODE_BLOCK_COUNT)
            {
                int ternary_intermediate_index = (i -
                                                  INODE_PRIMARY_TABLE_SIZE -
                                                  INODE_BLOCK_POINTER_TABLE_SIZE) /
                                                 INODE_BLOCK_POINTER_TABLE_SIZE;
                int ternary_data_index = (i - INODE_PRIMARY_TABLE_SIZE -
                                          INODE_BLOCK_POINTER_TABLE_SIZE) %
                                         INODE_BLOCK_POINTER_TABLE_SIZE;
                // allocating the main ternary table block
                if (i ==
                    INODE_PRIMARY_TABLE_SIZE + INODE_BLOCK_POINTER_TABLE_SIZE)
                {
                    inode.ternary_data_table_block = allocate_block();
                }
                // allocating intermediate ternary table blocks
                if (ternary_data_index == 0)
                {
                    this->write_table_block_pointer(
                            inode.ternary_data_table_block,
                            ternary_intermediate_index, allocate_block());
                }
                // writing actual data blocks
                uint32_t intermediate_block_pointer =
                        read_table_block_pointer(
                                inode.ternary_data_table_block,
                                ternary_intermediate_index);
                uint32_t data_block_pointer = allocate_block();
                write_table_block_pointer(intermediate_block_pointer,
                                          ternary_data_index,
                                          data_block_pointer);
            }
        }


    }
        // truncating the file
    else if (new_real_block_count < old_real_block_count)
    {
        for (int i = old_data_block_count - 1; i >= new_data_block_count; --i)
        {
            if (i < INODE_PRIMARY_TABLE_SIZE)
            {
                release_block(inode.data_pointers[i]);

            }
            else if (i <
                     INODE_PRIMARY_TABLE_SIZE + INODE_BLOCK_POINTER_TABLE_SIZE)
            {
                int secondary_index = i - INODE_PRIMARY_TABLE_SIZE;
                release_block(read_table_block_pointer(
                        inode.secondary_data_table_block, secondary_index));
                if (secondary_index == 0)
                {
                    release_block(inode.secondary_data_table_block);
                }
            }
            else if (i < MAX_INODE_BLOCK_COUNT)
            {
                int ternary_intermediate_index =
                        (i - INODE_PRIMARY_TABLE_SIZE -
                         INODE_BLOCK_POINTER_TABLE_SIZE) /
                        INODE_BLOCK_POINTER_TABLE_SIZE;
                int ternary_data_index =
                        (i - INODE_PRIMARY_TABLE_SIZE -
                         INODE_BLOCK_POINTER_TABLE_SIZE) %
                        INODE_BLOCK_POINTER_TABLE_SIZE;

                uint32_t intermediate_block_pointer =
                        read_table_block_pointer(
                                inode.ternary_data_table_block,
                                ternary_intermediate_index);
                uint32_t data_block_pointer = read_table_block_pointer(
                        intermediate_block_pointer, ternary_data_index);
                release_block(data_block_pointer);
                if (ternary_data_index == 0)
                {
                    this->release_block(intermediate_block_pointer);
                }
                if (intermediate_block_pointer == 0)
                {
                    this->release_block(inode.ternary_data_table_block);
                }
            }

        }

    }

    inode.size = new_size;
    inode.last_modified = superblock.last_modified = get_current_time();
    write_inode(index, inode);
}

void FileSystem::write_file(int index, char *data, uint64_t size,
                            uint64_t pos)
{
    Inode inode = this->read_inode(index);
    if (size == 0)
        return;


    uint64_t starting_block = pos / BLOCK_SIZE;
    int starting_block_offset = pos % BLOCK_SIZE;
    uint64_t ending_block = (pos + size - 1) / BLOCK_SIZE;
    int ending_block_offset = (pos + size - 1) % BLOCK_SIZE;

    uint64_t result_size = std::max(inode.size, pos + size);

    uint64_t current_block_count = (inode.size + BLOCK_SIZE - 1) / BLOCK_SIZE;
    uint64_t new_block_count = (result_size + BLOCK_SIZE - 1) / BLOCK_SIZE;

    // check if the new size will be bigger
    if (new_block_count > current_block_count)
    {
        // allocate blocks
        try
        {
            this->resize_file(index, size);
        }
        catch (const FileSizeTooBigException &e)
        {
            throw e;
        }
    }

    inode = this->read_inode(index);
    for (uint64_t block = starting_block;
         block <= ending_block; ++block, data += BLOCK_SIZE)
    {
        int start = (block == starting_block) ? (starting_block_offset) : (0);
        int end = (block == ending_block) ? (ending_block_offset)
                                          : (BLOCK_SIZE - 1);
        if (block < INODE_PRIMARY_TABLE_SIZE)
        {
            write_block(inode.data_pointers[block], data, end - start + 1,
                        start);
        }
        else if (block <
                 INODE_PRIMARY_TABLE_SIZE + INODE_BLOCK_POINTER_TABLE_SIZE)
        {
            int block_index = block - INODE_PRIMARY_TABLE_SIZE;
            uint64_t block_pointer = read_table_block_pointer(
                    inode.secondary_data_table_block, block_index);
            write_block(block_pointer, data, end - start + 1, start);
        }
        else if (block < MAX_INODE_BLOCK_COUNT)
        {
            int intermediate_block_index = (block - INODE_PRIMARY_TABLE_SIZE -
                                            INODE_BLOCK_POINTER_TABLE_SIZE) /
                                           INODE_BLOCK_POINTER_TABLE_SIZE;
            int data_block_index = (block - INODE_PRIMARY_TABLE_SIZE -
                                    INODE_BLOCK_POINTER_TABLE_SIZE) %
                                   INODE_BLOCK_POINTER_TABLE_SIZE;
            uint64_t intermediate_block_pointer = read_table_block_pointer(
                    inode.ternary_data_table_block, intermediate_block_index);
            uint64_t data_block_pointer = read_table_block_pointer(
                    intermediate_block_pointer, data_block_index);
            write_block(data_block_pointer, data, end - start + 1, start);
        }
    }


    inode.last_modified = superblock.last_modified = get_current_time();
    inode.size = result_size;
    this->write_inode(index, inode);
}

void FileSystem::read_file(int index, char *dest, uint64_t size, uint64_t pos)
{
    // 0, dest, 4, 0
    Inode inode = this->read_inode(index);
    if (size == 0)
        return;


    uint64_t last_byte_read = size + pos - 1;
    if (last_byte_read >= inode.size)
    {
        throw ReadTooBigException();
    }

    uint64_t starting_block = pos / BLOCK_SIZE;
    int starting_block_offset = pos % BLOCK_SIZE;
    uint64_t ending_block = (pos + size - 1) / BLOCK_SIZE;
    int ending_block_offset = (pos + size - 1) % BLOCK_SIZE;

    for (uint64_t block_index = starting_block;
         block_index <=
         ending_block; ++block_index, dest += BLOCK_SIZE)
    {
        int start = (block_index == starting_block) ? (starting_block_offset)
                                                    : (0);
        int end = (block_index == ending_block) ? (ending_block_offset)
                                                : (BLOCK_SIZE - 1);
        if (block_index < INODE_PRIMARY_TABLE_SIZE)
        {
            read_from_block(inode.data_pointers[block_index], dest,
                            end - start + 1,
                            start);
        }
        else if (block_index <
                 INODE_PRIMARY_TABLE_SIZE + INODE_BLOCK_POINTER_TABLE_SIZE)
        {
            int secondary_index = block_index - INODE_PRIMARY_TABLE_SIZE;
            uint64_t block_pointer = read_table_block_pointer(
                    inode.secondary_data_table_block, secondary_index);
            read_from_block(block_pointer, dest, end - start + 1, start);
        }
        else if (block_index < MAX_INODE_BLOCK_COUNT)
        {
            int intermediate_block_index =
                    (block_index - INODE_PRIMARY_TABLE_SIZE -
                     INODE_BLOCK_POINTER_TABLE_SIZE) /
                    INODE_BLOCK_POINTER_TABLE_SIZE;
            int data_block_index = (block_index - INODE_PRIMARY_TABLE_SIZE -
                                    INODE_BLOCK_POINTER_TABLE_SIZE) %
                                   INODE_BLOCK_POINTER_TABLE_SIZE;
            uint64_t intermediate_block_pointer = read_table_block_pointer(
                    inode.ternary_data_table_block, intermediate_block_index);
            uint64_t data_block_pointer = read_table_block_pointer(
                    intermediate_block_pointer, data_block_index);
            read_from_block(data_block_pointer, dest, end - start + 1, start);
        }
        if (block_index == ending_block)
        {
            break;
        }
    }

    this->write_inode(index, inode);
}

void FileSystem::write_inode(int index, Inode &inode)
{
    this->drive->seekp(inodes_offset + index * sizeof(Inode));
    this->drive->write(reinterpret_cast<char *>(&inode), sizeof(Inode));
}

bool FileSystem::read_bitmap(const int &index)
{
    int index_byte = index >> 3;
    int index_bit = 1 << (index & 7);
    this->drive->seekg(bitmap_offset + index_byte);
    auto gotten = this->drive->get();
    return gotten & index_bit;
}

void FileSystem::write_bitmap(int index, const bool &data)
{
    int index_byte = index >> 3;
    char index_bit = 1 << (index & 7);
    this->drive->seekg(bitmap_offset + index_byte);
    char existing;
    this->drive->get(existing);
    existing = existing & (~index_bit);
    if (data)
        existing = existing | index_bit;
    this->drive->seekp(bitmap_offset + index_byte);
    this->drive->put(existing);
//    this->drive->putback(existing);
}

bool FileSystem::is_name_unique(const std::string name,
                                const uint32_t parent_index)
{
    Inode parent = this->read_inode(parent_index);
    uint64_t remaining_size = parent.size;
    for (uint64_t pos = 0; pos < remaining_size;)
    {
        uint32_t name_size, inode_pointer;
        read_file(parent_index, reinterpret_cast<char *>( &inode_pointer ),
                  sizeof(uint32_t), pos);
        pos += sizeof(uint32_t);
        read_file(parent_index, reinterpret_cast<char *>( &name_size ),
                  sizeof(uint32_t), pos);
        pos += sizeof(uint32_t);
        char *name_buffer = new char[name_size + 1];
        read_file(parent_index, name_buffer,
                  sizeof(char) * name_size, pos);
        name_buffer[name_size] = '\0';
        pos += sizeof(char) * name_size;
        if (name == name_buffer)
        {
            delete[] name_buffer;
            return false;
        }
        delete[] name_buffer;
    }
    return true;
}

void FileSystem::add_inode_to_dir(const uint32_t &parent_index,
                                  const uint32_t &child_index,
                                  const std::string &file_name)
{
    Inode parent = read_inode(parent_index);
    struct
    {
        uint32_t inode_pointer;
        uint32_t name_size;
        const char *name;
    } record;
    record.inode_pointer = child_index;
    record.name_size = file_name.length();
    record.name = file_name.c_str();

    write_file(parent_index, reinterpret_cast<char *>(&record.inode_pointer),
               sizeof(uint32_t), parent.size);
    write_file(parent_index, reinterpret_cast<char *>(&record.name_size),
               sizeof(uint32_t), parent.size + sizeof(uint32_t));
    write_file(parent_index, const_cast<char *>(record.name),
               record.name_size, parent.size + 2 * sizeof(uint32_t));
}

void FileSystem::remove_inode_from_dir(const uint32_t &parent_index,
                                       const uint32_t &child_index)
{
    Inode parent = this->read_inode(parent_index);
    uint64_t remaining_size = parent.size;
    for (uint64_t pos = 0; pos < remaining_size;)
    {
        uint32_t name_size, inode_pointer;
        read_file(parent_index, reinterpret_cast<char *>( &inode_pointer ),
                  sizeof(uint32_t), pos);
        pos += sizeof(uint32_t);
        read_file(parent_index, reinterpret_cast<char *>( &name_size ),
                  sizeof(uint32_t), pos);
        pos += sizeof(uint32_t);
        pos += sizeof(char) * name_size;
        if (inode_pointer == child_index)
        {
            char *remaining_data = new char[parent.size - pos];
            read_file(parent_index, remaining_data, parent.size - pos, pos);
            write_file(parent_index, remaining_data, parent.size - pos,
                       pos - 2 * sizeof(uint32_t) - sizeof(char) * name_size);
            parent.size = parent.size - name_size - 2 * sizeof(uint32_t);
            this->write_inode(parent_index, parent);
            delete[] remaining_data;
            break;
        }
    }
}

uint32_t FileSystem::get_dir_inode(const int &dir_index,
                                   const std::string &name)
{
    uint64_t remaining_size = this->read_inode(dir_index).size;
    for (uint64_t pos = 0; pos < remaining_size;)
    {
        uint32_t inode_pointer, name_size;
        read_file(dir_index, reinterpret_cast<char *>( &inode_pointer ),
                  sizeof(uint32_t), pos);
        pos += sizeof(uint32_t);
        read_file(dir_index, reinterpret_cast<char *>( &name_size ),
                  sizeof(uint32_t), pos);
        pos += sizeof(uint32_t);
        char *name_buffer = new char[name_size + 1];
        read_file(dir_index, name_buffer, sizeof(char) * name_size, pos);
        name_buffer[name_size] = '\0';
        pos += sizeof(char) * name_size;
        if (name == name_buffer)
        {
            delete[] name_buffer;
            return inode_pointer;

        }
        delete[] name_buffer;
    }
    throw DirectoryNotFoundException();
}

uint32_t FileSystem::find_file_in_dir(const std::string &name)
{
    if (name == "/")
    {
        return 0;
    }
    std::vector<std::string> inters;
    std::stringstream dir(name);
    std::string intermediate;
    while (std::getline(dir, intermediate, '/'))
    {
        inters.push_back(intermediate);
    }
    if (inters[0] == "")
    {
        inters.erase(inters.begin());
    }
    uint32_t inode_index = 0;
    for (auto &name: inters)
    {
        inode_index = get_dir_inode(inode_index, name);
    }
    return inode_index;
}

void FileSystem::create_root()
{
    Inode root = read_inode(0);
    root.creation_time = get_current_time();
    root.last_modified = root.creation_time;
    root.size = 0;
    root.reference_count = 1;
    root.flags = 0b11000000;
    this->write_inode(0, root);
    add_inode_to_dir(0, 0, static_cast<const std::string &>("."));
    add_inode_to_dir(0, 0, static_cast<const std::string &>(".."));
    superblock.file_count++;
}

void FileSystem::create_file(const std::string &name,
                             const std::string &parent_name, FILE_TYPE type)
{
    uint32_t parent_index = find_file_in_dir(parent_name);
    if (!is_name_unique(name, parent_index))
        throw NonUniqueNameException();
    Inode parent_dir = this->read_inode(parent_index);

    if ((parent_dir.flags & INODE_MODE_MASK) != FILE_TYPE::DIR)
        throw NotADirectoryException();

    uint64_t block_index = this->allocate_block();

    int child_index = this->find_unused_inode();
    Inode inode;
    inode.creation_time = get_current_time();
    inode.last_modified = inode.creation_time;
    inode.data_pointers[0] = block_index;
    inode.reference_count = 1;
    inode.size = 1;
    inode.flags = type | INODE_USED_MASK;
    this->write_inode(child_index, inode);
    if (type == FILE_TYPE::DIR)
    {
        add_inode_to_dir(child_index, child_index,
                         static_cast<const std::string &>("."));
        add_inode_to_dir(child_index, parent_index,
                         static_cast<const std::string &>(".."));
    }
    ++superblock.file_count;
    write_superblock();
    this->write_inode(child_index, inode);
    this->add_inode_to_dir(parent_index, child_index, name);
}

void FileSystem::create_file(const std::string &name, FILE_TYPE type)
{
    std::string file_name = name;
    if (file_name[0] != '/')
        file_name = "/" + file_name;
    std::string parent_dir(file_name);
    if (parent_dir[parent_dir.length() - 1] == '/')
    {
        parent_dir = parent_dir.substr(0, parent_dir.length() - 1);
    }
    int dir_split_index = parent_dir.rfind("/");
    file_name = parent_dir.substr(dir_split_index + 1);
    parent_dir = parent_dir.substr(0, dir_split_index + 1);
    create_file(file_name, parent_dir, type);

}


void FileSystem::create_link(const std::string &link_name,
                             const std::string &linked_name)
{
    uint32_t linked_index = find_file_in_dir(linked_name);
    std::string parent_name = linked_name.substr(0,
                                                 linked_name.rfind("/") + 1);
    uint32_t parent_id = find_file_in_dir(parent_name);
    add_inode_to_dir(parent_id, linked_index, link_name);
    Inode linked = read_inode(linked_index);
    ++linked.reference_count;
    write_inode(linked_index, linked);
}

void FileSystem::init_inodes()
{
    this->drive->seekp(inodes_offset);
    Inode *inodes = new Inode[this->superblock.max_file_count]{0, 0, 0, {0}, 0,
                                                               0, 0, 0};
    this->drive->write(reinterpret_cast<char *>(inodes),
                       sizeof(inodes));
    delete[] inodes;
}

void FileSystem::init_bitmap()
{
    int bitmap_byte_count = (this->superblock.block_count + 7) >> 3;
    char *bitmap = new char[bitmap_byte_count]{0};
    this->drive->seekp(bitmap_offset);
    this->drive->write(bitmap, bitmap_byte_count);
    delete[] bitmap;
}

void FileSystem::init_blocks()
{
    this->drive->seekp(blocks_offset);
    DataBlock *blocks = new DataBlock[this->superblock.block_count];
    this->drive->write(reinterpret_cast<char *>(blocks),
                       this->superblock.block_count * sizeof(DataBlock));
    delete[] blocks;
}

void FileSystem::init_drive()
{
    this->drive->write(reinterpret_cast<char *>(&this->superblock),
                       sizeof(this->superblock));

    this->init_inodes();

    this->init_bitmap();

    this->init_blocks();

}

FileSystem::FileSystem(const std::string &file_name, const int &bytes)
{
    unsigned int block_count = (bytes + BLOCK_SIZE - 1) / BLOCK_SIZE;
    unsigned int bitmap_size = (block_count + 7) >> 3;

    this->superblock.last_modified = get_current_time();
    this->superblock.block_count = static_cast<uint16_t>(block_count);
    this->superblock.occupied_count = static_cast<uint16_t>(0);
    this->superblock.free_count = static_cast<uint16_t>(block_count);
    this->superblock.block_size = static_cast<uint16_t>(BLOCK_SIZE);
    this->superblock.max_file_count = static_cast<uint16_t>(MAX_FILE_COUNT);
    this->superblock.file_count = static_cast<uint16_t>(0);

    this->drive = std::make_unique<std::fstream>(file_name,
                                                 std::ios::in |
                                                 std::ios::out |
                                                 std::ios::trunc);

    this->inodes_offset = sizeof(superblock);
    this->bitmap_offset = this->inodes_offset +
                          this->superblock.max_file_count *
                          sizeof(Inode);
    this->blocks_offset = this->bitmap_offset + bitmap_size;
    std::cout << this->inodes_offset << " " << this->bitmap_offset << " "
              << this->blocks_offset << std::endl;

    this->init_drive();

    this->create_root();

}

FileSystem::~FileSystem()
{
    this->drive->close();
}

void FileSystem::cplocal(const std::string &local_name,
                         const std::string &virtual_name)
{
    std::fstream local_stream(local_name, std::ios::in);
    local_stream.seekg(0, std::ios::end);
    size_t size = local_stream.tellg();
    local_stream.seekg(0);
    char *data = new char[size];
    local_stream.read(data, size);
    create_file(
            ((virtual_name[0] == '/') ? (virtual_name) : ("/" + virtual_name)),
            FILE_TYPE::FILE);
    int index = this->find_file_in_dir(virtual_name);
    Inode root = read_inode(0);
    write_file(index, data, size, 0);
    root = read_inode(0);
    local_stream.close();
}

void FileSystem::cpvirtual(const std::string &virtual_name,
                           const std::string &local_name)
{
    std::fstream local_stream(local_name, std::ios::out | std::ios::trunc);

    int index = this->find_file_in_dir(virtual_name);
    Inode inode = read_inode(index);
    char *data = new char[inode.size];
    read_file(index, data, inode.size, 0);
    local_stream.write(data, inode.size);
    local_stream.close();
}

void FileSystem::mkdir(const std::string &name)
{

    std::string file_name = name;
    if (name == "/")
        throw std::exception();
    if (name[0] != '/')
        file_name = "/" + file_name;
    std::vector<std::string> inters;
    std::stringstream dir(file_name);
    std::string intermediate;
    while (std::getline(dir, intermediate, '/'))
    {
        inters.push_back(intermediate);
    }
    if (inters[0] == "")
    {
        inters.erase(inters.begin());
    }
    std::string parent_name = "/";
    for (auto &inter: inters)
    {
        try
        {
            find_file_in_dir(parent_name + inter);
        }
        catch (const DirectoryNotFoundException &e)
        {
            this->create_file(inter, parent_name, FILE_TYPE::DIR);
        }
        parent_name.append(inter + "/");
    }
}

/*

void FileSystem::rmdir(const std::string &dir_name)
{

}
*/

void FileSystem::rm(const std::string &file_name)
{
    std::string name = file_name;
    if (name[0] != '/')
        name = "/" + name;
    int index = this->find_file_in_dir(name);
    Inode inode = read_inode(index);
    int parent_index = this->find_file_in_dir(
            name.substr(0, name.rfind("/") + 1));
    if ((inode.flags & INODE_MODE_MASK) == FILE_TYPE::DIR)
    {
        throw NotAFileException();
    }
    --inode.reference_count;
    if (inode.reference_count == 0)
    {
        resize_file(index, 0);
        inode = read_inode(index);
        remove_inode_from_dir(parent_index, index);
        inode.flags = 0b00000000;
        inode.creation_time = 0;
        --superblock.file_count;
    }
    write_inode(index, inode);
    superblock.last_modified = get_current_time();
    write_superblock();
}

//
//void
//FileSystem::link(const std::string &file_name, const std::string &link_name)
//{
//    uint32_t linked_index = find_file_in_dir(link_name);
//
//}
//
void FileSystem::extend(const std::string &name, int bytes)
{
    int dir_index = this->find_file_in_dir(name);
    Inode inode = read_inode(dir_index);
    if ((inode.flags & INODE_MODE_MASK) != FILE_TYPE::FILE)
    {
        throw NotAFileException();
    }
    resize_file(dir_index, inode.size + bytes);
}


void FileSystem::truncate(const std::string &name, int bytes)
{
    int dir_index = this->find_file_in_dir(name);
    Inode inode = read_inode(dir_index);
    if ((inode.flags & INODE_MODE_MASK) != FILE_TYPE::FILE)
    {
        throw NotAFileException();
    }
    resize_file(dir_index, inode.size - bytes);
}


std::string FileSystem::ls(const std::string &directory)
{
    std::stringstream result;
    std::string dir_str = directory;
    if (directory == "")
    {
        dir_str = "/";
    }
    int dir_index = find_file_in_dir(dir_str);
    Inode temp = this->read_inode(dir_index);
    uint64_t remaining_size = temp.size;
    result << dir_str << " size: " << remaining_size << std::endl;
    for (uint64_t pos = 0; pos < remaining_size;)
    {
        uint32_t name_size, inode_pointer;
        read_file(dir_index, reinterpret_cast<char *>( &inode_pointer ),
                  sizeof(uint32_t), pos);
        Inode inode = read_inode(inode_pointer);
        mask_type type = inode.flags & INODE_MODE_MASK;
        pos += sizeof(uint32_t);
        read_file(dir_index, reinterpret_cast<char *>( &name_size ),
                  sizeof(uint32_t), pos);
        pos += sizeof(uint32_t);
        char *name_buffer = new char[name_size + 1];
        read_file(dir_index, name_buffer, sizeof(char) * name_size, pos);
        name_buffer[name_size] = '\0';
        switch (type)
        {
            case FILE_TYPE::FILE:
                result << "F ";
                break;
            case FILE_TYPE::DIR:
                result << "D ";
                break;
            case FILE_TYPE::LINK:
                result << "L ";
                break;
        }
        result << name_buffer << ((type == FILE_TYPE::DIR) ? ("/ ") : (" "))
               << inode.size << std::endl;
        pos += sizeof(char) * name_size;
        delete[] name_buffer;
    }
    return result.str();
}

std::string FileSystem::df()
{
    std::stringstream result;
    result << "Block count (used/free): " << this->superblock.block_count
           << " (" << this->superblock.occupied_count << " / "
           << this->superblock.free_count << ")." << std::endl;
    result << "Inode count: " << superblock.max_file_count << " (used: "
           << superblock.file_count << ")." << std::endl;
    return result.str();
}