#ifndef FFMPEG_COMMANDS_H
#define FFMPEG_COMMANDS_H

void build_trim_command(const char *input, const char *output, char *cmd, size_t size);
void build_resize_command(const char *input, const char *output, char *cmd, size_t size);
void build_convert_command(const char *input, const char *output, char *cmd, size_t size);
void build_extract_audio_command(const char *input, const char *output, char *cmd, size_t size);
void build_extract_video_command(const char *input, const char *output, char *cmd, size_t size);
void build_adjust_brightness_command(const char *input, const char *output, char *cmd, size_t size);
void build_adjust_contrast_command(const char *input, const char *output, char *cmd, size_t size);
void build_adjust_saturation_command(const char *input, const char *output, char *cmd, size_t size);
void build_rotate_command(const char *input, const char *output, char *cmd, size_t size);
void build_crop_command(const char *input, const char *output, char *cmd, size_t size);
void build_add_watermark_command(const char *input, const char *output, char *cmd, size_t size);
void build_add_subtitles_command(const char *input, const char *output, char *cmd, size_t size);
void build_change_speed_command(const char *input, const char *output, char *cmd, size_t size);
void build_reverse_command(const char *input, const char *output, char *cmd, size_t size);
void build_extract_frame_command(const char *input, const char *output, char *cmd, size_t size);
void build_create_gif_command(const char *input, const char *output, char *cmd, size_t size);
void build_denoise_command(const char *input, const char *output, char *cmd, size_t size);
void build_stabilize_command(const char *input, const char *output, char *cmd, size_t size);
void build_merge_command(const char *input, const char *output, char *cmd, size_t size);
void build_add_audio_command(const char *input, const char *output, char *cmd, size_t size);

#endif // FFMPEG_COMMANDS_H