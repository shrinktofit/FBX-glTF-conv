
#include "ReadCliArgs.h"
#include <array>
#include <bee/Converter.h>
#include <bee/polyfills/filesystem.h>
#include <fstream>
#include <iostream>
#include <numeric>
#include <optional>
#include <string>

std::u8string relativeUriBetweenPath(const bee::filesystem::path& from_,
  const bee::filesystem::path& to_) {
  return to_.lexically_relative(from_).generic_u8string();
}

int main(int argc_, char* argv_[]) {
  namespace fs = bee::filesystem;

  auto cliOptions = beecli::readCliArgs(argc_, argv_);
  if (!cliOptions) {
    return -1;
  }

  if (!cliOptions->fbmDir.empty()) {
    cliOptions->convertOptions.fbmDir = cliOptions->fbmDir;
  }

  if (cliOptions->outFile.empty()) {
    const auto inputFilePath = fs::path{ cliOptions->inputFile };
    const auto inputBaseNameNoExt = inputFilePath.stem().string();
    auto outFilePath = fs::current_path() / (inputBaseNameNoExt + "_glTF") /
      (inputBaseNameNoExt + ".gltf");
    fs::create_directories(outFilePath.parent_path());
    cliOptions->outFile = outFilePath.u8string();
  }
  cliOptions->convertOptions.out = cliOptions->outFile;

  class MyWriter : public bee::GLTFWriter {
  public:
    MyWriter(std::u8string_view in_file_, std::u8string_view out_file_)
      : _inFile(in_file_), _outFile(out_file_) {
    }

    virtual std::optional<std::u8string> buffer(const std::byte* data_,
      std::size_t size_,
      std::uint32_t index_,
      bool multi_) {
      const auto outFilePath = fs::path{ _outFile };
      const auto glTFOutBaseName = outFilePath.stem();
      const auto glTFOutDir = outFilePath.parent_path();
      const auto bufferOutPath =
        glTFOutDir /
        (multi_ ? (glTFOutBaseName.string() + std::to_string(index_) + ".bin")
          : (glTFOutBaseName.string() + ".bin"));
      std::error_code errc;
      fs::create_directories(bufferOutPath.parent_path(), errc);
      if (errc) {
        throw std::runtime_error("Failed to create directories for buffer " +
          bufferOutPath.string());
      }

      std::ofstream ofs(bufferOutPath.string(), std::ios::binary);
      ofs.exceptions(std::ios::badbit | std::ios::failbit);
      ofs.write(reinterpret_cast<const char*>(data_), size_);
      ofs.flush();

      return relativeUriBetweenPath(glTFOutDir, bufferOutPath);
    }

  private:
    std::u8string_view _inFile;
    std::u8string_view _outFile;
    bool _dataUriForBuffers;
  };

  MyWriter writer{ cliOptions->inputFile, cliOptions->outFile };
  cliOptions->convertOptions.useDataUriForBuffers = false;
  cliOptions->convertOptions.writer = &writer;

  const auto imageSearchDepth = 2;
  const std::array<const char*, 4> searchDirName = { "texture", "textures",
                                                     "material", "materials" };
  auto searchParentDir = fs::path(cliOptions->inputFile).parent_path();
  for (int i = 0; i < imageSearchDepth && !searchParentDir.empty();
    ++i, searchParentDir = searchParentDir.parent_path()) {
    cliOptions->convertOptions.textureSearch.locations.push_back(
      searchParentDir.u8string());
    for (auto& name : searchDirName) {
      cliOptions->convertOptions.textureSearch.locations.push_back(
        (searchParentDir / name).u8string());
    }
  }

  cliOptions->convertOptions.pathMode = bee::ConvertOptions::PathMode::copy;

  try {
    auto glTFJson =
      bee::convert(cliOptions->inputFile, cliOptions->convertOptions);

    const auto outFilePath = fs::path{ cliOptions->outFile };
    std::ofstream glTFJsonOStream(outFilePath.string());
    glTFJsonOStream.exceptions(std::ios::badbit | std::ios::failbit);
    glTFJsonOStream << glTFJson;
    glTFJsonOStream.flush();
  }
  catch (const std::exception& exception) {
    std::cerr << exception.what() << "\n";
  }

  return 0;
}