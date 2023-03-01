#include <string>
#include <fstream>
#include <memory>
#include <vector>

class FileSystem
{
    typedef uint8_t mask_type;
    static const uint64_t ID = 0x00BEAFEDDEADBEEF;
    static const int MAX_NAME_LENGTH = 256;
    static const uint16_t MAX_FILE_COUNT = 256;
    static const int BLOCK_SIZE = 4096;
    static const int INODE_PRIMARY_TABLE_SIZE = 15;
    static const int INODE_BLOCK_POINTER_TABLE_SIZE = BLOCK_SIZE / 4;
    static const int MAX_INODE_BLOCK_COUNT =
            INODE_PRIMARY_TABLE_SIZE +
            INODE_BLOCK_POINTER_TABLE_SIZE *
            (INODE_BLOCK_POINTER_TABLE_SIZE + 1);

    static const mask_type INODE_USED_MASK = 0b10000000;
    static const mask_type INODE_MODE_MASK = 0b01100000;

    enum FILE_TYPE
    {
        FILE = 0b00100000, DIR = 0b01000000, LINK = 0b01100000
    };

    static uint64_t get_current_time();

    struct
    {
        const uint64_t id = FileSystem::ID;
        int64_t last_modified;
        uint32_t block_count;
        uint32_t occupied_count;
        uint32_t free_count;
        uint16_t block_size;
        uint16_t max_file_count;
        uint16_t file_count;
    } superblock;

    typedef struct
    {
        uint64_t creation_time;
        uint64_t last_modified;
        uint64_t size;
        uint32_t data_pointers[INODE_PRIMARY_TABLE_SIZE];
        uint32_t secondary_data_table_block;
        uint32_t ternary_data_table_block;
        uint16_t reference_count;
        mask_type flags; // UMMSTst0
    } Inode;

    typedef struct
    {
        char data[BLOCK_SIZE];
    } DataBlock;

    unsigned long inodes_offset;
    unsigned long bitmap_offset;
    unsigned long blocks_offset;

    std::unique_ptr<std::fstream> drive;

    void write_superblock();

    void insert_block_data(DataBlock &, char *, int, int); // done

    int get_file_data_block_count(uint64_t);

    int get_file_data_block_count(const Inode &);

    int get_file_real_block_count(uint64_t);

    int get_file_real_block_count(const Inode &);

    uint32_t find_unused_block(); // done

    uint32_t allocate_block(); // done

    void release_block(uint32_t index); // done

    void resize_file(int index, uint64_t size); // done :)))))))))

    void
    write_file(int index, char *data, uint64_t size, uint64_t pos); // done

    void read_file(int index, char *dest, uint64_t size, uint64_t pos); // done

    void write_inode(int, Inode &); // done

    Inode read_inode(int index); // done

    void write_block(uint64_t index, char *data, int size, int pos); // done

    void write_block(uint64_t index, DataBlock &block); // done

    DataBlock read_block(int index); // done

    void read_from_block(DataBlock &block, char *dest, int size,
                         int pos); // done, working :)

    void read_from_block(int index, char *dest, int size, int pos); // done

    void write_table_block_pointer(const int &table_block_index,
                                   const int &pointer_index,
                                   uint32_t pointer_value); // git gud

    uint64_t read_table_block_pointer(const int &table_block_index,
                                      const int &pointer_index); // git gud

    void write_bitmap(int, const bool &); // done

    bool read_bitmap(const int &); // done

    void init_inodes(); // git gud

    void init_bitmap(); // git gud

    void init_blocks(); // git gud

    void init_drive(); // git gud

    bool is_name_unique(const std::string name, const uint32_t parent_index);

    void
    add_inode_to_dir(const uint32_t &parent_index, const uint32_t &child_index,
                     const std::string &file_name); // gud

    void remove_inode_from_dir(const uint32_t &parent_index,
                               const uint32_t &child_index);

    void create_root();

    uint32_t
    get_dir_inode(const int &dir_index, const std::string &name); // done

    uint32_t find_file_in_dir(const std::string &name); // done

    void
    create_link(const std::string &link_name, const std::string &linked_name);

    void create_file(const std::string &name, const std::string &parent_name,
                     FILE_TYPE type);

    void create_file(const std::string &name, FILE_TYPE type);

    int find_unused_inode(); // git gud


public:
    FileSystem(const std::string &, const int &);

    virtual ~FileSystem();

    void test();

    void
    cplocal(const std::string &local_name, const std::string &virtual_name);

    void
    cpvirtual(const std::string &virtual_name, const std::string &local_name);

    void mkdir(const std::string &name);

//    void rmdir(const std::string &dir_name);

    void rm(const std::string &file_name); // done

    void link(const std::string &file_name, const std::string &link_name);

    void extend(const std::string &name, int bytes); // done

    void truncate(const std::string &name, int bytes); // done

    std::string ls(const std::string &directory); // done

    std::string df(); // done

};