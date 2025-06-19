#include <stdio.h>
#include <string.h>
#include "ffmpeg_commands.h"

void build_trim_command(const char *input, const char *output, char *cmd, size_t size) {
    char start_time[16], end_time[16];
    printf("Enter start time (HH:MM:SS): ");
    scanf("%15s", start_time);
    printf("Enter end time (HH:MM:SS): ");
    scanf("%15s", end_time);
    
    snprintf(cmd, size, "ffmpeg -i %s -ss %s -to %s -c copy %s", 
             input, start_time, end_time, output);
}

void build_resize_command(const char *input, const char *output, char *cmd, size_t size) {
    int width, height;
    printf("Enter width: ");
    scanf("%d", &width);
    printf("Enter height: ");
    scanf("%d", &height);
    
    snprintf(cmd, size, "ffmpeg -i %s -vf scale=%d:%d %s", 
             input, width, height, output);
}

void build_convert_command(const char *input, const char *output, char *cmd, size_t size) {
    snprintf(cmd, size, "ffmpeg -i %s %s", input, output);
}

void build_extract_audio_command(const char *input, const char *output, char *cmd, size_t size) {
    snprintf(cmd, size, "ffmpeg -i %s -vn -acodec copy %s", input, output);
}

void build_extract_video_command(const char *input, const char *output, char *cmd, size_t size) {
    snprintf(cmd, size, "ffmpeg -i %s -an -vcodec copy %s", input, output);
}

void build_adjust_brightness_command(const char *input, const char *output, char *cmd, size_t size) {
    float value;
    printf("Enter brightness value (-1.0 to 1.0): ");
    scanf("%f", &value);
    
    snprintf(cmd, size, "ffmpeg -i %s -vf eq=brightness=%.2f %s", 
             input, value, output);
}

void build_adjust_contrast_command(const char *input, const char *output, char *cmd, size_t size) {
    float value;
    printf("Enter contrast value (-2.0 to 2.0): ");
    scanf("%f", &value);
    
    snprintf(cmd, size, "ffmpeg -i %s -vf eq=contrast=%.2f %s", 
             input, value, output);
}

void build_adjust_saturation_command(const char *input, const char *output, char *cmd, size_t size) {
    float value;
    printf("Enter saturation value (0.0 to 3.0): ");
    scanf("%f", &value);
    
    snprintf(cmd, size, "ffmpeg -i %s -vf eq=saturation=%.2f %s", 
             input, value, output);
}

void build_rotate_command(const char *input, const char *output, char *cmd, size_t size) {
    int angle;
    printf("Enter rotation angle (90, 180, 270): ");
    scanf("%d", &angle);
    
    const char *transpose;
    switch (angle) {
        case 90: transpose = "transpose=1"; break;
        case 180: transpose = "transpose=1,transpose=1"; break;
        case 270: transpose = "transpose=2"; break;
        default: transpose = "";
    }
    
    snprintf(cmd, size, "ffmpeg -i %s -vf \"%s\" %s", input, transpose, output);
}

void build_crop_command(const char *input, const char *output, char *cmd, size_t size) {
    int x, y, width, height;
    printf("Enter crop parameters (x y width height): ");
    scanf("%d %d %d %d", &x, &y, &width, &height);
    
    snprintf(cmd, size, "ffmpeg -i %s -filter:v \"crop=%d:%d:%d:%d\" %s", 
             input, width, height, x, y, output);
}

void build_add_watermark_command(const char *input, const char *output, char *cmd, size_t size) {
    char watermark[256];
    int x, y;
    printf("Enter watermark image: ");
    scanf("%255s", watermark);
    printf("Enter position (x y): ");
    scanf("%d %d", &x, &y);
    
    snprintf(cmd, size, "ffmpeg -i %s -i %s -filter_complex \"overlay=%d:%d\" %s", 
             input, watermark, x, y, output);
}

void build_add_subtitles_command(const char *input, const char *output, char *cmd, size_t size) {
    char subtitles[256];
    printf("Enter subtitles file: ");
    scanf("%255s", subtitles);
    
    snprintf(cmd, size, "ffmpeg -i %s -vf subtitles=%s %s", 
             input, subtitles, output);
}

void build_change_speed_command(const char *input, const char *output, char *cmd, size_t size) {
    float speed;
    printf("Enter speed factor (0.5 for 50%% slower, 2.0 for 2x faster): ");
    scanf("%f", &speed);
    
    snprintf(cmd, size, "ffmpeg -i %s -filter:v \"setpts=%.2f*PTS\" %s", 
             input, 1/speed, output);
}

void build_reverse_command(const char *input, const char *output, char *cmd, size_t size) {
    snprintf(cmd, size, "ffmpeg -i %s -vf reverse -af areverse %s", input, output);
}

void build_extract_frame_command(const char *input, const char *output, char *cmd, size_t size) {
    char timestamp[16];
    printf("Enter timestamp (HH:MM:SS): ");
    scanf("%15s", timestamp);
    
    snprintf(cmd, size, "ffmpeg -i %s -ss %s -vframes 1 %s", input, timestamp, output);
}

void build_create_gif_command(const char *input, const char *output, char *cmd, size_t size) {
    char start[16], end[16];
    int width, fps;
    printf("Enter start time (HH:MM:SS): ");
    scanf("%15s", start);
    printf("Enter end time (HH:MM:SS): ");
    scanf("%15s", end);
    printf("Enter width: ");
    scanf("%d", &width);
    printf("Enter FPS: ");
    scanf("%d", &fps);
    
    snprintf(cmd, size, "ffmpeg -i %s -ss %s -to %s -vf \"fps=%d,scale=%d:-1:flags=lanczos,split[s0][s1];[s0]palettegen[p];[s1][p]paletteuse\" -loop 0 %s", 
             input, start, end, fps, width, output);
}

void build_denoise_command(const char *input, const char *output, char *cmd, size_t size) {
    float strength;
    printf("Enter denoise strength (1.0-30.0): ");
    scanf("%f", &strength);
    
    snprintf(cmd, size, "ffmpeg -i %s -vf hqdn3d=%.1f:%.1f:%.1f:%.1f %s", 
             input, strength, strength, strength/2, strength/2, output);
}

void build_stabilize_command(const char *input, const char *output, char *cmd, size_t size) {
    snprintf(cmd, size, "ffmpeg -i %s -vf vidstabdetect=shakiness=10:accuracy=15:result=transform_vectors.trf -f null - && "
             "ffmpeg -i %s -vf vidstabtransform=input=transform_vectors.trf:zoom=0:smoothing=10 %s", 
             input, input, output);
}

void build_merge_command(const char *input, const char *output, char *cmd, size_t size) {
    char second_file[256];
    printf("Enter second video file: ");
    scanf("%255s", second_file);
    
    snprintf(cmd, size, "ffmpeg -i %s -i %s -filter_complex \"concat=n=2:v=1:a=1\" %s", 
             input, second_file, output);
}

void build_add_audio_command(const char *input, const char *output, char *cmd, size_t size) {
    char audio_file[256];
    printf("Enter audio file: ");
    scanf("%255s", audio_file);
    
    snprintf(cmd, size, "ffmpeg -i %s -i %s -c:v copy -map 0:v:0 -map 1:a:0 -shortest %s", 
             input, audio_file, output);
}