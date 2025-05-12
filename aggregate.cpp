/// @author Johannes Nilsson

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include <cxxopts.hpp>

namespace fs = std::filesystem;

enum class ScoreFilter { Invalid, More, Less };

enum class DiffFlags : uint32_t {
  Invalid = 0x00,
  RGB     = 0x01,
  HSV     = 0x02,
  Mask    = 0x04,
};

constexpr DiffFlags operator|(DiffFlags f1, DiffFlags f2)
{
  return static_cast<DiffFlags>(static_cast<uint32_t>(f1) |
                                static_cast<uint32_t>(f2));
}

constexpr DiffFlags operator|=(DiffFlags& f1, const DiffFlags& f2)
{
  return f1 = f1 | f2;
}

constexpr uint32_t operator&(DiffFlags f1, DiffFlags f2)
{
  return static_cast<uint32_t>(f1) & static_cast<uint32_t>(f2);
}

int main(int argc, char* argv[])
{
  try {
    bool dry_run{ false };
    bool exclude_inputs{ false };
    // clang-format off
    cxxopts::Options options(
        "Aggregate",
        "Filters results created by png-compare based on similarity score");
    options.add_options()
      ("i,input", "Directory containing image comparison results",
       cxxopts::value<fs::path>())
      ("o,output", "Directory to store aggregate results in",
       cxxopts::value<fs::path>())
      ("s,score-filter", "Only include outputs with a score above/below "
       "'threshold'",
       cxxopts::value<std::string>()->default_value("less"))
      ("d,diff-flags", "Comma separated list of diff image types to include "
       "(Valid types: rgb,hsv,mask)",
       cxxopts::value<std::string>()->default_value("rgb,hsv,mask"))
      ("t,threshold", "Score threshold to compare against",
       cxxopts::value<double>()->default_value("100.0"))
      ("exclude-inputs", "Excludes source input images (only computed diff "
       "images are included in result",
       cxxopts::value<bool>(exclude_inputs)->default_value("false"))
      ("dry-run", "Print copy actions without actually copying",
       cxxopts::value<bool>(dry_run)->default_value("false"))
      ("h,help", "Print help");
    // clang-format on

    auto result = options.parse(argc, argv);

    if (result.count("help") || !result.count("input") ||
        !result.count("output")) {
      std::cout << options.help() << "\n";
      return 0;
    }

    const fs::path input_dir{ result["input"].as<fs::path>() };
    const fs::path output_dir{ result["output"].as<fs::path>() };
    const double   threshold{ result["threshold"].as<double>() };

    // Parse score filter
    const ScoreFilter score_filter{ [&]() -> ScoreFilter {
      const auto      str = result["score-filter"].as<std::string>();
      if (str.compare("less") == 0) {
        return ScoreFilter::Less;
      }
      else if (str.compare("more") == 0) {
        return ScoreFilter::More;
      }
      return ScoreFilter::Invalid;
    }() };

    // Parse diff flags
    const DiffFlags diff_flags{ [&]() -> DiffFlags {
      const auto    str = result["diff-flags"].as<std::string>();
      DiffFlags     flags{};
      std::stringstream ss(str);
      std::string   token;

      while (std::getline(ss, token, ',')) {
        if (token.compare("hsv") == 0) {
          flags |= DiffFlags::HSV;
        }
        else if (token.compare("rgb") == 0) {
          flags |= DiffFlags::RGB;
        }
        else if (token.compare("mask") == 0) {
          flags |= DiffFlags::Mask;
        }
        else {
          std::cerr << "Invalid diff flag option " << token << "\n";
        }
      }
      return flags == DiffFlags::Invalid
               ? DiffFlags::RGB | DiffFlags::HSV | DiffFlags::Mask
               : static_cast<DiffFlags>(flags);
    }() };

    // Check inputs
    if (not fs::is_directory(input_dir)) {
      std::cerr << "Invalid directory: " << input_dir << "\n";
      return 1;
    }
    if (score_filter == ScoreFilter::Invalid ||
        diff_flags == DiffFlags::Invalid) {
      std::cerr << "Invalid filter type\n";
      std::cout << options.help() << "\n";
      return 1;
    }
    // Aggregation
    double score{ 0.0 };
    // map directory to what files to be copied
    std::unordered_map<fs::path, std::vector<std::string>> filtered_entries;
    for (const auto& entry : fs::directory_iterator(input_dir)) {
      if (entry.is_directory()) {
        const fs::path info_file = entry.path() / "info.txt";
        if (fs::is_regular_file(info_file)) {
          std::ifstream file(info_file);
          if (not file.is_open()) {
            std::cerr << "Failed to open file " << info_file << "\n";
            continue;
          }
          std::string name1, name2;
          file >> std::quoted(name1) >> std::quoted(name2) >> score;
          if (file.fail()) {
            std::cerr << "Failed to read file " << info_file << "\n";
            continue;
          }
          if ((score_filter == ScoreFilter::Less && score <= threshold) ||
              (score_filter == ScoreFilter::More && score >= threshold)) {
            // Always add score file
            auto& file_list{ filtered_entries[entry.path()] };
            file_list.push_back("info.txt");

            // Add diff files
            if (diff_flags & DiffFlags::RGB) {
              file_list.push_back("absdiff_rgb.png");
            }
            if (diff_flags & DiffFlags::HSV) {
              file_list.push_back("absdiff_hsv.png");
            }
            if (diff_flags & DiffFlags::Mask) {
              file_list.push_back("threshold_mask.png");
            }

            // Add inputs if they are wanted
            if (not exclude_inputs) {
              file_list.push_back(name1);
              file_list.push_back(name2);
            }
          }
        }
        else {
          std::cerr << "Couldn't find " << info_file << "\n";
        }
      } // else - not a directory, ignore
    }
    // Store all entries in output folder, replacing any existing files
    const auto copy_options =
      fs::copy_options::update_existing | fs::copy_options::recursive;
    for (const auto& kv : filtered_entries) {
      const auto result_dir = output_dir / kv.first.filename();
      if (not fs::is_directory(result_dir)) {
        if (not dry_run) {
          if (not fs::create_directories(result_dir)) {
            std::cerr << "Failed to create output directory!\n";
            return 1;
          }
        }
        else {
          std::cout << "Create directory " << result_dir << "\n";
        }
      }
      for (const auto& filename : kv.second) {
        const auto source = kv.first / filename;
        const auto target = result_dir / filename;
        if (not dry_run) {
          fs::copy(source, target, copy_options);
        }
        else {
          std::cout << "Copy " << source << " to " << target << "\n";
        }
      }
      // Write the command used to command.txt
      if (not dry_run) {
        const fs::path command_file{ output_dir / "command.txt" };
        std::ofstream  out(command_file);
        out << "Command used: ";
        for (int i{ 0 }; i < argc; ++i) {
          out << argv[i] << " ";
        }
        out.close();
      }
    }
  }
  catch (const cxxopts::exceptions::exception& e) {
    std::cerr << "Error parsing options: " << e.what() << "\n";
    return 1;
  }
  return 0;
}
