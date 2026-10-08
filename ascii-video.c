#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/ioctl.h>

#define FPS 60

int HEIGHT;
int WIDTH;
const char *ASCII_CHARS = " .:-=+*#%@";

// helper function to get current time in milliseconds
long long get_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (tv.tv_sec * 1000LL) + (tv.tv_usec / 1000);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Usage: %s <video_file.mp4>\n", argv[0]);
        return 1;
    }
    struct winsize ws;

    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1) {
	perror("ioctl");
	return 1;
    }

    WIDTH = ws.ws_col;
    HEIGHT = ws.ws_row;

    char command[512];
    // command instructs FFmpeg to decode the file, convert to grayscale, scale it to our terminal size, and stream raw bytes to standard output
    snprintf(command, sizeof(command), 
             "ffmpeg -i \"%s\" -f rawvideo -pix_fmt gray -s %dx%d -loglevel error -", 
             argv[1], WIDTH, HEIGHT);

    // open a pipe to read the output of the FFmpeg command
    FILE *pipe = popen(command, "r");
    if (!pipe) {
        printf("Error: Could not execute FFmpeg. Make sure it is installed and in your PATH.\n");
        return 1;
    }

    int frame_size = WIDTH * HEIGHT;
    unsigned char *frame_data = malloc(frame_size);
    
    // calculate output buffer size:  pixels + newlines + null terminator + space for the ANSI cursor jump code
    int out_size = (WIDTH + 1) * HEIGHT + 20; 
    char *out_buffer = malloc(out_size);

    int frame_delay_ms = 1000 / FPS;
    int num_chars = strlen(ASCII_CHARS);

    // clear the terminal screen entirely once at the start
    printf("\033[2J");

    // read exactly one frame of bytes at a time until the pipe runs out
    while (fread(frame_data, 1, frame_size, pipe) == frame_size) {
        long long start_time = get_time_ms();
        int out_idx = 0;

        // ANSI escape code: move cursor to home position (Row 1, Column 1) this overwrites the old frame instead of clearing the screen (prevents flicker)
        out_idx += snprintf(&out_buffer[out_idx], 10, "\033[H");

        // map each pixel byte to an ASCII character
        for (int y = 0; y < HEIGHT; y++) {
            for (int x = 0; x < WIDTH; x++) {
                unsigned char pixel = frame_data[y * WIDTH + x];
                // Math: Scale 0-255 value into the index range of our ASCII array
                int ascii_idx = (pixel * (num_chars - 1)) / 255;
                out_buffer[out_idx++] = ASCII_CHARS[ascii_idx];
            }
            out_buffer[out_idx++] = '\n'; // End of row
        }
        out_buffer[out_idx] = '\0';

        // blast the entire frame buffer to the terminal at once
        fputs(out_buffer, stdout);
        fflush(stdout);

        // framerate timing logic
        long long end_time = get_time_ms();
        long long elapsed = end_time - start_time;
        long long sleep_time = frame_delay_ms - elapsed;

        // sleep for the remainder of the frame's time budget
        if (sleep_time > 0) {
            usleep(sleep_time * 1000); 
        }
    }

    free(frame_data);
    free(out_buffer);
    pclose(pipe);
    
    // reset terminal coloring and cursor
    printf("\033[0m\nVideo Finished.\n");
    return 0;
}

