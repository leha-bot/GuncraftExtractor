#include "../include/converters.h"


void convertXnb(const std::string& fileName)
{
    auto xnb = XNBUniquePtr(new XNB());
    std::string error = xnb->openRead(fileName);
    if (!error.empty()) {
        std::cout << error << std::endl;
        return;
    }

    if (xnb->readerType == 1) {
        std::cout << "It's a texture file, converting to png" << std::endl;
        XnbToPng(std::move(xnb));
    }
    else if (xnb->readerType == 2) {
        std::cout << "It's a sound effect file, converting to wav" << std::endl;
        XnbToWav(std::move(xnb));
    }
    else {
        std::cout << "This xnb file type isn't supported" << std::endl;
    }
}

void XnbToPng(XNBUniquePtr xnb)
{

    const unsigned int format = xnb->readInt();
    if (format != 0) {
        std::cout << "Unsupported surface format " << std::to_string(format) << std::endl;
        return;
    }

    const unsigned int width = xnb->readInt();
    const unsigned int height = xnb->readInt();

    std::cout << "Width: " << width << "; Height: " << height << std::endl;

    //const unsigned int mipCount = xnb->readInt();

    //cout << "Mip count: " << to_string(mipCount) << endl;

    const unsigned int size = xnb->readInt();
    if (size != width * height * 4) {
        std::cout << "Size " << size << " isn't correct, it should be " << width * height * 4 << std::endl;
        return;
    }

    const std::string filename = xnb->filename;
    const std::string filenamePng = filename.substr(0, filename.length() - 4) + ".png";

    char* imageData = xnb->readBytes(size);
    std::vector<unsigned char> image(imageData, imageData + size);

    // Was unused
    //const unsigned error = lodepng::encode(filenamePng, image, width, height);

    std::cout << "Successfull converted the xnb texture file " << filename << " to png file " << filenamePng << std::endl;
}

void XnbToWav(XNBUniquePtr xnb)
{
    if (xnb->readInt() != 18) {
        std::cout << "Wrong format chunk size" << std::endl;
        return;
    }

    if (xnb->readShort() != 1) {
        std::cout << "Wrong wav codec" << std::endl;
        return;
    }

    const unsigned short channels = xnb->readShort();
    const unsigned int samplesPerSec = xnb->readInt();
    const unsigned int avgBytesPerSec = xnb->readInt();
    const unsigned short blockAlign = xnb->readShort();
    const unsigned short bitsPerSample = xnb->readShort();

    if (avgBytesPerSec != (samplesPerSec * channels * (bitsPerSample / 8))) {
        std::cout << "Average bytes per second is incorrect" << std::endl;
        return;
    }

    if (blockAlign != (channels * (bitsPerSample / 8))) {
        std::cout << "Block align is incorrect" << std::endl;
        return;
    }

    xnb->movePointer(2);

    const unsigned int dataSize = xnb->readInt();
    char* waveData = xnb->readBytes(dataSize);

    const std::string filename = xnb->filename;
    const std::string filenameWav = filename.substr(0, filename.length() - 4) + ".wav";

    WAV wav;

    wav.format = 18;
    wav.channels = channels;
    wav.samplesPerSec = samplesPerSec;
    wav.avgBytesPerSec = avgBytesPerSec;
    wav.blockAlign = blockAlign;
    wav.bitsPerSample = bitsPerSample;
    wav.fmtChunkSize = 16;
    wav.dataChunkSize = dataSize;
    wav.waveData = waveData;

    wav.openWrite(filenameWav);

    std::cout << "Successfull converted the xnb soundeffect file " << filename << " to wav file" << filenameWav << std::endl;
}

