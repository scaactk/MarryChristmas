#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h> // 各种位图数据结构

class Converter
{
public:
    Converter() : pixels_(NULL), width_(0), height_(0) {}
    ~Converter() { Clear(); }

    // w, h 指输出时字符画的宽高
    bool ConvImage(char const *image, char const *html, int w, int h)
    {
        // 1. 读取位图文件中的像素颜色信息
        if (!OpenBitmap(image))
            return false;
        // 2. 根据图片的像素颜色，转换字符，输出字符画
        FILE *file = fopen(html, "w");
        if (file) {
            WriteHead(file);
            WriteAscii(file, w, h);
            WriteFoot(file);

            fclose(file);
        }
        return (file != NULL);
    }

    bool ConvAnimation(char const *pattern, int sid, int eid,
                       char const *html, int w, int h)
    {
        FILE *file = fopen(html, "w");
        if (file) {
            WriteHead(file);
            WriteScript(file);

            char filename[MAX_PATH];
            for (int i = sid; i != eid; i++) {
                snprintf(filename, sizeof(filename), pattern, i);

                if (OpenBitmap(filename))
                    WriteAscii(file, w, h);
            }

            WriteFoot(file);
            fclose(file);
        }
        return (file != NULL);
    }

    void Clear()
    {
        if (pixels_)
            delete[] pixels_;
        pixels_ = NULL;
        width_  = 0;
        height_ = 0;
    }

private:
    bool OpenBitmap(char const *filename)
    {
        FILE *file = fopen(filename, "rb");
        if (file) {
            Clear();

            BITMAPFILEHEADER bf;
            BITMAPINFOHEADER bi;

            // 这里假设读取的都是24位未压缩的位图，演示用，不作任何出错处理:P
            fread(&bf, sizeof(bf), 1, file);
            fread(&bi, sizeof(bi), 1, file);

            // 好吧，还是稍微假设一下
            //assert(bi.biBitCount == 24);
            //assert(bi.biCompression == BI_RGB);

            // 下面开始正式读取～
            width_  = bi.biWidth;
            height_ = bi.biHeight;
            pixels_ = new RGBQUAD[width_ * height_];

            // 位图像素数据需要DWORD对齐，所以一行颜色的大小应该是这么算的
            uint32_t rowSize = (bi.biBitCount * width_ + 31) / 32 * 4;
            uint8_t *line = new uint8_t[rowSize];

            for (int y = 0; y < height_; y++) {
                fread(line, rowSize, 1, file);
                for (int x = 0; x < width_; x++) {
                    uint8_t *color = line + x * 3;  // 24Bit
                    RGBQUAD *pixel = &pixels_[(height_ - y - 1) * width_ + x];
                    pixel->rgbBlue  = color[0];
                    pixel->rgbGreen = color[1];
                    pixel->rgbRed   = color[2];
                }
            }

            delete[] line;

            fclose(file);
        }
        return (file != NULL);
    }

    void WriteHead(FILE *file) const
    {
        fprintf(file, "%s\n", "\
                <html>\
                <head>\
                <style type=\"text/css\">\
                pre { font-size:8px; line-height:0.8; }\
                </style>\
                </head>\
                <body>");
    }
    void WriteFoot(FILE *file) const
    {
        fprintf(file, "%s\n", "\
                </body>\
                </html>");
    }
    void WriteScript(FILE *file) const
    {
        fprintf(file, "%s\n", "\
                <script type=\"text/javascript\">\
                window.onload = function() {    \
                    var frames = document.getElementsByTagName('pre'); \
                    var length = frames.length; \
                    var current = 0;            \
                    \
                    var doframe = function() {  \
                        frames[current].style.display = 'block';                            \
                        frames[(current - 1 + length) % length].style.display = 'none';     \
                        current = (current + 1) % length;                                   \
                    };                          \
                    \
                    for (var i = 0; i < length; i++)        \
                        frames[i].style.display = 'none';   \
                    setInterval(doframe, 1000 / 8);         \
                };                              \
                </script>");
    }
    // 将图片转化为字符画，输出到一个 pre 标签中
    void WriteAscii(FILE *file, int w, int h) const
    {
        // 如果字符画大小不大于图片大小，会把 stepX stepY 个像素合并为一个字符
        // 这里又假设 w h 不会比 width_ height_ 大
        int stepX = width_  / w;
        int stepY = height_ / h;

        fprintf(file, "%s", "<pre>");
        for (int y = 0; y < height_; y += stepY) {
            for (int x = 0; x < width_; x += stepX) {
                RGBQUAD color = GetColor(x, y, stepX, stepY);
                fprintf(file, "%c", ColorToCharacter(color));
            }
            fprintf(file, "\n");
        }
        fprintf(file, "%s", "</pre>");
    }

    // 将 (x,y) 为左上角的 w*h 区域混合为一个颜色
    RGBQUAD GetColor(int x, int y, int w, int h) const
    {
        int r = 0, g = 0, b = 0;
        for (int i = 0; i < w; i++) {
            if (i + x >= width_) continue;
            for (int j = 0; j < h; j++) {
                if (j + y >= height_) continue;
                RGBQUAD const& color = pixels_[(y + j) * width_ + (x + i)];
                r += color.rgbRed;
                g += color.rgbGreen;
                b += color.rgbBlue;
            }
        }
        RGBQUAD result = {};
        result.rgbRed   = r / (w * h);
        result.rgbGreen = g / (w * h);
        result.rgbBlue  = b / (w * h);
        return result;
    }

    char ColorToCharacter(RGBQUAD const& color) const
    {
        int brightness = (color.rgbRed + color.rgbGreen + color.rgbBlue) / 3;

        // 这是输出字符画用的字符
        // 越靠前的字符对应越深的颜色（所以最后一个是空格，最亮，纯白） 
        static char const *characters = "M80V1;:*-. ";

        int count = strlen(characters);
        int span = 0xFF / count;    // 亮度值 [0,255]
        int cidx = brightness / span;
        if (cidx == count)
            cidx--;

        return characters[cidx];
    }

    Converter(Converter const&);
    Converter& operator=(Converter const&);

    int32_t width_;     // 图片宽度
    int32_t height_;    // 图片高度
    RGBQUAD *pixels_;   // 像素颜色数组
};

int main()
{
    Converter conv;
    //conv.ConvImage("a/a0001.bmp", "test2.htm", 300/2, 100);
    conv.ConvAnimation("JB/JB_%04d.bmp", 1, 1810, "index.html", 768/4, 432/4);
}

