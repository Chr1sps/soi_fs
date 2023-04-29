#include <iostream>
#include <fstream>
#include <chrono>
#include <sstream>
#include "fs.hpp"
#include "exceptions.hpp"

int main(int argc, char **argv)
{
    if (argc == 3)
    {
        FileSystem fs(argv[1], atoi(argv[2]));
        for (std::string line, command, first_arg, second_arg;
             std::cout << ":> "; command = "", first_arg = "", second_arg = "")
        {
            if (!getline(std::cin, line))
            {
                return 0;
            }
            std::stringstream line_stream(line);
            line_stream >> command >> first_arg >> second_arg;
            if (command == "ls")
                std::cout << fs.ls(first_arg) << std::endl;
            else if (command == "upload")
                try
                {
                    fs.cplocal(first_arg, second_arg);
                }
                catch (NonUniqueNameException &e)
                {
                    std::cout << e.what() << std::endl;
                }
            else if (command == "extract")
                fs.cpvirtual(first_arg, second_arg);
            else if (command == "mkdir")
                fs.mkdir(first_arg);
            //            else if (command == "rmdir")
            //                fs.rmdir(first_arg);
            else if (command == "rm")
                fs.rm(first_arg);
            else if (command == "extend")
                fs.extend(first_arg, stoi(second_arg));
            else if (command == "truncate")
                fs.truncate(first_arg, stoi(second_arg));
            else if (command == "remove")
                fs.rm(first_arg);
            else if (command == "df")
                std::cout << fs.df() << std::endl;
            else if (command == "help" || command == "h")
            {
                std::cout << "ls <dir> - prints dir content." << std::endl;
                std::cout
                    << "upload <local_file> <virtual_file> - copies a local file into the file system."
                    << std::endl;
                std::cout
                    << "extract <virtual_file> <local_file> - extracts a virtual file into a local file."
                    << std::endl;
                std::cout << "extend <file> <bytes> - extends file size."
                          << std::endl;
                std::cout
                    << "truncate <file> <bytes> - truncates file size."
                    << std::endl;
                std::cout << "df - prints file system usage." << std::endl;
                std::cout << "rm <file> - deletes a virtual file."
                          << std::endl;
                std::cout << "h|help - shows this help text." << std::endl;
            }
            else if (command == "exit")
                break;
        }
    }
    else
    {
        std::cout << "Usage: ./fs.out <file_name> <size_in_bytes>"
                  << std::endl;
    }
    return 0;
}