void PngToXnb(const std::string& filename)
{
    const std::string filenameXnb = filename.substr(0, filename.length() - 4) + ".xnb";
    XNB *xnb = new XNB();
    xnb->readers.push_back(std::make_pair("Microsoft.Xna.Framework.Content.Texture2DReader, Microsoft.Xna.Framework.Graphics, Version=4.0.0.0, Culture=neutral, PublicKeyToken=842cf8be1de50553", 0));
    const std::string errorXnb = xnb->openWrite(filenameXnb);
    if (!errorXnb.empty()) {
        std::cout << errorXnb << std::endl;
        delete xnb;
        return;
    }

    std::vector<unsigned char> image;
    unsigned int width, height;
    unsigned int error = lodepng::decode(image, width, height, filename);
    if (error == 1) {
        std::cout << "Error opening png file" << std::endl;
        return;
    }

    xnb->writeInt(0); // Surface format = 0 (color)
    xnb->writeInt(width);
    xnb->writeInt(height);

    unsigned int size = width * height * 4;

    unsigned int mipCount = 12;
    if (size < 4194304) { // mipmapping breaks non block textures
        mipCount = 1;
    }
    else if (size == 4194304) {
        mipCount = 11;
    }
    xnb->writeInt(mipCount);

    unsigned char* imageBuffer = new unsigned char[size];
    unsigned char* shrinkedImage = new unsigned char[size];

    copy(image.begin(), image.end(), imageBuffer);

    for (unsigned int i = 0; i < mipCount; i++) {
        xnb->writeInt(size);
        xnb->writeBytes(reinterpret_cast<char*>(imageBuffer), size);

        if (i == mipCount - 1) // don't calculate a mipmap for the last one
            break;

        unsigned int newWidth = std::max(width / 2 + width % 2, 1U);
        unsigned int newHeight = std::max(height / 2 + height % 2, 1U);

        size = newWidth * newHeight * 4;

        unsigned int x2, y2, index, color1, color2, color3, color4;

        for (unsigned int y = 0; y < newHeight; y++) {
            y2 = y * 2;
            for (unsigned int x = 0; x < newWidth; x++) {
                x2 = x * 2;

                color1 = (y2*width*4) + (x2*4);
                color2 = (y2*width * 4) + ((x2+1) * 4);
                color3 = ((y2+1)*width * 4) + (x2 * 4);
                color4 = ((y2+1)*width * 4) + ((x2+1) * 4);

                index = (y*newWidth * 4) + (x * 4);

                shrinkedImage[index] = (imageBuffer[color1] + imageBuffer[color2] + imageBuffer[color3] + imageBuffer[color4]) / 4;
                shrinkedImage[index + 1] = (imageBuffer[color1 + 1] + imageBuffer[color2 + 1] + imageBuffer[color3 + 1] + imageBuffer[color4 + 1]) / 4;
                shrinkedImage[index + 2] = (imageBuffer[color1 + 2] + imageBuffer[color2 + 2] + imageBuffer[color3 + 2] + imageBuffer[color4 + 2]) / 4;
                shrinkedImage[index + 3] = (imageBuffer[color1 + 3] + imageBuffer[color2 + 3] + imageBuffer[color3 + 3] + imageBuffer[color4 + 3]) / 4;
            }
        }

        width = newWidth;
        height = newHeight;

        std::swap(imageBuffer, shrinkedImage);
    }

    delete xnb;

    std::cout << "Successfull converted the png file " << filename << " to xnb texture file" <<  filenameXnb << std::endl;
}

void WavToXnb(const std::string& filename)
{
    WAV* wav = new WAV();
    std::string error = wav->openRead(filename);
    if (!error.empty()) {
        std::cout << error << std::endl;
        delete wav;
        return;
    }

    std::string filenameXnb = filename.substr(0, filename.length() - 4) + ".xnb";
    XNB* xnb = new XNB();
    xnb->readers.push_back(std::make_pair("Microsoft.Xna.Framework.Content.SoundEffectReader", 0));
    error = xnb->openWrite(filenameXnb);
    if (!error.empty()) {
        std::cout << error << std::endl;
        delete wav;
        delete xnb;
        return;
    }

    xnb->writeInt(18);
    xnb->writeShort(1);
    xnb->writeShort(wav->channels);
    xnb->writeInt(wav->samplesPerSec);
    xnb->writeInt(wav->avgBytesPerSec);
    xnb->writeShort(wav->blockAlign);
    xnb->writeShort(wav->bitsPerSample);
    xnb->writeShort(0);

    xnb->writeInt(wav->dataChunkSize);
    xnb->writeBytes(wav->waveData, wav->dataChunkSize);

    xnb->writeInt(0);
    xnb->writeInt(wav->dataChunkSize/4);
    unsigned int duration = ((float)wav->dataChunkSize / (float)wav->avgBytesPerSec)*1000.0f;
    xnb->writeInt(duration);

    delete wav;
    delete xnb;

    std::cout << "Successfull converted the wav file " << filename << " to xnb soundeffect file" << filenameXnb << std::endl;
}
