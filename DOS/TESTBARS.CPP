#include <dpmi.h>
#include <go32.h>
#include <array>
#include <iostream>

constexpr int WIDTH=320;
constexpr int HEIGHT=200;
const int INT_10H=0x10;
constexpr float ONE_THIRD=1.0f/3.0f;
constexpr float TWO_THIRDS=ONE_THIRD*2;
constexpr float ONE_QUARTER=1.0f/4.0f;
constexpr float THREE_QUARTERS=ONE_QUARTER*3;
constexpr unsigned char DESATURATE=24;
constexpr unsigned char DARKEN=72;

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

const std::array<unsigned char,7> LONG_BARS={
   static_cast<unsigned char>(ColorName::WHITE),
   static_cast<unsigned char>(ColorName::YELLOW),
   static_cast<unsigned char>(ColorName::CYAN),
   static_cast<unsigned char>(ColorName::GREEN),
   static_cast<unsigned char>(ColorName::MAGENTA),
   static_cast<unsigned char>(ColorName::RED),
   static_cast<unsigned char>(ColorName::BLUE)
};

const std::array<unsigned char,7> SHORT_BAR_STUBS={
   static_cast<unsigned char>(ColorName::BLUE),
   static_cast<unsigned char>(ColorName::GRAY01),
   static_cast<unsigned char>(ColorName::MAGENTA),
   static_cast<unsigned char>(ColorName::GRAY01),
   static_cast<unsigned char>(ColorName::CYAN),
   static_cast<unsigned char>(ColorName::GRAY01),
   static_cast<unsigned char>(ColorName::WHITE)
};

const std::array<unsigned char,4> SHORT_BARS_LEFT={
   104+DARKEN,
   static_cast<unsigned char>(ColorName::BRIGHT_WHITE),
   106+DARKEN,
   static_cast<unsigned char>(ColorName::GRAY01)
};

int main()
{
   VideoMode mode13h(0x0013); // AH=0, AL=13h -> set mode 13h

   unsigned char buffer[WIDTH*HEIGHT];
   constexpr float LONG_BAR_COLUMN_WIDTH=static_cast<float>(WIDTH)/LONG_BARS.size();
   constexpr int LONG_BAR_HEIGHT=static_cast<int>(static_cast<float>(HEIGHT)*TWO_THIRDS);
   for (int y=0; y < LONG_BAR_HEIGHT; y++)
   {
      for (int x=0; x < WIDTH; x++)
      {
         buffer[y*WIDTH+x]=LONG_BARS[static_cast<int>(static_cast<float>(x)/LONG_BAR_COLUMN_WIDTH)];
      }
   }
   const int SHORT_BAR_STUB_HEIGHT=static_cast<int>(static_cast<float>(HEIGHT)*ONE_THIRD*ONE_QUARTER);
   for (int y=LONG_BAR_HEIGHT; y < LONG_BAR_HEIGHT+SHORT_BAR_STUB_HEIGHT; y++)
   {
      for (int x=0; x < WIDTH; x++)
      {
         buffer[y*WIDTH+x]=SHORT_BAR_STUBS[static_cast<int>(static_cast<float>(x)/LONG_BAR_COLUMN_WIDTH)];
      }
   }
   const int SHORT_BARS_LEFT_HEIGHT=static_cast<int>(static_cast<float>(HEIGHT)*ONE_THIRD*THREE_QUARTERS);
   constexpr float SHORT_BARS_SECTION_WIDTH=LONG_BAR_COLUMN_WIDTH*5;
   const float SHORT_BARS_COLUMN_WIDTH=static_cast<float>(SHORT_BARS_SECTION_WIDTH)/SHORT_BARS_LEFT.size();
   constexpr float COLUMN_WIDTH_THIRD=LONG_BAR_COLUMN_WIDTH/3.0f;
   for (int y=LONG_BAR_HEIGHT+SHORT_BAR_STUB_HEIGHT; y < HEIGHT; y++)
   {
      for (int x=0; x < SHORT_BARS_SECTION_WIDTH; x++)
      {
         buffer[y*WIDTH+x]=SHORT_BARS_LEFT[static_cast<int>(static_cast<float>(x)/SHORT_BARS_COLUMN_WIDTH)];
      }
      for (int x=SHORT_BARS_SECTION_WIDTH; x < SHORT_BARS_SECTION_WIDTH+LONG_BAR_COLUMN_WIDTH; x++)
      {
         buffer[y*WIDTH+x]=static_cast<unsigned char>(ColorName::GRAY00)+static_cast<unsigned char>(static_cast<float>(x-SHORT_BARS_SECTION_WIDTH)/COLUMN_WIDTH_THIRD);
      }
      for (int x=SHORT_BARS_SECTION_WIDTH+LONG_BAR_COLUMN_WIDTH; x < WIDTH; x++)
      {
         buffer[y*WIDTH+x]=static_cast<unsigned char>(ColorName::GRAY01);
      }
   }
   dosmemput(buffer,WIDTH*HEIGHT,0xA0000UL); // copy the buffer to physical video memory at 0xA000:0000

   std::cin.get();
   return 0;
}
