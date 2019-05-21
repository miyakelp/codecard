/*
  serialBmp.h
  
*/


uint16_t serialRead16()
{
  while (Serial.available() < 2)
    ;
  // BMP data is stored little-endian, same as Arduino.
  uint16_t result;
  ((uint8_t *)&result)[0] = Serial.read(); // LSB
  ((uint8_t *)&result)[1] = Serial.read(); // MSB
  return result;
}

uint32_t serialRead32()
{
  while (Serial.available() < 4)
    ;
  // BMP data is stored little-endian, same as Arduino.
  uint32_t result;
  ((uint8_t *)&result)[0] = Serial.read(); // LSB
  ((uint8_t *)&result)[1] = Serial.read();
  ((uint8_t *)&result)[2] = Serial.read();
  ((uint8_t *)&result)[3] = Serial.read(); // MSB
  return result;
}

int16_t serialSkip(int16_t size)
{
  int i;
  for (i = 0; i < size; i++)
    while (Serial.available() < 1)
      Serial.read();
  return i;
}

void displayImageFromSerial(int16_t x, int16_t y, bool with_color) {
  bool valid = false; // valid format to be handled
  bool flip = true; // bitmap is stored bottom-to-top
  uint32_t startTime = millis();  

  display.setRotation(0);
  Serial.print("   Display width : "); Serial.println(display.width());
  Serial.print("   Display height : "); Serial.println(display.height());
  
  // Parse BMP header
  uint16_t t = serialRead16();
  if (t == 0x4D42) // BMP signature
  {
    uint32_t fileSize = serialRead32();
    uint32_t creatorBytes = serialRead32();
    uint32_t imageOffset = serialRead32(); // Start of image data
    uint32_t headerSize = serialRead32();
    uint32_t width  = serialRead32();
    uint32_t height = serialRead32();
    uint16_t planes = serialRead16();
    uint16_t depth = serialRead16(); // bits per pixel
    uint32_t format = serialRead32();
    uint32_t bytes_read = 7 * 4 + 3 * 2; // read so far
    if ((planes == 1) && ((format == 0) || (format == 3))) // uncompressed is handled, 565 also
    {
      Serial.print("  File size: "); Serial.println(fileSize);
      Serial.print("  Image Offset: "); Serial.println(imageOffset);
      Serial.print("  Header size: "); Serial.println(headerSize);
      Serial.print("  Bit Depth: "); Serial.println(depth);
      Serial.print("  Image size: ");
      Serial.print(width);
      Serial.print('x');
      Serial.println(height);
      // BMP rows are padded (if needed) to 4-byte boundary
      uint32_t rowSize = (width * depth / 8 + 3) & ~3;
      if (height < 0)
      {
        height = -height;
        flip = false;
      }
      uint16_t w = width;
      uint16_t h = height;
      if ((x + w - 1) >= display.width())  w = display.width()  - x;
      if ((y + h - 1) >= display.height()) h = display.height() - y;

      // works for small icons
      y = (display.height() - height) + y;

//      Serial.print("x: "); Serial.println(x);
//      Serial.print("y: "); Serial.println(y);
//      Serial.print("display.width(): "); Serial.println(display.width());
//      Serial.print("display.height(): "); Serial.println(display.height());
    
      if (w < max_row_width) // handle with direct drawing
      {
        valid = true;
        uint8_t bitmask = 0xFF;
        uint8_t bitshift = 8 - depth;
        uint16_t red, green, blue;
        bool whitish, colored;
        if (depth == 1) with_color = false;
        if (depth <= 8)
        {
          if (depth < 8) bitmask >>= depth;
          bytes_read += serialSkip(54 - bytes_read); //palette is always @ 54
          for (uint16_t pn = 0; pn < (1 << depth); pn++)
          {
            while (Serial.available() < 4)
              ;
            blue  = Serial.read();
            green = Serial.read();
            red   = Serial.read();
            Serial.read();
            bytes_read += 4;
            whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80)) : ((red + green + blue) > 3 * 0x80); // whitish
            colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
            if (0 == pn % 8) mono_palette_buffer[pn / 8] = 0;
            mono_palette_buffer[pn / 8] |= whitish << pn % 8;
            if (0 == pn % 8) color_palette_buffer[pn / 8] = 0;
            color_palette_buffer[pn / 8] |= colored << pn % 8;
            //Serial.print("0x00"); Serial.print(red, HEX); Serial.print(green, HEX); Serial.print(blue, HEX);
            //Serial.print(" : "); Serial.print(whitish); Serial.print(", "); Serial.println(colored);
          }
        }
        //display.writeScreenBuffer();
        uint32_t rowPosition = flip ? imageOffset + (height - h) * rowSize : imageOffset;
        //Serial.print("skip "); Serial.println(rowPosition - bytes_read);
        bytes_read += serialSkip(rowPosition - bytes_read);
        for (uint16_t row = 0; row < h; row++, rowPosition += rowSize) // for each line
        {
          delay(1); // yield() to avoid WDT
          uint32_t in_remain = rowSize;
          uint32_t in_idx = 0;
          uint32_t in_bytes = 0;
          uint8_t in_byte = 0; // for depth <= 8
          uint8_t in_bits = 0; // for depth <= 8
          uint8_t out_byte = 0;
          uint8_t out_color_byte = 0;
          uint32_t out_idx = 0;
          for (uint16_t col = 0; col < w; col++) // for each pixel
          {
            yield();
            // Time to read more pixel data?
            if (in_idx >= in_bytes) // ok, exact match for 24bit also (size IS multiple of 3)
            {
              uint32_t get = in_remain > sizeof(input_buffer) ? sizeof(input_buffer) : in_remain;
              //if (get > client.available()) delay(200); // does improve? yes, if >= 200
              // there seems an issue with long downloads on ESP8266
              uint32_t got = 0;
              for (got = 0; got < get; got++) {
                while (Serial.available() < 1)
                  ;
                input_buffer[got] = Serial.read();
              }
              while ((got < get))
              {
                //Serial.print("got "); Serial.print(got); Serial.print(" < "); Serial.print(get); Serial.print(" @ "); Serial.println(bytes_read);
                //if ((get - got) > client.available()) delay(200); // does improve? yes, if >= 200
                // there seems an issue with long downloads on ESP8266
                uint32_t gotmore = 0;
                for (gotmore = 0; gotmore < get - got; gotmore++) {
                  while (Serial.available() < 1)
                    ;
                  input_buffer[got + gotmore] = Serial.read();
                }
                got += gotmore;
                // Serial.println(gotmore);
              }
              //Serial.print("got "); Serial.print(got); Serial.print(" < "); Serial.print(get); Serial.print(" @ "); Serial.print(bytes_read); Serial.print(" of "); Serial.println(fileSize);
              //Serial.print("  "); Serial.print(bytes_read); Serial.print(" of "); Serial.print(fileSize); Serial.println(" bytes");
              in_bytes = got;
              in_remain -= got;
              bytes_read += got;
            }

            switch (depth)
            {
              case 24:
                blue = input_buffer[in_idx++];
                green = input_buffer[in_idx++];
                red = input_buffer[in_idx++];
                whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80)) : ((red + green + blue) > 3 * 0x80); // whitish
                colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
                break;
              case 16:
                {
                  uint8_t lsb = input_buffer[in_idx++];
                  uint8_t msb = input_buffer[in_idx++];
                  if (format == 0) // 555
                  {
                    blue  = (lsb & 0x1F) << 3;
                    green = ((msb & 0x03) << 6) | ((lsb & 0xE0) >> 2);
                    red   = (msb & 0x7C) << 1;
                  }
                  else // 565
                  {
                    blue  = (lsb & 0x1F) << 3;
                    green = ((msb & 0x07) << 5) | ((lsb & 0xE0) >> 3);
                    red   = (msb & 0xF8);
                  }
                  whitish = with_color ? ((red > 0x80) && (green > 0x80) && (blue > 0x80)) : ((red + green + blue) > 3 * 0x80); // whitish
                  colored = (red > 0xF0) || ((green > 0xF0) && (blue > 0xF0)); // reddish or yellowish?
                }
                break;
              case 1:
              case 4:
              case 8:
                {
                  if (0 == in_bits)
                  {
                    in_byte = input_buffer[in_idx++];
                    in_bits = 8;
                  }
                  uint16_t pn = (in_byte >> bitshift) & bitmask;
                  whitish = mono_palette_buffer[pn / 8] & (0x1 << pn % 8);
                  colored = color_palette_buffer[pn / 8] & (0x1 << pn % 8);
                  in_byte <<= depth;
                  in_bits -= depth;
                }
                break;
            }
            if (whitish)
            {
              out_byte |= 0x80 >> col % 8; // not black
              out_color_byte |= 0x80 >> col % 8; // not colored
            }
            else if (colored && with_color)
            {
              out_byte |= 0x80 >> col % 8; // not black
            }
            else
            {
              out_color_byte |= 0x80 >> col % 8; // not colored
            }
            if (7 == col % 8)
            {
              output_row_color_buffer[out_idx] = out_color_byte;
              output_row_mono_buffer[out_idx++] = out_byte;
              out_byte = 0;
              out_color_byte = 0;
            }
          } // end pixel
          int16_t yrow = y + (flip ? h - row - 1 : row);
          display.writeImage(output_row_mono_buffer, output_row_color_buffer, x, yrow, w, 1);
        } // end line
        Serial.print("  downloaded in "); Serial.print(millis() - startTime); Serial.println(" ms");
        Serial.print("  bytes read "); Serial.println(bytes_read);
        display.refresh(true);
        display.setRotation(3);
      }
    }
  }
  if (!valid)
  {
    Serial.println("bitmap format not handled.");
  }
}
