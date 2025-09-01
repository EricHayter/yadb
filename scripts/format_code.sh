#!/bin/bash

# Script to format all C++ files using clang-format-19
# Formats files in src/, tests/, and include/ directories

# Check if clang-format-19 is available
if ! command -v clang-format-19 &> /dev/null; then
    echo "Error: clang-format-19 not found. Please install it first."
    exit 1
fi

# Check if .clang-format file exists
if [ ! -f ".clang-format" ]; then
    echo "Warning: .clang-format file not found in current directory."
    echo "clang-format will use default settings."
fi

# Define directories to search
DIRECTORIES=("src" "tests" "include")

echo "Formatting C++ files with clang-format-19..."
echo "Working directory: $(pwd)"

# Counter for formatted files
formatted_count=0

# Process each directory
for dir in "${DIRECTORIES[@]}"; do
    if [ -d "$dir" ]; then
        echo "Processing directory: $dir/"

        # Use a simpler approach - find all files then filter by extension
        mapfile -t files < <(find "$dir" -type f \( -name "*.cpp" -o -name "*.cxx" -o -name "*.cc" -o -name "*.c++" -o -name "*.hpp" -o -name "*.hxx" -o -name "*.hh" -o -name "*.h++" -o -name "*.h" -o -name "*.C" \) 2>/dev/null)

        echo "  Found ${#files[@]} C++ files"

        for file in "${files[@]}"; do
            echo "  Formatting: $file"

            # Test the clang-format command and capture its exit status
            if clang-format-19 -i -style=file "$file"; then
                ((formatted_count++))
                echo "    ✓ Success"
            else
                exit_code=$?
                echo "    ✗ Failed with exit code $exit_code"
                echo "    File: '$file'"
                echo "    File exists: $([ -f "$file" ] && echo "yes" || echo "no")"
                echo "    File readable: $([ -r "$file" ] && echo "yes" || echo "no")"
                echo "    File writable: $([ -w "$file" ] && echo "yes" || echo "no")"

                # Try to get more info about the failure
                echo "    Testing clang-format on this file:"
                clang-format-19 --dry-run -style=file "$file" || echo "    Dry run also failed"

                exit 1
            fi
        done
    else
        echo "Directory $dir/ not found, skipping..."
    fi
done

echo "Formatting complete! Processed $formatted_count files."
