#include <stdio.h>
#include "menu.h"

void display_main_menu() {
    printf("\n===== Video Editing Menu =====\n");
    printf(" 1. Trim video\n");
    printf(" 2. Resize video\n");
    printf(" 3. Convert format\n");
    printf(" 4. Extract audio\n");
    printf(" 5. Extract video\n");
    printf(" 6. Adjust brightness\n");
    printf(" 7. Adjust contrast\n");
    printf(" 8. Adjust saturation\n");
    printf(" 9. Rotate video\n");
    printf("10. Crop video\n");
    printf("11. Add watermark\n");
    printf("12. Add subtitles\n");
    printf("13. Change playback speed\n");
    printf("14. Reverse video\n");
    printf("15. Extract frame\n");
    printf("16. Create GIF\n");
    printf("17. Denoise video\n");
    printf("18. Stabilize video\n");
    printf("19. Merge videos\n");
    printf("20. Add audio track\n");
    printf(" 0. Exit\n");
    printf("=============================\n");
    printf("Enter your choice: ");
}

int get_menu_choice() {
    int choice;
    scanf("%d", &choice);
    return choice;
}