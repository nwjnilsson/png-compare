/// @author Johannes Nilsson

#include <filesystem>
#include <fstream>
#include <iostream>

#include <opencv2/imgproc/types_c.h>
#include <opencv2/opencv.hpp>

namespace fs = std::filesystem;

struct ResultData {
  std::string    name1;
  std::string    name2;
  const cv::Mat* img1;
  const cv::Mat* img2;
  double         score;
};

/// Compute the structural similarity index of a single channel images. OpenCV
/// makes it easy to do this on the GPU as well but since this was meant to be
/// part of an automation test pipeline, CUDA might not be an option anyway.
///                 (2*mu_x*mu_y + c1)(2*sigma_xy + c2)
///  SSIM(x,y) =   --------------------------------------
///            (mu_x^2 + mu_y^2 + c1)(sigma_x^2 + sigma_y^2 + c2)
///
/// Comparing a black and white image may not result in exactly 0.00000%
/// similarity due to the numerical instability introduced by the constants and
/// the number format conversions
double compute_ssim_y(const cv::Mat& img1, const cv::Mat& img2)
{
  if (img1.channels() != 1 || img2.channels() != 1) {
    std::cerr << "compute_ssim_y(): inputs should only have one channel\n";
    return -1.0;
  }

  // The purpose of these constants is to stabilize division with otherwise weak
  // denominators. C1 = (K1*L)^2, C2 = (K2*L)^2, where K1 and K2 are 0.01 and
  // 0.03 respectively, and L is the dynamic range of the pixel values. I'm
  // limiting myself to a bit depth of 8 by hardcoding these values here. These
  // are the constants suggested by research:
  // https://live.ece.utexas.edu/publications/2021/Hitchiker_SSIM_Access.pdf
  constexpr double c1{ 6.5025 }, c2{ 58.5225 };
  constexpr double stddev{ 1.5 };
  const cv::Size   filter{ 11, 11 };

  cv::Mat x, y;
  img1.convertTo(x, CV_32F);
  img2.convertTo(y, CV_32F);

  cv::Mat y_2{ y.mul(y) }; // y^2
  cv::Mat x_2{ x.mul(x) }; // x^2
  cv::Mat x_y{ x.mul(y) }; // x * y

  cv::Mat mu_x, mu_y;
  // Compute mean. This filter size and standard deviation is also recommended
  // in research (paper link above)
  cv::GaussianBlur(x, mu_x, filter, stddev);
  cv::GaussianBlur(y, mu_y, filter, stddev);

  // Compute mean squared
  cv::Mat mu_x_2{ mu_x.mul(mu_x) };
  cv::Mat mu_y_2{ mu_y.mul(mu_y) };
  cv::Mat mu_x_mu_y{ mu_x.mul(mu_y) };

  // Compute sample variance
  cv::Mat sigma_x_2, sigma_y_2, sigma_xy;
  cv::GaussianBlur(x_2, sigma_x_2, filter, stddev);
  cv::GaussianBlur(y_2, sigma_y_2, filter, stddev);
  cv::GaussianBlur(x_y, sigma_xy, filter, stddev);
  sigma_x_2 -= mu_x_2;
  sigma_y_2 -= mu_y_2;
  sigma_xy -= mu_x_mu_y;

  // Compute nominator and denominator
  cv::Mat t1{ 2 * mu_x_mu_y + c1 };
  cv::Mat t2{ 2 * sigma_xy + c2 };
  cv::Mat t3{ t1.mul(t2) };

  t1 = mu_x_2 + mu_y_2 + c1;
  t2 = sigma_x_2 + sigma_y_2 + c2;
  t1 = t1.mul(t2);

  cv::Mat ssim_map;
  divide(t3, t1, ssim_map);

  return cv::mean(ssim_map)[0];
}

/// Compute the SSIM for images of 1-4 channels and return the average score
double compute_ssim_rgba(const cv::Mat& img1, const cv::Mat& img2)
{
  if (img1.size != img2.size) {
    std::cerr << "compute_ssim(): inputs should be of same size\n";
    return -1.0;
  }
  if (img1.channels() != img2.channels()) {
    std::cerr << "compute_ssim(): inputs should have same number of channels\n";
    return -1.0;
  }

  // Split channels into separate r, g and b parts
  const int            channel_count{ img1.channels() };
  std::vector<cv::Mat> channels1(channel_count);
  std::vector<cv::Mat> channels2(channel_count);
  cv::split(img1, channels1);
  cv::split(img2, channels2);

  // Compute SSIM for each channel and return average
  double ssim_total{ 0.0 };
  for (size_t i{ 0 }; i < channel_count; ++i) {
    ssim_total += compute_ssim_y(channels1[i], channels2[i]);
  }
  return ssim_total / channel_count;
}

