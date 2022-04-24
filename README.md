# Morse-Code-Decoder-and-Encoder
A simple morse-code decoder/encoder project I wrote for the 6th-semester Linux for Embedded Systems course at Computer Science program at Warsaw University of
Technology. It was written to be used on Rasberry Pi 4B using `libgpiod` library, developed on a QEMU emulated aarch64 linux kernel built with BuildRoot and tested  
on a real RPi4B. There's still room for improvement but overall it works.

Basically, you enter a morse code pattern using the RPi4B's button and it decodes the input from the relevant GPIO pin to letters, or you enter a text via the terminal and it encodes the text to morse code and sends the output to the designated GPIO pin.
