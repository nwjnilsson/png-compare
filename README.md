# png-compare
PNG Comparison tool that utilizes OpenCV to compute the Structural Similarity
Index Metric on the CPU. The CMake project builds and links libraries statically
since this was part of the criteria for the interview assignment.
## Usage
png-compare tool:
```
png-compare <image1.png> <image2.png> <output_dir>
```
aggregate tool:
```
Filters results created by png-compare based on similarity score
Usage:
  Aggregate [OPTION...]

  -i, --input arg         Directory containing image comparison results
  -o, --output arg        Directory to store aggregate results in
  -s, --score-filter arg  Only include outputs with a score above/below 
                          'threshold' (default: less)
  -d, --diff-flags arg    Comma separated list of diff image types to 
                          include (Valid types: rgb,hsv,mask) (default: 
                          rgb,hsv,mask)
  -t, --threshold arg     Score threshold to compare against (default: 
                          100.0)
      --exclude-inputs    Excludes source input images (only computed diff 
                          images are included in result
      --dry-run           Print copy actions without actually copying
  -h, --help              Print help
```