/// Computes the absolute difference of the rgb channels and creates a mask to
/// illustrate segments/pixels with intense noise
void compute_diff_and_store_result(const ResultData& data,
                                   const fs::path&   output_dir)
{
  // HSV color space seems to work better when creating the mask
  cv::Mat img1_hsv, img2_hsv, absdiff_rgb, absdiff_hsv;
  cv::cvtColor(*data.img1, img1_hsv, CV_BGR2HSV);
  cv::cvtColor(*data.img2, img2_hsv, CV_BGR2HSV);
  cv::absdiff(*data.img1, *data.img2, absdiff_rgb);
  cv::absdiff(img1_hsv, img2_hsv, absdiff_hsv);

  cv::Mat mask{ cv::Mat::zeros(absdiff_hsv.rows, absdiff_hsv.cols, CV_8UC1) };

  // Attempt to make segments where the difference is big
  constexpr float threshold{ 25.0f };
  // I would have put a 'pragma omp parallel for' here but I'm not in the mood
  // for another dependency and the rest of the computations are pretty slow
  // anyway
  for (int i = 0; i < absdiff_hsv.rows; ++i) {
    for (int j = 0; j < absdiff_hsv.cols; ++j) {
      cv::Vec3b pix{ absdiff_hsv.at<cv::Vec3b>(i, j) };
      float     dist{ static_cast<float>(pix[0] * pix[0] + pix[1] * pix[1] +
                                     pix[2] * pix[2]) };
      dist = sqrt(dist);
      if (dist > threshold) {
        mask.at<unsigned char>(i, j) = 255;
      }
    }
  }

  // Write results. I'm making certain assumptions about the filenames here.
  // Generate a fingerprint based on image names. Overwrite previous result if
  // image combination already exists.
  const fs::path result_dir{ output_dir / (data.name1 + "-" + data.name2) };

  if (std::filesystem::is_directory(result_dir)) {
    std::cerr << "Overwriting previous result...\n";
    fs::remove_all(result_dir);
  }
  fs::create_directory(result_dir);

  // Write images
  const fs::path file1{ result_dir / std::string{ data.name1 + "_rgb.png" } };
  const fs::path file2{ result_dir / std::string{ data.name2 + "_rgb.png" } };
  cv::imwrite(file1.string(), *data.img1);
  cv::imwrite(file2.string(), *data.img2);
  cv::imwrite((result_dir / "absdiff_rgb.png").string(), absdiff_rgb);
  cv::imwrite((result_dir / "absdiff_hsv.png").string(), absdiff_hsv);
  cv::imwrite((result_dir / "threshold_mask.png").string(), mask);

  // Write score to score.txt
  const fs::path info_file{ result_dir / "info.txt" };
  std::ofstream  out(info_file);
  out << file1.filename() << " " << file2.filename() << " " << data.score
      << "\n";
  out.close();
}

int main(int argc, char* argv[])
{
  if (argc != 4) {
    std::cerr << "Usage: png-compare <image1.png> <image2.png> <output_dir>\n";
    return 1;
  }

  const fs::path output_dir{ argv[3] };
  if (!std::filesystem::is_directory(output_dir)) {
    std::cout << "Creating directory " << output_dir << "\n";
    if (not fs::create_directory(output_dir)) {
      std::cerr << "Failed to create output directory!\n";
      return 1;
    }
  }

  // Silence some OpenCV info stuff
  cv::utils::logging::setLogLevel(
    cv::utils::logging::LogLevel::LOG_LEVEL_WARNING);

  const cv::Mat img1{ cv::imread(argv[1]) };
  const cv::Mat img2{ cv::imread(argv[2]) };
  std::cout << "Computing SSIM...\n";
  const double     score{ 100.0 * compute_ssim_rgba(img1, img2) };
  const ResultData data{
    .name1 = fs::path(argv[1]).stem().string(), // ../a/foo.txt->foo
    .name2 = fs::path(argv[2]).stem().string(),
    .img1  = &img1,
    .img2  = &img2,
    .score = score
  };

  std::cout << "Computing deltas...\n";
  compute_diff_and_store_result(data, output_dir);
  std::cout << "Done.\n";
  std::cout << "Similarity: " << std::fixed << std::setprecision(2) << score
            << "\n";
  return 0;
}
