#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>

#define STB_IMAGE_IMPLEMENTATION
#include "vendor/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "vendor/stb_image_write.h"

namespace fs = std::filesystem;

struct color {
    int red, green, blue;
    color() : red(0), green(0), blue(0) { }
    color(int r, int g, int b) : red(r), green(g), blue(b) { }

    friend std::ostream& operator<<(std::ostream& os, const color& cl);
};

std::ostream& operator<<(std::ostream& os, color& cl) {
    os << cl.red << ", " << cl.green << ", " << cl.blue;
    return os;
}

color& operator/=(color& cl, int val) {
    cl.red /= val;
    cl.green /= val;
    cl.blue /= val;
    return cl;
}

int main(int argc, char* argv[]) {
    if(argc < 3) {
        std::cout << "Too few arguments" << std::endl;
        std::cout << "Usage: imageMosaic pathToMainImage pathToImageFolder [output quality (1 - 100)]" << std::endl;
        return -1;
    }

    std::string mainImagePath = argv[1];
    std::string imageFolderPath = argv[2];

    int outputQuality = 100;
    if(argc >= 4) outputQuality = std::stoi(argv[3]);
    if(outputQuality < 1 || outputQuality > 100) {
        std::cout << "Output quality " << argv[3] << " is not valid, value must be within 1 - 100" << std::endl;
        return -1;
    }

    std::vector<color> colors;
    std::vector<unsigned char*> images;
    int smallImageWidth = -1, smallImageHeight = -1;

    //loop through image folder
    int sampleImageCounter = 0;
    for(const auto& entry : fs::directory_iterator(imageFolderPath)) {
        std::string imagePath = entry.path();
        int imageWidth, imageHeight, imageChannels;
        unsigned char* data = stbi_load(imagePath.c_str(), &imageWidth, &imageHeight, &imageChannels, 0);
        if(data == nullptr) {
            std::cout << "Could not load image " << imagePath << std::endl;
            return -1;
        }

        color avgColor;
        for(int i = 0; i + 2 < imageWidth * imageHeight * 3; i += 3) {
            avgColor.red += data[i];
            avgColor.green += data[i+1];
            avgColor.blue += data[i+2];
        }

        avgColor /= imageWidth * imageHeight;

        colors.push_back(avgColor);
        images.push_back(data);

        if(smallImageHeight == -1 || smallImageWidth == -1) {
            smallImageWidth = imageWidth;
            smallImageHeight = imageHeight;
        }
        else if(smallImageWidth != imageWidth || smallImageHeight != imageHeight) {
            std::cout << "All images must have the same resolution" << std::endl;
            return -1;
        }

        ++sampleImageCounter;
    }

    std::cout << "Finished preparing " << sampleImageCounter << " sample images" << std::endl;

    //Load main image
    int imageWidth, imageHeight, imageChannels;
    unsigned char* data = stbi_load(mainImagePath.c_str(), &imageWidth, &imageHeight, &imageChannels, 0);
    if(data == nullptr) {
        std::cout << "Could not load image " << mainImagePath << std::endl;
        return -1;
    }

    //Calculate best sample image per pixel
    std::vector<int> smallestDiffIndexes;

    int imageSize = imageWidth * imageHeight * 3;
    for(int i = 0, outPixel = 0; i + 2 < imageSize; i += 3, ++outPixel) {
        color pixel((int)data[i], (int)data[i+1], (int)data[i+2]);

        int smallestDiff = -1, smallestDiffIndex = 0;
        for(int j = 0; j < colors.size(); ++j) {
            int diff = 0;
            diff += std::abs(pixel.red - colors[j].red);
            diff += std::abs(pixel.green - colors[j].green);
            diff += std::abs(pixel.blue - colors[j].blue);

            if(diff < smallestDiff || smallestDiff == -1) {
                smallestDiff = diff;
                smallestDiffIndex = j;
            }
        }

        smallestDiffIndexes.push_back(smallestDiffIndex);
    }
    stbi_image_free(data);

    //Render output image
    int outImageWidth = smallImageWidth * imageWidth, outImageHeight = smallImageHeight * imageHeight;
    unsigned char* outputData = new unsigned char[outImageWidth * outImageHeight * 3];

    int pixels = imageWidth * imageHeight;
    for(int i = 0; i < pixels; ++i) {
        int x = i % imageWidth;
        int y = i / imageWidth;

        int sampleIndex = smallestDiffIndexes[i];
        unsigned char* sampleData = images[sampleIndex];

        for(int j = 0; j < smallImageHeight; ++j) {
            int outY = y * smallImageHeight + j;
            for(int k = 0; k < smallImageWidth; ++k) {
                int outX = x * smallImageWidth + k;

                int outDataPixelIndex = (outX + outY * outImageWidth) * 3;
                int sampleDataPixelIndex = (k + j * smallImageWidth) * 3;
                outputData[outDataPixelIndex] = sampleData[sampleDataPixelIndex];
                outputData[outDataPixelIndex + 1] = sampleData[sampleDataPixelIndex + 1];
                outputData[outDataPixelIndex + 2] = sampleData[sampleDataPixelIndex + 2];
            }
        }
    }

    int result = stbi_write_jpg("result.jpg", outImageWidth, outImageHeight, 3, outputData, outputQuality);
    if(result == 0) std::cout << "Could not write output image" << std::endl;
    else std::cout << "Done" << std::endl;

    for(unsigned char* image : images) {
        stbi_image_free(image);
    }

    delete outputData;
}