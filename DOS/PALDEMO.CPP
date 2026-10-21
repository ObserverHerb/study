// visual layout of the VGA color palette
// demonstrating why they were chosen
// 24 hues
// 3 levels of darkness
// 3 levels of desaturation for each level of darkness

#include <dpmi.h>
#include <go32.h>
#include <conio.h>
#include <array>
#include <iostream>

constexpr int WIDTH=320;
constexpr int HEIGHT=200;
const int INT_10H=0x10;

class VideoMode
{
public:
    VideoMode(unsigned short mode)
    {
        __dpmi_regs regs={
            .x={
            .ax=mode
            }
        };
        __dpmi_int(INT_10H,&regs);
    }

    ~VideoMode()
    {
        __dpmi_regs regs={
            .x={
            .ax=0x0003 // mode 3 = 80x25 text
            }
        };
        __dpmi_int(INT_10H,&regs);
    }
};

enum class ColorName: unsigned char
{
   BLACK,
   BLUE,
   GREEN,
   CYAN,
   RED,
   MAGENTA,
   BROWN,
   WHITE,
   GRAY,
   LIGHT_BLUE,
   LIGHT_GREEN,
   LIGHT_CYAN,
   LIGHT_RED,
   LIGHT_MAGENTA,
   YELLOW,
   BRIGHT_WHITE,
   GRAY00,
   GRAY01,
   GRAY02,
   GRAY03,
   GRAY04,
   GRAY05,
   GRAY06,
   GRAY07,
   GRAY08,
   GRAY09,
   GRAY10,
   GRAY11,
   GRAY12,
   GRAY13,
   GRAY14,
   GRAY15
};

enum class ASCIIKey
{
    ESCAPE=27,
    ARROW_UP=72,
    ARROW_DOWN=80,
    ARROW_LEFT=75,
    ARROW_RIGHT=77
};

constexpr int BLOCK_SIZE=6;
constexpr int BORDER_SIZE=8;
constexpr int PADDING=5;
constexpr int ROW_COUNT=9;
constexpr int HUE_COUNT=24;
constexpr int COLOR_COUNT=216;

class Color
{
public:
    Color(): hue(0), darkness(0), desaturation(0) { }

    unsigned char Index() const
    {
        return 32+hue+darkness*72+desaturation*24;
    }

    int Desaturation() const
    {
        return desaturation;
    }

    int Darkness() const
    {
        return darkness;
    }

    int Hue() const
    {
        return hue;
    }

    void Darken()
    {
        darkness=++darkness%3;
    }

    void Lighten()
    {
        if (--darkness < 0) darkness=2;
    }

    void Saturate()
    {
        if (--desaturation < 0)
        {
            desaturation=2;
            Lighten();
        }
    }

    void Desaturate()
    {
        if (++desaturation > 2)
        {
            desaturation=0;
            Darken();
        }
    }

    void NextHue()
    {
        hue=++hue%HUE_COUNT;
    }

    void PreviousHue()
    {
        if (--hue < 0) hue=HUE_COUNT-1;
    }

protected:
    int hue;
    int darkness;
    int desaturation;
};

int main()
{
    VideoMode mode13h(0x0013); // AH=0, AL=13h -> set mode 13h

    unsigned char buffer[WIDTH*HEIGHT];
    Color color;
    int key=-1;
    while (key != static_cast<int>(ASCIIKey::ESCAPE))
    {
        // background
        for (int y=0; y < HEIGHT; y++)
        {
            for (int x=0; x < WIDTH; x++)
            {
                buffer[y*WIDTH+x]=color.Index();
            }
        }

        int pixelY=HEIGHT-BORDER_SIZE*ROW_COUNT-PADDING*ROW_COUNT;
        int cursorPixelY=pixelY+BORDER_SIZE*color.Desaturation()+PADDING*color.Desaturation();
        cursorPixelY+=BORDER_SIZE*3*color.Darkness()+PADDING*3*color.Darkness();
        int pixelX=PADDING;
        // selection
        for (int y=cursorPixelY; y < cursorPixelY+BORDER_SIZE; y++)
        {
            int startX=pixelX+BORDER_SIZE*color.Hue()+PADDING*color.Hue();
            for (int x=startX; x < startX+BORDER_SIZE; x++)
            {
                buffer[y*WIDTH+x]=static_cast<unsigned char>(ColorName::BRIGHT_WHITE);
            }
        }

        // UI
        pixelY+=(BORDER_SIZE-BLOCK_SIZE)/2;
        pixelX+=(BORDER_SIZE-BLOCK_SIZE)/2;
        Color gridColor;
        for (int blockY=0; blockY < ROW_COUNT; blockY++)
        {
            int startY=pixelY+BLOCK_SIZE*blockY+(PADDING+BORDER_SIZE-BLOCK_SIZE)*blockY;
            for (int blockX=0; blockX < HUE_COUNT; blockX++)
            {
                int startX=pixelX+BLOCK_SIZE*blockX+(PADDING+BORDER_SIZE-BLOCK_SIZE)*blockX;
                for (int y=startY; y < startY+BLOCK_SIZE; y++)
                {
                    for (int x=startX; x < startX+BLOCK_SIZE; x++)
                    {
                        buffer[y*WIDTH+x]=gridColor.Index();
                    }
                }

                gridColor.NextHue();
            }
            gridColor.Desaturate();
        }

        dosmemput(buffer,WIDTH*HEIGHT,0xA0000UL);
        key=getch();

        if (key == 0)
        {
            switch (getch())
            {
            case static_cast<int>(ASCIIKey::ARROW_UP):
                color.Saturate();
                break;
            case static_cast<int>(ASCIIKey::ARROW_DOWN):
                color.Desaturate();
                break;
            case static_cast<int>(ASCIIKey::ARROW_LEFT):
                color.PreviousHue();
                break;
            case static_cast<int>(ASCIIKey::ARROW_RIGHT):
                color.NextHue();
                break;
            }
        }
    }

    return 0;
}
