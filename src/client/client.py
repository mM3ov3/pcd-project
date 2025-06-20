#!/usr/bin/env python3
def display_menu():
    print("=============================")
    print("        Video Options        ")
    print("=============================")
    print(" 1. Trim video")
    print(" 2. Resize video")
    print(" 3. Convert format")
    print(" 4. Extract audio")
    print(" 5. Extract video")
    print(" 6. Adjust brightness")
    print(" 7. Adjust contrast")
    print(" 8. Adjust saturation")
    print(" 9. Rotate video")
    print("10. Crop video")
    print("11. Add watermark")
    print("12. Add subtitles")
    print("13. Change playback speed")
    print("14. Reverse video")
    print("15. Extract frame")
    print("16. Create GIF")
    print("17. Denoise video")
    print("18. Stabilize video")
    print("19. Merge videos")
    print("20. Add audio track")
    print(" 0. Exit")
    print("=============================")

def generate_command(choice, input_file, output_file):
    match choice:
        case 1:  return f"ffmpeg -i {input_file} -ss 00:00:05 -t 00:00:10 -c copy {output_file}"
        case 2:  return f"ffmpeg -i {input_file} -vf scale=1280:720 {output_file}"
        case 3:  return f"ffmpeg -i {input_file} {output_file}"
        case 4:  return f"ffmpeg -i {input_file} -q:a 0 -map a {output_file}"
        case 5:  return f"ffmpeg -i {input_file} -an {output_file}"
        case 6:  return f"ffmpeg -i {input_file} -vf eq=brightness=0.06 {output_file}"
        case 7:  return f"ffmpeg -i {input_file} -vf eq=contrast=1.5 {output_file}"
        case 8:  return f"ffmpeg -i {input_file} -vf eq=saturation=2.0 {output_file}"
        case 9:  return f"ffmpeg -i {input_file} -vf transpose=1 {output_file}"
        case 10: return f"ffmpeg -i {input_file} -filter:v \"crop=300:300:100:100\" {output_file}"
        case 11: return f"ffmpeg -i {input_file} -i watermark.png -filter_complex \"overlay=10:10\" {output_file}"
        case 12: return f"ffmpeg -i {input_file} -vf subtitles=subs.srt {output_file}"
        case 13: return f"ffmpeg -i {input_file} -filter:v \"setpts=0.5*PTS\" {output_file}"
        case 14: return f"ffmpeg -i {input_file} -vf reverse -af areverse {output_file}"
        case 15: return f"ffmpeg -i {input_file} -ss 00:00:01.000 -vframes 1 {output_file}"
        case 16: return f"ffmpeg -i {input_file} -vf \"fps=10,scale=320:-1:flags=lanczos\" -t 5 {output_file}"
        case 17: return f"ffmpeg -i {input_file} -vf hqdn3d {output_file}"
        case 18: return f"ffmpeg -i {input_file} -vf vidstabdetect=shakiness=5:accuracy=15 -f null - && ffmpeg -i {input_file} -vf vidstabtransform=smoothing=30 {output_file}"
        case 19: return f"ffmpeg -i \"concat:file1.mp4|file2.mp4\" -c copy {output_file}"
        case 20: return f"ffmpeg -i {input_file} -i audio.mp3 -c:v copy -map 0:v:0 -map 1:a:0 {output_file}"
        case _:  return None

def main():
    print("=== Python Video Client ===")
    while True:
        display_menu()
        try:
            choice = int(input("Enter your choice: "))
        except ValueError:
            print("Invalid input. Try again.\n")
            continue

        if choice == 0:
            print("Exiting.")
            break

        if not (1 <= choice <= 20):
            print("Invalid choice. Try again.\n")
            continue

        input_file = input("Enter input file name: ").strip()
        output_file = input("Enter output file name: ").strip()

        command = generate_command(choice, input_file, output_file)
        if command:
            print(f"\nGenerated ffmpeg command:\n{command}\n")
        else:
            print("Failed to generate command.\n")

if __name__ == "__main__":
    main()
