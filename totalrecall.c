#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sqlite3.h>
#include <windows.h>
#include <direct.h>
#include <getopt.h>

#define VERSION "0.3"

void display_banner() {
    printf("\033[94m\n");
    printf("___________     __         .__ __________                     .__  .__   \n");
    printf("\\__    ___/____/  |______  |  |\\______   \\ ____   ____ _____  |  | |  |  \n");
    printf("  |    | /  _ \\   __\\__  \\ |  | |       _// __ \\_/ ___\\\\__  \\ |  | |  |  \n");
    printf("  |    |(  <_> )  |  / __ \\|  |_|    |   \\  ___/\\  \\___ / __ \\|  |_|  |__\n");
    printf("  |____| \\____/|__| (____  /____/____|_  /\\___  >\\___  >____  /____/____/\n");
    printf("                         \\/            \\/     \\/     \\/     \\/           \n");
    printf("v%s / Alexander Hagenah / @xaitax / ah@primepage.de\n", VERSION);
    printf("\033[0m\n");
}

void modify_permissions(const char *path) {
    char command[1024];
    snprintf(command, sizeof(command), "icacls \"%s\" /grant %%username%%:(OI)(CI)F /T /C /Q", path);
    if (system(command) == 0) {
        printf("\033[92m✅ Permissions modified for %s and all its subdirectories and files\033[0m\n", path);
    } else {
        printf("\033[91m❌ Failed to modify permissions for %s\033[0m\n", path);
    }
}

void create_directory(const char *path) {
    _mkdir(path);
}

void copy_file(const char *src, const char *dst) {
    CopyFile(src, dst, FALSE);
}

void copy_directory(const char *src, const char *dst) {
    char command[1024];
    snprintf(command, sizeof(command), "xcopy \"%s\" \"%s\" /E /I /Y", src, dst);
    system(command);
}

void rename_image_files(const char *image_store_path) {
    WIN32_FIND_DATA find_file_data;
    HANDLE h_find;
    char search_path[1024];

    snprintf(search_path, sizeof(search_path), "%s\\*.*", image_store_path);
    h_find = FindFirstFile(search_path, &find_file_data);

    if (h_find == INVALID_HANDLE_VALUE) {
        return;
    }

    do {
        if (!(find_file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
            char old_file_path[1024], new_file_path[1024];
            snprintf(old_file_path, sizeof(old_file_path), "%s\\%s", image_store_path, find_file_data.cFileName);
            snprintf(new_file_path, sizeof(new_file_path), "%s\\%s.jpg", image_store_path, find_file_data.cFileName);
            rename(old_file_path, new_file_path);
        }
    } while (FindNextFile(h_find, &find_file_data) != 0);

    FindClose(h_find);
}

void extract_data(const char *db_path, const char *extraction_folder, const char *from_date, const char *to_date, const char *search_term) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    char sql[1024];

    if (sqlite3_open(db_path, &db)) {
        fprintf(stderr, "Cannot open database: %s\n", sqlite3_errmsg(db));
        return;
    }

    char output_file_path[1024];
    snprintf(output_file_path, sizeof(output_file_path), "%s\\TotalRecall.txt", extraction_folder);
    FILE *output_file = fopen(output_file_path, "w");

    if (!output_file) {
        fprintf(stderr, "Could not open output file for writing.\n");
        sqlite3_close(db);
        return;
    }

    long long from_timestamp = 0, to_timestamp = 0;

    if (from_date) {
        struct tm tm;
        strptime(from_date, "%Y-%m-%d", &tm);
        from_timestamp = mktime(&tm) * 1000;
    }

    if (to_date) {
        struct tm tm;
        strptime(to_date, "%Y-%m-%d", &tm);
        to_timestamp = (mktime(&tm) + 86400) * 1000;
    }

    snprintf(sql, sizeof(sql), "SELECT WindowTitle, TimeStamp, ImageToken FROM WindowCapture WHERE (WindowTitle IS NOT NULL OR ImageToken IS NOT NULL)");
    sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);

    fprintf(output_file, "Captured Windows:\n");
    int captured_windows_count = 0;
    int images_taken_count = 0;

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *window_title = (const char *)sqlite3_column_text(stmt, 0);
        long long timestamp = sqlite3_column_int64(stmt, 1);
        const char *image_token = (const char *)sqlite3_column_text(stmt, 2);

        if ((from_timestamp == 0 || from_timestamp <= timestamp) && (to_timestamp == 0 || timestamp < to_timestamp)) {
            char readable_timestamp[20];
            time_t rawtime = timestamp / 1000;
            struct tm *timeinfo = localtime(&rawtime);
            strftime(readable_timestamp, sizeof(readable_timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);

            if (window_title) {
                fprintf(output_file, "[%s] %s\n", readable_timestamp, window_title);
                captured_windows_count++;
            }
            if (image_token) {
                fprintf(output_file, "[%s] %s\n", readable_timestamp, image_token);
                images_taken_count++;
            }
        }
    }

    fprintf(output_file, "\nImages Taken:\n");
    fprintf(output_file, "\nSearch Results:\n");

    if (search_term) {
        snprintf(sql, sizeof(sql), "SELECT c1, c2 FROM WindowCaptureTextIndex_content WHERE c1 LIKE ? OR c2 LIKE ?");
        sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
        sqlite3_bind_text(stmt, 1, search_term, -1, SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, search_term, -1, SQLITE_TRANSIENT);

        int search_results_count = 0;

        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char *c1 = (const char *)sqlite3_column_text(stmt, 0);
            const char *c2 = (const char *)sqlite3_column_text(stmt, 1);

            fprintf(output_file, "c1: %s, c2: %s\n", c1, c2);
            search_results_count++;
        }

        printf("🔍 Search results for '%s': %d\n", search_term, search_results_count);
    }

    fclose(output_file);
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    printf("🪟 Captured Windows: %d\n", captured_windows_count);
    printf("📸 Images Taken: %d\n", images_taken_count);
    printf("\n📄 Summary of the extraction is available in the file:\n");
    printf("\033[93m%s\\TotalRecall.txt\033[0m\n", extraction_folder);
    printf("\n📂 Full extraction folder path:\n");
    printf("\033[93m%s\033[0m\n", extraction_folder);
}

