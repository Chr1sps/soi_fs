#ifndef __EXCEPTIONS_HPP__
#define __EXCEPTIONS_HPP__

#include <exception>

class NoEmptyInodesException : public std::exception
{
public:
    const char *what() const noexcept override
    {
        return "No empty inodes.";
    }
};

class NotADirectoryException : public std::exception
{
public:
    const char *what() const noexcept override
    {
        return "Not a directory.";
    }
};

class NotAFileException : public std::exception
{
public:
    const char *what() const noexcept override
    {
        return "Not a file.";
    }
};

class MemoryException : public std::exception
{
public:
    const char *what() const noexcept override
    {
        return "Not enough memory.";
    }
};

class FileSizeTooBigException : public std::exception
{
public:
    const char *what() const noexcept override
    {
        return "File size too big to be supported.";
    }
};

class ReadTooBigException : public std::exception
{
public:
    const char *what() const noexcept override
    {
        return "Read position and size exceeding file bounds.";
    }
};

class DirectoryNotFoundException : public std::exception
{
public:
    const char *what() const noexcept override
    {
        return "Directory not found.";
    }
};

class FileNotFoundException : public std::exception
{
public:
    const char *what() const noexcept override
    {
        return "File not found.";
    }
};

class NonUniqueNameException : public std::exception
{
public:
    const char *what() const noexcept override
    {
        return "Name already exists.";
    }
};

#endif