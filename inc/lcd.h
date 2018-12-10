#ifndef LCD_H
#define LCD_H

void LCD_Init();

void LCD_Clear();

void LCD_MoveCursor(uint8_t row, uint8_t col);

void LCD_WriteString(char *str, uint8_t len);

#endif