int main(int argc, char *argv[]) {
    char *from_date = NULL;
    char *to_date = NULL;
    char *search_term = NULL;

    int option;
    while ((option = getopt(argc, argv, "f:t:s:")) != -1) {
        switch (option) {
            case 'f':
                from_date = optarg;
                break;
            case 't':
                to_date = optarg;
                break;
            case 's':
                search_term = optarg;
                break;
            default:
                fprintf(stderr, "Usage: %s [-f from_date] [-t to_date] [-s search_term]\n", argv[0]);
                return EXIT_FAILURE;
        }
    }

    display_banner();
    char username[256];
    DWORD username_len = sizeof(username);
    GetUserName(username, &username_len);

    char base_path[512];
    snprintf(base_path, sizeof(base_path), "C:\\Users\\%s\\AppData\\Local\\CoreAIPlatform.00\\UKP", username);

    if (GetFileAttributes(base_path) == INVALID_FILE_ATTRIBUTES) {
        printf("🚫 Base path does not exist.\n");
        return EXIT_FAILURE;
    }

    modify_permissions(base_path);

    WIN32_FIND_DATA find_file_data;
    HANDLE h_find = FindFirstFile(strcat(base_path, "\\*"), &find_file_data);

    char guid_folder[512] = {0};
    if (h_find != INVALID_HANDLE_VALUE) {
        do {
            if (find_file_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                snprintf(guid_folder, sizeof(guid_folder), "%s\\%s", base_path, find_file_data.cFileName);
                break;
            }
        } while (FindNextFile(h_find, &find_file_data) != 0);
        FindClose(h_find);
    }

    if (strlen(guid_folder) == 0) {
        printf("🚫 Could not find the GUID folder.\n");
        return EXIT_FAILURE;
    }

    printf("📁 Recall folder found: %s\n", guid_folder);

    char db_path[512];
    snprintf(db_path, sizeof(db_path), "%s\\ukg.db", guid_folder);
    char image_store_path[512];
    snprintf(image_store_path, sizeof(image_store_path), "%s\\ImageStore", guid_folder);

    if (GetFileAttributes(db_path) == INVALID_FILE_ATTRIBUTES || GetFileAttributes(image_store_path) == INVALID_FILE_ATTRIBUTES) {
        printf("🚫 Windows Recall feature not found. Nothing to extract.\n");
        return EXIT_FAILURE;
    }

    printf("🟢 Windows Recall feature found. Do you want to proceed with the extraction? (yes/no): ");
    char proceed[4];
    scanf("%3s", proceed);
    if (strcmp(proceed, "yes") != 0) {
        printf("⚠️ Extraction aborted.\n");
        return EXIT_FAILURE;
    }

    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d-%H-%M", tm_info);

    char extraction_folder[512];
    snprintf(extraction_folder, sizeof(extraction_folder), "%s\\%s_Recall_Extraction", _getcwd(NULL, 0), timestamp);
    create_directory(extraction_folder);

    printf("📂 Creating extraction folder: %s\n", extraction_folder);

    char dst_db_path[512];
    snprintf(dst_db_path, sizeof(dst_db_path), "%s\\ukg.db", extraction_folder);
    copy_file(db_path, dst_db_path);

    char dst_image_store_path[512];
    snprintf(dst_image_store_path, sizeof(dst_image_store_path), "%s\\ImageStore", extraction_folder);
    copy_directory(image_store_path, dst_image_store_path);

    rename_image_files(dst_image_store_path);

    extract_data(dst_db_path, extraction_folder, from_date, to_date, search_term);

    return 0;
}
