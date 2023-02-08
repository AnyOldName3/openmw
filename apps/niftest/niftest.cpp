/// Program to test .nif files both on the FileSystem and in BSA archives.

#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <components/files/configurationmanager.hpp>
#include <components/files/constrainedfilestream.hpp>
#include <components/files/conversion.hpp>
#include <components/misc/strings/algorithm.hpp>
#include <components/nif/niffile.hpp>
#include <components/vfs/archive.hpp>
#include <components/vfs/bsaarchive.hpp>
#include <components/vfs/filesystemarchive.hpp>
#include <components/vfs/manager.hpp>

#include <boost/program_options.hpp>

// Create local aliases for brevity
namespace bpo = boost::program_options;

/// See if the file has the named extension
bool hasExtension(const std::filesystem::path& filename, const std::string& extensionToFind)
{
    const auto extension = Files::pathToUnicodeString(filename.extension());
    return Misc::StringUtils::ciEqual(extension, extensionToFind);
}

/// See if the file has the "nif" extension.
bool isNIF(const std::filesystem::path& filename)
{
    return hasExtension(filename, ".nif");
}
/// See if the file has the "bsa" extension.
bool isBSA(const std::filesystem::path& filename)
{
    return hasExtension(filename, ".bsa");
}

std::unique_ptr<VFS::Archive> makeBsaArchive(const std::filesystem::path& path)
{
    switch (Bsa::CompressedBSAFile::detectVersion(path))
    {
        case Bsa::BSAVER_UNKNOWN:
            std::cerr << '"' << path << "\" is unknown BSA archive" << std::endl;
            return nullptr;
        case Bsa::BSAVER_UNCOMPRESSED:
            return std::make_unique<VFS::BsaArchive>(path);
        case Bsa::BSAVER_COMPRESSED:
            return std::make_unique<VFS::CompressedBsaArchive>(path);
    }

    std::cerr << '"' << path << "\" is unsupported BSA archive" << std::endl;

    return nullptr;
}

/// Check all the nif files in a given VFS::Archive
/// \note Can not read a bsa file inside of a bsa file.
void readVFS(std::unique_ptr<VFS::Archive>&& anArchive, const std::filesystem::path& archivePath = {})
{
    if (anArchive == nullptr)
        return;

    VFS::Manager myManager(true);
    myManager.addArchive(std::move(anArchive));
    myManager.buildIndex();

    for (const auto& name : myManager.getRecursiveDirectoryIterator(""))
    {
        try
        {
            if (isNIF(name))
            {
                //           std::cout << "Decoding: " << name << std::endl;
                Nif::NIFFile file(archivePath / name);
                Nif::Reader reader(file);
                reader.parse(myManager.get(name));
            }
            else if (isBSA(name))
            {
                if (!archivePath.empty() && !isBSA(archivePath))
                {
                    //                     std::cout << "Reading BSA File: " << name << std::endl;
                    readVFS(makeBsaArchive(archivePath / name), archivePath / name);
                    //                     std::cout << "Done with BSA File: " << name << std::endl;
                }
            }
        }
        catch (std::exception& e)
        {
            std::cerr << "ERROR, an exception has occurred:  " << e.what() << std::endl;
        }
    }
}

bool parseOptions(int argc, char** argv, std::vector<Files::MaybeQuotedPath>& files)
{
    bpo::options_description desc(R"(Ensure that OpenMW can use the provided NIF and BSA files

Usages:
  niftool <nif files, BSA files, or directories>
      Scan the file or directories for nif errors.

Allowed options)");
    auto addOption = desc.add_options();
    addOption("help,h", "print help message.");
    addOption("input-file", bpo::value<Files::MaybeQuotedPathContainer>(), "input file");

    // Default option if none provided
    bpo::positional_options_description p;
    p.add("input-file", -1);

    bpo::variables_map variables;
    try
    {
        bpo::parsed_options valid_opts = bpo::command_line_parser(argc, argv).options(desc).positional(p).run();
        bpo::store(valid_opts, variables);
        bpo::notify(variables);
        if (variables.count("help"))
        {
            std::cout << desc << std::endl;
            return false;
        }
        if (variables.count("input-file"))
        {
            files = variables["input-file"].as<Files::MaybeQuotedPathContainer>();
            return true;
        }
    }
    catch (std::exception& e)
    {
        std::cout << "ERROR parsing arguments: " << e.what() << "\n\n" << desc << std::endl;
        return false;
    }

    std::cout << "No input files or directories specified!" << std::endl;
    std::cout << desc << std::endl;
    return false;
}

int main(int argc, char** argv)
{
    std::vector<Files::MaybeQuotedPath> files;
    if (!parseOptions(argc, argv, files))
        return 1;

    Nif::Reader::setLoadUnsupportedFiles(true);
    //     std::cout << "Reading Files" << std::endl;
    for (const auto& path : files)
    {
        try
        {
            if (isNIF(path))
            {
                // std::cout << "Decoding: " << name << std::endl;
                Nif::NIFFile file(path);
                Nif::Reader reader(file);
                reader.parse(Files::openConstrainedFileStream(path));
            }
            else if (isBSA(path))
            {
                //                 std::cout << "Reading BSA File: " << name << std::endl;
                readVFS(makeBsaArchive(path));
            }
            else if (std::filesystem::is_directory(path))
            {
                //                 std::cout << "Reading All Files in: " << name << std::endl;
                readVFS(std::make_unique<VFS::FileSystemArchive>(path), path);
            }
            else
            {
                std::cerr << "ERROR:  \"" << Files::pathToUnicodeString(path)
                          << "\" is not a nif file, bsa file, or directory!" << std::endl;
            }
        }
        catch (std::exception& e)
        {
            std::cerr << "ERROR, an exception has occurred:  " << e.what() << std::endl;
        }
    }
    return 0;
}
