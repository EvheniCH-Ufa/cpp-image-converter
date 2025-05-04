#include "bmp_image.h"
#include "img_lib.h"
#include "pack_defines.h"

#include <array>
#include <fstream>
#include <string_view>

using namespace std;

namespace img_lib {

//    Заголовок файла, 14 байт.
    PACKED_STRUCT_BEGIN BitmapFileHeader
    {
        char type[2] = {'B', 'M'};  // Подпись — 2 байта.Символы BM.
        uint32_t size = 0;          // Суммарный размер заголовка и данных — 4 байта, беззнаковое целое. Размер данных определяется как отступ, умноженный на высоту изображения.
        uint32_t reserved = 0;      // Зарезервированное пространство — 4 байта, заполненные нулями. 
        uint32_t off_bits = 54;    // Отступ данных от начала файла — 4 байта, беззнаковое целое.Он равен размеру двух частей заголовка. 14+40
    }
    PACKED_STRUCT_END

//  Информационный заголовок, 40 байт.
    PACKED_STRUCT_BEGIN BitmapInfoHeader{
        uint32_t hdr_size = 40;                 // Размер заголовка — 4 байта, беззнаковое целое.Учитывается только размер второй части заголовка.
        int32_t width = 0;                      // Ширина изображения в пикселях — 4 байта, знаковое целое.
        int32_t height = 0;                     // Высота изображения в пикселях — 4 байта, знаковое целое.
        uint16_t planes = 1;                    // Количество плоскостей — 2 байта, беззнаковое целое.В нашем случае всегда 1 — одна RGB плоскость.
        uint16_t bits_per_pixel = 24;           // Количество бит на пиксель — 2 байта, беззнаковое целое.В нашем случае всегда 24.
        uint32_t compression = 0;               // Тип сжатия — 4 байта, беззнаковое целое.В нашем случае всегда 0 — отсутствие сжатия.
        uint32_t size_image = 0;                // Количество байт в данных — 4 байта, беззнаковое целое.Произведение отступа на высоту.
        int32_t x_pels_per_meter = 11811;       // Горизонтальное разрешение, пикселей на метр — 4 байта, знаковое целое.Нужно записать 11811, что примерно соответствует 300 DPI.
        int32_t y_pels_per_meter = 11811;       // Вертикальное разрешение, пикселей на метр — 4 байта, знаковое целое.Нужно записать 11811, что примерно соответствует 300 DPI.
        uint32_t colors_used = 0;               // Количество использованных цветов — 4 байта, знаковое целое.Нужно записать 0 — значение не определено.
        uint32_t colors_important = 0x1000000;  // Количество значимых цветов — 4 байта, знаковое целое.Нужно записать 0x1000000.
    }
    PACKED_STRUCT_END

    // функция вычисления отступа по ширине
    static int GetBMPStride(int w) {
        return 4 * ((w * 3 + 3) / 4);
    }

    bool SaveBMP(const Path& file, const Image& image)
    {
        ofstream out(file, ios::binary);
        if (!out)
        {
            return false;
        }

        const int w = image.GetWidth();
        const int h = image.GetHeight();
        const int stride = GetBMPStride(w); // вычисление отступа по ширине

        BitmapFileHeader file_header;
        file_header.size = 54 + stride * h; // размер файла = заголовки + данные

        // 1-й заг-к
        BitmapInfoHeader info_header;
        info_header.width = w;
        info_header.height = h;
        info_header.size_image = stride * h; 

        //пишем в файл два заг-ка
        out.write(reinterpret_cast<const char*>(&file_header), sizeof(file_header));
        out.write(reinterpret_cast<const char*>(&info_header), sizeof(info_header));

        //а теперь тело
        vector<char> buffer(stride); // Буфер для одной строки изображ-я
        for (int y = h - 1; y >= 0; --y) // берем строки изображения снизу вверх, т.к. BMP
        {
            const Color* line = image.GetLine(y);
            for (int x = 0; x < w; ++x)
            {
            // конвертируем пиксели из RGB в BGR и пишем в буфер
                buffer[x * 3 + 0] = static_cast<char>(line[x].b);
                buffer[x * 3 + 1] = static_cast<char>(line[x].g);
                buffer[x * 3 + 2] = static_cast<char>(line[x].r);
            }
            out.write(buffer.data(), stride); // собсвенно сама запись в файл
        }
        return out.good(); // это типа файл записан - состояние потока
    }

    Image LoadBMP(const Path& file) {
        ifstream in(file, ios::binary);
        if (!in) { 
            return {}; //при ошибке открытия файла
        }

        // заг-ки
        BitmapFileHeader file_header;
        BitmapInfoHeader info_header;

        // Читаем их
        in.read(reinterpret_cast<char*>(&file_header), sizeof(file_header));
        in.read(reinterpret_cast<char*>(&info_header), sizeof(info_header));

        // если тип не BM, то пусто
        if (file_header.type[0] != 'B' || file_header.type[1] != 'M')
        {
            return {};
        }

/*        if (info_header.bits_per_pixel != 24 || info_header.compression != 0)
        {
            return {};
        }*/

        const int w = info_header.width;
        const int h = info_header.height;
        const int stride = GetBMPStride(w); //вычисл р-р стр

        Image result(w, h, Color::Black());
        vector<char> buffer(stride); // без буферов... нельзя (не примут)

   //     in.seekg(file_header.off_bits);

        // пишем строки снизу вверх
        for (int y = h - 1; y >= 0; --y)
        {
            in.read(buffer.data(), stride);
            if (!in) {
                return {};
            }

            // с нижних строк
            Color* line = result.GetLine(y);
            // конвертируем пиксели из BGR в RGB
            for (int x = 0; x < w; ++x)
            {
                line[x].b = static_cast<byte>(buffer[x * 3 + 0]);
                line[x].g = static_cast<byte>(buffer[x * 3 + 1]);
                line[x].r = static_cast<byte>(buffer[x * 3 + 2]);
                line[x].a = byte{ 255 };
            }
        }
        return result;
    }
}  // namespace img_lib