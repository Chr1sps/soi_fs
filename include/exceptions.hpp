#ifndef __EXCEPTIONS_HPP__
#define __EXCEPTIONS_HPP___

#include <exception>

class NoEmptyInodesException : public std::exception
{
public:
    const char *what()
    { return "No empty inodes."; }
};

class NotADirectoryException : public std::exception
{
public:
    const char *what()
    { return "Not a directory."; }
};

class NotAFileException : public std::exception
{
public:
    const char *what()
    { return "Not a file."; }
};

class MemoryException : public std::exception
{
public:
    const char *what()
    { return "Not enough memory."; }
};

class FileSizeTooBigException : public std::exception
{
public:
    const char *what()
    { return "File size too big to be supported."; }
};

class ReadTooBigException : public std::exception
{
public:
    const char *what()
    { return "Read position and size exceeding file bounds."; }
};

class DirectoryNotFoundException : public std::exception
{
public:
    const char *what()
    { return "Directory not found."; }
};

class FileNotFoundException : public std::exception
{
public:
    const char *what()
    { return "File not found."; }
};

class NonUniqueNameException : public std::exception
{
public:
    const char *what()
    { return "Name already exists."; }
};

#endif